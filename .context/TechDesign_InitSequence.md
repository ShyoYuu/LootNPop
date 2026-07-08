# 초기화 시퀀스 기술 설계

## 1. 한눈에 보기

LootNPop의 월드는 즉시 플레이 가능한 상태가 아니다. **옥탄트 월드 생성 → 표면 캐시 베이킹 → Mass 엔티티 스폰**이 완료되어야 게임플레이가 시작되며, 이 순서를 서버가 주도하고 클라이언트가 복제 신호에 반응하는 구조다.

**핵심 원칙:**
- 서버(`ALNPGameMode`)가 단계 진행을 결정하고, `ALNPGameState::ServerPhase` 복제로 클라이언트에 전파.
- 클라이언트는 각 OnRep 콜백에서 자신의 로컬 작업(월드 생성·베이킹)을 수행.
- 플레이어 폰 스폰은 **서버 초기화 완료 + 해당 클라이언트 준비 완료**의 두 게이트를 모두 통과해야 허용.

### 초기화 단계 (ELNPInitPhase)

| 단계 | 열거값 | 담당 클래스 |
|:---|:---|:---|
| 옥탄트 월드 생성 | `WorldGeneration` | `ULNPOctantSpawnSubsystem` |
| 표면 캐시 베이킹 | `SurfaceBaking` | `ULNPSurfaceCacheSubsystem` |
| Mass 엔티티 스폰 | `EntitySpawning` | `ULNPMassSpawnSubsystem` |
| 초기화 완료 | `Complete` | — |

### 관련 클래스 인덱스

| 클래스 | 위치 | 역할 |
|:---|:---|:---|
| `ALNPGameMode` | `GameMode/LNPGameMode` | 서버 오케스트레이터 + 폰 스폰 게이팅 |
| `ALNPGameState` | `GameMode/LNPGameState` | 페이즈·시드 복제 브릿지 + 클라이언트 투-게이트 |
| `ULNPOctantSpawnSubsystem` | `GameLogic/LNPOctantSpawnSubsystem` | 옥탄트 레벨 인스턴스 스폰 |
| `ULNPSurfaceCacheSubsystem` | `GameLogic/LNPSurfaceCacheSubsystem` | 구형 표면 사전 베이킹 |
| `ULNPMassSpawnSubsystem` | `GameLogic/LNPMassSpawnSubsystem` | Mass 엔티티 스폰 |
| `ALNPPlayerController` | `Player/LNPPlayerController` | 로딩 스크린 + 클라이언트 Ready RPC |

---

## 2. 서버 초기화 흐름

`ALNPGameMode`는 서버 전용 클래스이므로 아래 흐름은 서버에서만 실행된다.

```
ALNPGameMode::BeginPlay()
│
├─ OctantGenSeed 확정 (config가 0이면 FMath::Rand()로 생성)
├─ GameState->OctantGenSeed = seed  ──────────────── (복제됨)
├─ GameState->ServerPhase = WorldGeneration  ──────── (복제됨)
└─ OctantSpawnSubsystem::StartWorldGeneration()
        │ Tick()마다 8개 레벨 인스턴스 로드+가시화 완료 감시
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
└─ PendingPlayers 순회 → ReadyClients 등록된 플레이어만 Super::RestartPlayer()
```

---

## 3. 클라이언트 초기화 흐름

클라이언트는 GameMode를 갖지 않는다. GameState의 OnRep 콜백으로 각 단계에 반응한다.

### 3.1 월드 생성

```
ALNPGameState::OnRep_OctantGenSeed()
└─ [!bGenerationComplete && !IsTickable() 가드 — 이미 진행/완료 시 중복 호출 방지]
   ├─ OctantSub->OnWorldGenerationFinished에 OnClientWorldGenerationFinished 구독
   └─ OctantSpawnSubsystem::StartWorldGeneration()  (서버와 동일한 seed로 결정론적 스폰)
           │ 로드 완료 시
           ▼
ALNPGameState::OnClientWorldGenerationFinished()
└─ [ServerPhase >= SurfaceBaking 조건 확인 후]
   └─ TryBeginClientBaking()
```

### 3.2 표면 캐시 베이킹 — 투-게이트

```
조건 A: OnRep_ServerPhase → SurfaceBaking 수신
조건 B: OnClientWorldGenerationFinished (로컬 옥탄트 로드 완료)
         ↓
    TryBeginClientBaking()
    if (A && B) → BeginBaking()
```

서버의 SurfaceBaking 페이즈 신호와 클라이언트의 로컬 월드젠 완료는 **도착 순서가 보장되지 않는다**. `TryBeginClientBaking()`은 두 조건이 모두 충족되어야만 `BeginBaking()`을 호출하며, `BeginBaking()` 내부의 `if (bIsBaking) return;` 가드가 이중 호출을 방지한다. (상세 분석: §5.1)

> `bBakingComplete` 조건은 의도적으로 게이트에서 제외 — 멀티 판(라운드 재시작) 지원 시 매 판마다 재베이킹이 가능해야 하기 때문.

### 3.3 베이킹 완료 후 — Ready 신호

```
ALNPPlayerController::BeginPlay()  (로컬 컨트롤러만 실행)
├─ ShowLoadingScreen()
└─ GetBakingProgress() >= 1.0?
   ├─ Yes (리슨 서버 로컬 플레이어 등 이미 완료된 경우) → OnLocalBakingComplete() 즉시 호출
   └─ No → SurfaceCacheSubsystem::OnBakingComplete에 구독

ALNPPlayerController::OnLocalBakingComplete()
├─ bLoadingComplete = true
├─ HideLoadingScreen()
└─ ServerNotifyClientReady() RPC  →  서버 ALNPGameMode::OnClientReady() 수신
```

> Mass 엔티티 스폰은 서버 전담. 클라이언트는 MassReplication으로 엔티티 상태를 동기화받는다.

---

## 4. 플레이어 폰 스폰 게이팅

폰 스폰은 **두 조건을 모두 충족**해야 한다.

| 게이트 | 의미 |
|:---|:---|
| `bServerInitComplete` | 서버 전체 초기화 완료 (WorldGen → Baking → EntitySpawn) |
| `ReadyClients` 등록 | 해당 클라이언트가 로컬 베이킹 완료 후 `ServerNotifyClientReady()` 전송 |

`ALNPGameMode::RestartPlayer()`는 두 조건을 순차 확인하여 미충족 시 `PendingPlayers`에 보류하고, `OnClientReady()`는 등록 후 서버가 이미 완료 상태면 즉시 스폰을 트리거한다.

| 상황 | 동작 |
|:---|:---|
| 서버 미완료 + 클라이언트 접속 | `PendingPlayers` 대기 |
| `OnEntitySpawningComplete` 시점 | `ReadyClients` 등록된 플레이어만 스폰, 나머지 보류 |
| `OnClientReady` 수신 (서버 완료 후) | 해당 클라이언트 즉시 스폰 (중간 참여 포함) |
| 초기화 완료 후 접속 → 로컬 베이킹 완료 | `OnClientReady` 경로로 스폰 |

---

## 5. 어필 포인트 (트러블슈팅 & 설계 판단)

### 5.1 순서가 보장되지 않는 두 신호의 합류 — 투-게이트 패턴

클라이언트 베이킹 시작에는 "서버가 SurfaceBaking 단계에 진입했다"(복제 신호)와 "로컬 옥탄트 로드가 끝났다"(로컬 이벤트)라는 **서로 다른 소스의 두 신호**가 필요하다. 네트워크 상황과 로딩 속도에 따라 어느 쪽이 먼저 도착할지 알 수 없는 전형적인 레이스 컨디션.

**해결:** 두 콜백이 각각 같은 합류 함수(`TryBeginClientBaking`)를 호출하되, 함수 내부에서 **두 조건을 모두 재검사**하고, 실제 작업 함수(`BeginBaking`)에는 재진입 가드를 두는 구조. 어느 신호가 먼저 오든 마지막 신호가 도착하는 순간 정확히 한 번 실행된다.

### 5.2 폰 스폰의 이중 게이트 — "빈 월드 스폰" 방지

서버 초기화 완료 **후에** 접속한 클라이언트는 UE 로그인 흐름에서 `RestartPlayer()`가 즉시 호출된다. 서버 기준으로는 스폰 가능하지만, 클라이언트 로컬에서는 아직 옥탄트도 로드되지 않은 상태 — `ReadyClients` 게이트가 없으면 **빈 월드 한가운데에 폰이 스폰**된다. 서버 상태와 클라이언트 상태를 별도 게이트로 분리해 중간 참여(Late Join)까지 안전하게 처리했다.

### 5.3 로딩 스크린과 초기화 파이프라인의 자연스러운 연동

베이킹 진행률(`GetBakingProgress`)이 곧 로딩 진행률이 되는 구조. 리슨 서버의 로컬 플레이어처럼 구독 시점에 이미 베이킹이 끝나 있는 경우(델리게이트를 영원히 기다리는 버그 소지)는 진행률 선확인으로 처리한다.

---

## 6. 미구현 / 한계

- **라운드 재시작 흐름:** 투-게이트가 재베이킹을 허용하도록 설계되어 있으나(§3.2), 실제 멀티 판 세션 관리(승리 → 리셋 → 재초기화)는 미구현 (DevelopmentPlan Phase 5).
- **로딩 진행률 UI:** `GetBakingProgress()` API는 존재하나 로딩 스크린에 게이지로 표시하는 UI 연동은 미구현.
- **초기화 실패 처리:** 옥탄트 로드 실패·베이킹 트레이스 전체 미스 등 실패 경로에 대한 재시도/에러 플로우 없음.
