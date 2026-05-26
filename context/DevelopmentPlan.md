# LootNPop 개발 계획 (Development Plan)

---

## Phase 1: Foundation (핵심 시스템 & 구형 월드)

- [x] **Mover 플러그인 기반 캐릭터 이동**
    - `ULNPCharacterMoverComponent` + `ALNPCharacterBase` 구현. Enhanced Input 매핑 포함.
- [x] **구형 중력 시스템** (`ULNPPawnGravityComponent`)
    - `RadialOutward` 모드로 Dyson Sphere 내벽 중력 구현. 곡률 보정(`UpdateControllerOrientation`) 및 Pitch 클램프 완료.
- [x] **질주 시스템** (`LNPSprintModifier`)
    - Mover의 Stance 패턴 적용. 수정자(Modifier) 기반 설계로 네트워크 롤백 안정성 확보.
- [x] **게임 초기화 시퀀스** (`ALNPGameMode`, `ALNPGameState`)
    - 4단계 서버 주도 초기화: `WorldGeneration → SurfaceBaking → EntitySpawning → Complete`
    - 서버: `GameMode::BeginPlay()` 기점으로 단계 진행 및 `ServerPhase` 복제.
    - 클라이언트: `OnRep_OctantGenSeed` → 월드 생성, `OnRep_ServerPhase` → TryBeginClientBaking() (투게이트 패턴).
    - 플레이어 스폰 게이팅: `bServerInitComplete = true`까지 `PendingPlayers` 큐 보관.
- [x] **Octant 기반 월드 분할 스폰** (`ULNPOctantSpawnSubsystem`)
    - 8개 Level Instance를 결정론적 시드(`FRandomStream(OctantGenSeed)`)로 배치.
    - 서버 GameMode → 클라이언트 OnRep 경유로 동일 seed 보장.
- [x] **표면 캐시 시스템** (`ULNPSurfaceCacheSubsystem`)
    - 게임 시작 전 구형 내벽 표면 좌표를 등장방형(Equirectangular) 그리드로 사전 계산.
    - Mass 워커 스레드에서 O(1) 안전 조회. NavMesh 및 인라인 라인 트레이스를 대체.
- [x] **로딩 스크린 흐름** (`ALNPPlayerController`)
    - 클라이언트 로컬 베이킹 완료 시 로딩 스크린 해제 후 서버에 Ready 신호 전송.

---

## Phase 2: Environment (환경 및 상세화)

- [x] **PCG 기반 구체 지형 생성** (`UPCGSphereWorldSettings`)
    - Spherified Octant 투영 + 3D Perlin Noise 변조 + 경사면 정렬 구현.
    - 결과물은 HISM으로 Bake하여 런타임 계산 제거.
- [x] **SmartObject 기반 상호작용 인프라** (`ALNPLootPod`)
    - `SmartObjectComponent` + `UMassAgentComponent` 구성. Niagara VFX 연동.
- [ ] **LootPod 랜덤 스폰 로직**
    - Mass Spawner와 연동한 LootPod 위치 결정 및 동적 스폰.

---

## Phase 3: Gameplay (전투 및 상호작용)

- [x] **MassEntity 기반 루팅 시스템** (`ULNPLootingProcessor`, `ULNPIdleToLootingProcessor`)
    - 상태 전환(Idle ↔ Looting ↔ Popped), 게이지 누적, 거리 체크 완료.
- [x] **GAS 기반 전투 시스템**
    - ASC/AttributeSet (`ALNPPlayerState`), `ULNPEquipmentComponent`, `ULNPInventoryComponent`.
    - 어빌리티 계층: `ULNPGameplayAbility` → `ULNPAbility_BasicAttack` → `ULNPAbility_RangedAttack`.
    - 발사체 시스템: `ULNPProjectileMovementProcessor`(PrePhysics) + `ULNPProjectileHitDetectionProcessor`(StartPhysics) + Visualization + Destruction 4개 프로세서.
    - 선분-캡슐 원거리 HitDetection, `InstigatorTeam` 팀 구분 피격 처리.
    - 공격 입력 바인딩 (`ULNPInputHandlerComponent`), 0.05초 입력 버퍼링.
- [ ] **전투 애니메이션 시스템** (`ABP_Player`, `ULNPAnimInstance`)
    - `ALI_WeaponStyles` 인터페이스 정의 및 무기별 서브 AnimBP (`ABP_Unarmed`, `ABP_Sword`, `ABP_Pistol`) 제작.
    - UpperBody / FullBody 슬롯 분리 블랜딩 파이프라인 + Inertialization 적용.
    - GAS `State.Block.MovementInput` 태그 기반 이동 입력 차단 (`ULNPInputHandlerComponent` 연동).
    - 설계 명세: [TechDesign_CombatAnimation.md](TechDesign_CombatAnimation.md)
- [ ] **총기류 Aim 모드**
    - 총기 장비 시 UpperBody 레이어에 Aiming 포즈 블랜딩.
    - Aim 중 카메라 전환 및 `bIsAiming` 상태 처리 (대시 조건 등 기존 연동 포함).
- [x] **Enemy NPC HP Bar** (월드 스페이스)
    - High LOD Actor 상태에서만 표시. `HP > 0 && HP < MaxHP` 조건 충족 시 가시화.
    - `UWidgetComponent` (World Space, Transparent 블렌드) + `ULNPHpBarWidget` (BindWidget 기반).
    - GAS Health 속성 변경 델리게이트로 실시간 갱신. 스폰 시 `SyncFromEntity`에서 초기값 주입.
    - Blueprint 서브클래스(`WBP_LNPHpBar`)에 `UProgressBar` 이름 `HpBar`로 배치 필요.
- [ ] **플레이어 HUD HP Bar**
    - 자신의 HP 바 표시 (MVVM 기반).
- [ ] **근접 HitDetection** (AnimNotify 기반)
    - 칼날 Swept Volume vs. 타겟 캡슐 최단 거리.
    - 피격 시 `FLNPPlayerLootingTag` 제거 → LootPod 루팅 취소 연동.
- [ ] **피격 반응 시스템**
    - 넉백 Launch (구형 곡률 기반 궤적), HitStop, 아이템 드랍.
- [ ] **패링 시스템** (`FLNPParryStateFragment`)
    - HitDetection 프로세서 내부에서 방향·거리 조건 검사.
    - GAS 어빌리티로 입력 윈도우(`bIsParrying`) 관리.

---

## Phase 4: AI & Scale (대규모 AI)

- [x] **MassEntity 기반 적 NPC 스폰** (`ULNPMassSpawnSubsystem`)
    - 데이터 에셋(`DA_MassSpawnConfig`) 기반 배치. 스폰 완료 시 `OnSpawningComplete` 델리게이트 발행.
- [x] **적 이동 프로세서** (`ULNPEnemyMovementProcessor`)
    - 구형 표면을 따른 이동. `ULNPSurfaceCacheSubsystem`에서 표면 노멀 조회 (스레드 안전).
- [x] **슬롯 기반 타겟팅 서브시스템** (`ULNPTargetingSubsystem`)
    - Melee/Ranged 슬롯 풀 관리. `DistanceToTargetSq` 기반 우선순위 경쟁.
- [x] **StateTree 기반 적 AI** (`ULNPEnemyStateTreeProcessors`)
    - Leash / Chase / Attack 상태 전환. Mass-StateTree 통합.
- [ ] **시야각·상태 기반 타겟팅 가중치 보강**
    - 현재 거리 기반만 구현. 시야각(Angle) 및 공격 상태 가중치 추가.
- [ ] **난이도 스케일링**
    - 활성 LootPod 수 추적 → 슬롯 한도 또는 적 능력치 단계적 조정.
- [x] **LOD 기반 Actor ↔ Entity 전환**
    - `ULNPEnemyLODOverrideProcessor`: Confirmed/Combat 상태 시 거리 무관 High LOD 강제.
    - `ULNPEnemyActorInitializerProcessor` / `ULNPEnemyActorSyncProcessor`: Config 기반 초기화, HP/타겟 역동기화.

---

## Phase 5: Loop (멀티플레이어 완성)

- [ ] **Iris 기반 네트워킹 최적화**
    - MassEntity 상태 복제 및 예측 보정.
- [ ] **승리 조건 및 세션 관리**
    - 메달(가칭) 4개 수집 시 승리. 게임 시작/종료/결과 처리 흐름.

---

## Phase 6: Polish (최종 폴리싱)

- [ ] **MVVM 기반 HUD 및 반응형 UI**
- [ ] **VFX(Niagara) 및 사운드 통합**
- [ ] **게임플레이 밸런싱**
