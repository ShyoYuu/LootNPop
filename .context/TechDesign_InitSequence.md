# 초기화 시퀀스 기술 설계

## 1. 한눈에 보기

월드는 **옥탄트 월드 생성 → 표면 캐시 베이킹 → Mass 엔티티 스폰**이 끝나야 플레이 가능하다. 서버가 순서를 주도하고 클라이언트는 복제 신호에 반응한다.

- 서버(`ALNPGameMode`)가 단계를 진행하고 `ALNPGameState::ServerPhase` 복제로 전파한다.
- 클라이언트는 OnRep 콜백에서 로컬 작업(월드 생성·베이킹)을 한다.
- 플레이어 폰 스폰은 **서버 초기화 완료 + 해당 클라이언트 준비 완료** 두 게이트를 모두 통과해야 한다.

| 단계 (`ELNPInitPhase`) | 담당 |
|:---|:---|
| `WorldGeneration` | `ULNPOctantSpawnSubsystem` |
| `SurfaceBaking` | `ULNPSurfaceCacheSubsystem` |
| `EntitySpawning` | `ULNPMassSpawnSubsystem` (서버 전용) |
| `Complete` | — |

| 클래스 | 위치 | 역할 |
|:---|:---|:---|
| `ALNPGameMode` | `GameMode/` | 서버 오케스트레이터 + 폰 스폰 게이팅 |
| `ALNPGameState` | `GameMode/` | 페이즈·시드 복제 + 클라이언트 투-게이트 |
| `ALNPPlayerController` | `Player/` | 로딩 스크린 + 클라이언트 Ready RPC |

---

## 2. 서버 초기화 흐름

> 이보다 앞서 `ULNPMassSpawnSubsystem::OnWorldBeginPlay()`가 **서버·클라이언트 양쪽에서** Mass 템플릿 warm-up과(서버라면) 복제 파괴 옵저버 복구를 한다. NetDriver 생성 이후·`GameMode::StartPlay` 이전이어야 해서 이 자리다 — 근거는 [TechDesign_Networking.md](TechDesign_Networking.md)의 템플릿 빌드 시점 절.

```
ALNPGameMode::BeginPlay()
├─ OctantGenSeed 확정 (config가 0이면 FMath::Rand()) ─── (복제)
├─ ServerPhase = WorldGeneration ─────────────────────── (복제)
└─ OctantSpawnSubsystem::StartWorldGeneration()
        ▼ 8개 레벨 인스턴스 로드+가시화 완료
OnWorldGenerationComplete()
├─ ServerPhase = SurfaceBaking ───────────────────────── (복제)
└─ SurfaceCacheSubsystem::BeginBaking()
        ▼ Tick 분할 비동기 트레이스 → 전체 콜백 수집
OnSurfaceBakingComplete()
├─ ServerPhase = EntitySpawning ──────────────────────── (복제)
└─ MassSpawnSubsystem::BeginSpawning()
        │ SurfaceCache 스냅샷 + TaskGraph에서 위치 계산 → 게임 스레드에서 큐 조립
        ▼ Tick마다 MaxSpawnsPerFrame씩 스폰
OnEntitySpawningComplete()
├─ ServerPhase = Complete ────────────────────────────── (복제)
├─ bServerInitComplete = true
└─ PendingPlayers 중 ReadyClients에 있는 플레이어만 Super::RestartPlayer()
```

---

## 3. 클라이언트 초기화 흐름

### 3.1 월드 생성

```
OnRep_OctantGenSeed()
└─ [!bGenerationComplete && !IsTickable() — 중복 방지]
   ├─ OnWorldGenerationFinished 구독
   └─ StartWorldGeneration()  (서버와 같은 seed)
           ▼
OnClientWorldGenerationFinished()
└─ [ServerPhase >= SurfaceBaking이면] TryBeginClientBaking()
```

### 3.2 표면 캐시 베이킹 — 투-게이트

```
조건 A: OnRep_ServerPhase → SurfaceBaking 수신
조건 B: OnClientWorldGenerationFinished (로컬 옥탄트 로드 완료)
         ↓
TryBeginClientBaking(): A && B → BeginBaking()
```

두 신호의 **도착 순서는 보장되지 않는다**. 합류 함수가 두 조건을 재검사하고, `BeginBaking()`의 `if (bIsBaking || bBakingComplete) return;`이 이중 호출과 완료 후 재호출을 막는다(§5.1).

> **베이킹은 머신당 1회다.** 완료 후 재베이킹하면 Mass 워커가 읽는 도중 배열 포인터가 바뀌기 때문이다 — [TechDesign_SurfaceCache.md](TechDesign_SurfaceCache.md) §3.4.

### 3.3 베이킹 완료 후 — Ready 신호

```
ALNPPlayerController::BeginPlay()  (로컬 컨트롤러만)
├─ ShowLoadingScreen()
└─ GetBakingProgress() >= 1.0 ? OnLocalBakingComplete() 즉시 : OnBakingComplete 구독

OnLocalBakingComplete()
├─ bLoadingComplete = true, HideLoadingScreen()
└─ ServerNotifyClientReady() RPC → ALNPGameMode::OnClientReady()
```

진행률을 먼저 확인하는 이유: 리슨 서버 로컬 플레이어처럼 구독 시점에 이미 끝났으면 델리게이트를 영원히 기다린다.

---

## 4. 플레이어 폰 스폰 게이팅

| 게이트 | 의미 |
|:---|:---|
| `bServerInitComplete` | 서버 전체 초기화 완료 |
| `ReadyClients` 등록 | 해당 클라이언트가 로컬 베이킹 후 Ready RPC 전송 |

`RestartPlayer()`는 둘 중 하나라도 미충족이면 `PendingPlayers`에 보류한다.

| 상황 | 동작 |
|:---|:---|
| 서버 미완료 중 접속 | `PendingPlayers` 대기 |
| `OnEntitySpawningComplete` | Ready인 플레이어만 스폰 |
| 서버 완료 후 `OnClientReady` (중간 참여 포함) | 즉시 스폰 |
| 사망 후 리스폰 (`ScheduleRespawn` → `DoRespawn`) | 이미 두 게이트를 통과해 `RestartPlayer()`가 곧바로 스폰 |

> 리스폰은 같은 게이트를 탄다. `DoRespawn()`은 먼저 랙돌 폰을 `UnPossess` 후 파괴하고(폰을 쥔 컨트롤러는 엔진이 스폰을 건너뛴다), PlayerState 소유 ASC의 Health를 MaxHealth로 되돌린다. 사망 흐름 전체는 [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md).

---

## 5. 어필 포인트 (트러블슈팅 & 설계 판단)

### 5.1 순서가 보장되지 않는 두 신호의 합류 — 투-게이트

베이킹 시작에는 복제 신호(서버 페이즈)와 로컬 이벤트(옥탄트 로드)라는 **출처가 다른 두 신호**가 필요하다. 두 콜백이 같은 합류 함수를 부르고, 합류 함수는 두 조건을 재검사하며, 작업 함수에는 재진입 가드를 둔다. 어느 쪽이 먼저 오든 마지막 신호에서 정확히 한 번 실행된다.

### 5.2 폰 스폰의 이중 게이트 — "빈 월드 스폰" 방지

서버 초기화 **후** 접속한 클라이언트는 로그인 흐름에서 `RestartPlayer()`가 즉시 불린다. 서버 기준으로는 스폰 가능해도 클라이언트는 옥탄트조차 로드 전이라, `ReadyClients` 게이트가 없으면 **빈 월드에 폰이 스폰**된다. 서버·클라이언트 상태를 별도 게이트로 분리해 중간 참여까지 처리했다.

---

## 6. 미구현 / 한계

- **라운드 재시작:** 멀티 판 세션 관리(승리 → 리셋 → 재초기화)는 미구현(DevelopmentPlan Phase 5). `BeginBaking()`이 완료 후 재호출을 막으므로 SurfaceCache의 리셋 진입점부터 필요하다.
- **로딩 진행률 UI:** `GetBakingProgress()`는 분할 발사로 실제 차오르지만, 로딩 스크린(`ShowLoadingScreen`/`HideLoadingScreen` — BlueprintImplementableEvent)에 게이지로 표시하는 연동은 없다.
- **초기화 실패 처리:** 옥탄트 로드 실패·트레이스 전체 미스 등에 재시도/에러 플로우가 없다.
