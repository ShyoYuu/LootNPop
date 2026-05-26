# 초기화 시퀀스 기술 설계

## 1. 개요

LootNPop의 월드는 즉시 플레이 가능한 상태가 아님. 구형 레벨 인스턴스 스폰 → 표면 캐시 베이킹 → Mass 엔티티 스폰의 3단계가 완료되어야 게임플레이 시작.

**핵심 원칙:**
- 서버(`ALNPGameMode`)가 단계 진행을 결정하고, `ALNPGameState::ServerPhase`를 통해 클라이언트에게 복제.
- 클라이언트는 각 OnRep 콜백에서 자신의 로컬 작업을 수행.
- 플레이어 폰 스폰은 서버 초기화가 완전히 끝날 때까지 차단.

---

## 2. 초기화 단계 (ELNPInitPhase)

| 단계 | 열거값 | 담당 클래스 |
|:---|:---|:---|
| 옥탄트 월드 생성 | `WorldGeneration` | `ULNPOctantSpawnSubsystem` |
| 표면 캐시 베이킹 | `SurfaceBaking` | `ULNPSurfaceCacheSubsystem` |
| Mass 엔티티 스폰 | `EntitySpawning` | `ULNPMassSpawnSubsystem` |
| 초기화 완료 | `Complete` | — |

---

## 3. 서버 초기화 흐름

`ALNPGameMode`는 서버 전용 클래스이므로, 아래 흐름은 서버에서만 실행.

```
ALNPGameMode::BeginPlay()
│
├─ OctantGenSeed 확정 (config가 0이면 FMath::Rand()로 생성)
├─ GameState->OctantGenSeed = seed  ──────────────── (복제됨)
├─ GameState->ServerPhase = WorldGeneration  ──────── (복제됨)
└─ OctantSpawnSubsystem::StartWorldGeneration()
        │ Tick()마다 8개 레벨 인스턴스 로드 완료 감시
        ▼
ALNPGameMode::OnWorldGenerationComplete()
├─ GameState->ServerPhase = SurfaceBaking  ─────────── (복제됨)
└─ SurfaceCacheSubsystem::BeginBaking()
        │ 전체 샘플을 AsyncLineTraceByChannel로 일괄 발사 (물리 스레드 처리)
        │ 결과는 FTraceDelegate 콜백(OnAsyncTraceComplete)으로 수집
        │ 모든 콜백 완료 시 OnBakingComplete 브로드캐스트
        ▼
ALNPGameMode::OnSurfaceBakingComplete()
├─ GameState->ServerPhase = EntitySpawning  ────────── (복제됨)
└─ MassSpawnSubsystem::BeginSpawning()
        │ EnqueueSpawnProject: SurfaceCache 스냅샷(TSharedPtr 공유) + TaskGraph 백그라운드에서 위치 계산
        │ 큐 빌드 완료 후 게임스레드에서 SpawnQueue 조립
        │ 이후 Tick()마다 MaxSpawnsPerFrame씩 Mass 엔티티 스폰
        ▼
ALNPGameMode::OnEntitySpawningComplete()
├─ GameState->ServerPhase = Complete  ──────────────── (복제됨)
├─ bServerInitComplete = true
└─ PendingPlayers 순회 → Super::RestartPlayer() (폰 스폰 해제)
```

---

## 4. 클라이언트 초기화 흐름

클라이언트는 GameMode를 갖지 않음. GameState의 OnRep 콜백으로 각 단계에 반응.

### 4.1 월드 생성

```
ALNPGameState::OnRep_OctantGenSeed()
└─ [!bGenerationComplete && !IsTickable() 가드 — 이미 진행/완료 시 중복 호출 방지]
   ├─ OctantSub->OnWorldGenerationFinished에 OnClientWorldGenerationFinished 구독
   └─ OctantSpawnSubsystem::StartWorldGeneration()  (서버와 동일한 seed로 결정론적 스폰)
           │ 로드 완료 시
           ▼
ALNPGameState::OnClientWorldGenerationFinished()
└─ [ServerPhase >= SurfaceBaking 조건 확인 후]
   └─ TryBeginClientBaking()  → OctantSub->bGenerationComplete이면 BeginBaking() 호출
```

### 4.2 표면 캐시 베이킹

```
ALNPGameState::OnRep_ServerPhase()  (SurfaceBaking 수신 시)
└─ TryBeginClientBaking()  → OctantSub->bGenerationComplete이면 BeginBaking() 호출
```

> **레이스 컨디션 대응:** 서버의 SurfaceBaking 페이즈 신호와 클라이언트의 로컬 월드젠 완료는 순서가 보장되지 않음.  
> `TryBeginClientBaking()`은 두 조건이 **모두 충족**되어야만 `BeginBaking()`을 호출하며,  
> `BeginBaking()` 내부의 `if (bIsBaking) return;` 가드로 이중 호출을 방지.  
> (`bBakingComplete` 조건은 의도적으로 제외 — 멀티 판 지원 시 매 판마다 재베이킹이 가능해야 하기 때문)

```
조건 A: OnRep_ServerPhase → SurfaceBaking
조건 B: OnClientWorldGenerationFinished (로컬 옥탄트 로드 완료)
         ↓
    TryBeginClientBaking()
    if (A && B) → BeginBaking()
```

### 4.3 베이킹 완료 후

```
ALNPPlayerController::BeginPlay()  (로컬 컨트롤러만 실행)
├─ ShowLoadingScreen()
└─ GetBakingProgress() >= 1.0?
   ├─ Yes (리슨 서버 로컬 플레이어 등 이미 완료된 경우)
   │   └─ OnLocalBakingComplete() 즉시 호출
   └─ No
       └─ SurfaceCacheSubsystem::OnBakingComplete에 OnLocalBakingComplete 구독

SurfaceCacheSubsystem::OnBakingComplete  (아직 완료 전인 경우)
        ↓
ALNPPlayerController::OnLocalBakingComplete()
├─ bLoadingComplete = true
├─ HideLoadingScreen()
└─ ServerNotifyClientReady() RPC  →  서버 ALNPGameMode::OnClientReady() 수신
```

> Mass 엔티티 스폰은 서버가 전담. 클라이언트는 MassReplication을 통해 엔티티 상태를 동기화받음.

---

## 5. 플레이어 폰 스폰 타이밍

`ALNPGameMode::RestartPlayer()`를 오버라이드하여 초기화 완료 전 스폰을 차단.

```cpp
void ALNPGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!bServerInitComplete)
    {
        PendingPlayers.AddUnique(NewPlayer);  // 큐에 대기
        return;
    }
    Super::RestartPlayer(NewPlayer);  // 즉시 스폰
}
```

| 상황 | 동작 |
|:---|:---|
| 초기화 완료 전 접속 | `PendingPlayers`에 대기 |
| `OnEntitySpawningComplete` 시점 | 대기 플레이어 전원 일괄 스폰 |
| 초기화 완료 후 뒤늦게 접속 | 즉시 스폰 |

---

## 6. 관련 클래스 인덱스

| 클래스 | 위치 | 역할 |
|:---|:---|:---|
| `ALNPGameMode` | `GameMode/LNPGameMode` | 서버 오케스트레이터 |
| `ALNPGameState` | `GameMode/LNPGameState` | 페이즈 복제 브릿지 |
| `ULNPOctantSpawnSubsystem` | `GameLogic/LNPOctantSpawnSubsystem` | 옥탄트 레벨 인스턴스 스폰 |
| `ULNPSurfaceCacheSubsystem` | `GameLogic/LNPSurfaceCacheSubsystem` | 구형 표면 사전 베이킹 |
| `ULNPMassSpawnSubsystem` | `GameLogic/LNPMassSpawnSubsystem` | Mass 엔티티 스폰 |
| `ALNPPlayerController` | `Player/LNPPlayerController` | 로딩 화면 + 클라이언트 준비 RPC |
