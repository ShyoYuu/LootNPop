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
- [x] **LootPod 랜덤 스폰 로직**
    - Mass Spawner와 연동한 LootPod 위치 결정 및 동적 스폰.

---

## Phase 3: Gameplay (전투 및 상호작용)

- [x] **MassEntity 기반 루팅 시스템** (`ULNPLootingProcessor`, `ULNPIdleToLootingProcessor`)
    - 상태 전환(Idle ↔ Looting ↔ Popped), 게이지 누적, 거리 체크 완료.
- [x] **GAS 기반 전투 시스템**
    - ASC/AttributeSet (`ALNPPlayerState`), `ULNPEquipmentComponent`, `ULNPInventoryComponent`.
    - 어빌리티 계층: `ULNPGameplayAbility` → `ULNPAbility_BasicAttack` → `ULNPAbility_RangedAttack` / `ULNPAbility_MeleeAttack`.
    - 발사체 시스템: `ULNPProjectileMovementProcessor`(PrePhysics) + `ULNPProjectileHitDetectionProcessor`(StartPhysics) + Visualization + Destruction 4개 프로세서.
    - 선분-캡슐 원거리 HitDetection, `InstigatorTeam` 팀 구분 피격 처리.
    - 공격 입력 바인딩 (`ULNPInputHandlerComponent`), 0.05초 입력 버퍼링.
- [x] **전투 애니메이션 시스템** (`ABP_Player`, `ULNPAnimInstance`)
    - `ALI_WeaponStyles` 인터페이스 정의 및 무기별 서브 AnimBP (`ABP_Unarmed`, `ABP_Sword`, `ABP_Pistol`) 제작.
    - UpperBody / FullBody 슬롯 분리 블랜딩 파이프라인 + Inertialization 적용.
    - GAS `State.Block.MovementInput` 태그 기반 이동 입력 차단 (`ULNPInputHandlerComponent` 연동).
    - 설계 명세: [TechDesign_CombatAnimation.md](TechDesign_CombatAnimation.md)
- [x] **총기류 Aim 모드**
    - 총기 장비 시 UpperBody 레이어에 Aiming 포즈 블랜딩.
    - Aim 중 카메라 전환 및 `bIsAiming` 상태 처리 (대시 조건 등 기존 연동 포함).
- [x] **Enemy NPC HP Bar** (월드 스페이스)
    - High LOD Actor 상태에서만 표시. `HP > 0 && HP < MaxHP` 조건 충족 시 가시화.
    - `UWidgetComponent` (World Space, Transparent 블렌드) + `ULNPHpBarWidget` (BindWidget 기반).
    - GAS Health 속성 변경 델리게이트로 실시간 갱신. 스폰 시 `SyncFromEntity`에서 초기값 주입.
    - Blueprint 서브클래스(`WBP_LNPHpBar`)에 `UProgressBar` 이름 `HpBar`로 배치 필요.
- [x] **플레이어 HUD** (MVVM 기반)
    - `ULNPHudViewModel`(FieldNotify): ASC 델리게이트로 `HealthPercent`, `bIsFreeAiming` 자동 갱신.
    - `ULNPHudWidget`: ViewModel 생성·주입(`UMVVMView::SetViewModel`)·해제 담당.
    - `ALNPPlayerController`: `BeginPlay`에서 위젯 생성, `OnPossess`/`OnUnPossess`에서 ViewModel 초기화·해제.
    - 조준점: `TAG_AimMode_FreeAim` 활성 시에만 표시 (bIsFreeAiming → Blueprint Function Binding).
    - 설계 명세: [TechDesign_HUD.md](TechDesign_HUD.md)
- [x] **근접 HitDetection** (AnimNotify 기반)
    - `UANS_LNPMeleeHitWindow`: 무기 본 위치를 매 프레임 `FLNPWeaponTraceFragment`에 기록.
    - `ULNPWeaponTraceHitDetectionProcessor`: Swept Quad(삼각형 2개) vs. 캡슐 축 선분 최단 거리 판정. `SwordRadius + CapsuleRadius` 임계값.
    - `ULNPWeaponTraceLifetimeProcessor`: `TimeToLive` 만료 시 엔티티 자동 파괴 (NotifyEnd 미호출 안전장치).
    - 중복 피격 방지 (`AlreadyHit[8]` 배열). 에디터 전용 디버그 드로우 프로세서 포함.
    - 피격 시 `FLNPPlayerLootingTag` 제거 → LootPod 루팅 취소 연동은 미구현.
- [x] **콤보 시스템**
    - 몽타주 섹션 분기 기반 다단 콤보 구현.
- [x] **Chooser 기반 몽타주 선택 시스템**
    - Chooser 테이블을 활용하여 상황에 맞는 공격 몽타주 자동 선택.
- [x] **산탄 공격**
    - 다수의 발사체를 분산 발사하는 산탄 공격 구현.
- [x] **히트리액션**
    - 피격 시 캐릭터 히트리액션 애니메이션 구현.
- [ ] **피격 반응 시스템** (넉백·드랍)
    - HitStop 구현 완료.
    - 미구현: 넉백 Launch (구형 곡률 기반 궤적), 아이템 드랍.
- [x] **Guard / Parry 시스템** (핵심 기능 완료, GameplayCue 에셋 연결 잔여)
    - Guard: `FLNPParryStateFragment` Fragment 각도 판정 → `FLNPGuardBlockCommand` → 데미지 차단 + GameplayCue. 동작 확인.
    - Parry(근접): Guard 입력 직후 0.15초 창 → `FLNPMeleeParryCommand` → 방어자 GA_ParrySuccess + 공격자 GA_Stagger.
    - Parry(투사체): Processor에서 Fragment Velocity/InstigatorTeam/Instigator 반전 → `FLNPProjectileParryCommand` (방어자 GA_ParrySuccess만). 동작 확인.
    - 판정 구조: `FLNPParryStateFragment` Mirror Fragment 기반으로 Processor(Worker Thread)에서 직접 판정. `FLNPApplyDamageGECommand`는 GE 적용 + HitReact + HitStop 처리.
    - 판정 반경 분리: `HitRadius`(피격)와 `ParryRadius`(패링)를 독립 필드로 분리. 2단계 판정 — ParryRadius 먼저 체크, 미발동 시 HitRadius 체크. 동작 확인.
    - Guard 이동 제한: `FLNPGuardModifier` (GuardWalkSpeed 200 cm/s) — Sprint와 동일한 Mover Modifier 패턴.
    - Guard 자세 애니메이션: `ABP_Sub_LongSword`에서 `bIsGuarding` Bool Blend 노드로 Guard 자세 블렌딩. 동작 확인.
    - 에디터 잔여: GameplayCue 에셋 연결 (`GameplayCue.LNP.Guard.Block`, `GameplayCue.LNP.Parry.Success`), Guided/Lobbed 투사체 반사 타입.
    - 설계 명세: [TechDesign_ParrySystem.md](TechDesign_ParrySystem.md)

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

- [ ] **Iris 기반 네트워킹 구현**
    - 설계 명세: [TechDesign_Networking.md](TechDesign_Networking.md)
    - [x] Phase 1: Iris 활성화, GAS 복제 모드, ALNPEnemyCharacter bReplicates, GameplayCue 에셋 연결 (PIE 2P 검증 완료)
    - [x] Phase 2: HitStop/HitReact GameplayCue 전파 (코드 완료, GameplayCueNotify VFX·그래프 연결은 에디터 잔여)
    - [x] Phase 3: 근접 HitDetection 클라이언트 예측 + Lag Compensation + Guard/Parry 서버 복제 (PIE 2인 근접 PvP 왕복 검증 완료 — `UAnimNotifyState` 싱글턴 공유로 인한 근접 판정 실패 버그 발견·수정)
    - Phase 4: 원거리 Projectile 클라이언트 예측 (FScopedPredictionWindow, 롤백 처리)
    - [x] Phase 4.5: Ghost Projectile 정합성 개선 — 키 전역 고유화(SalvoID), per-entry TTL, 패링 반사 소멸+재스폰, 관전 Ghost Dead Reckoning·로컬 충돌, Rewind 발사 시점 캐싱 (코드 완료, PIE 3인 검증 대기)
    - Phase 5: 무기 교체 서버 권한화 (Server_EquipWeapon RPC)
    - Phase 6: Enemy MassReplication (FMassNetworkID, BubbleHandler) + ALNPEnemyCharacter bReplicates (이중 복제)
    - [x] Phase 6.5: Player MassReplication — 존재만 복제하는 최소 스키마 bubble + 엔진 퍼펫 링크로 클라 플레이어 엔티티 성립. 에이전트 경로 NetID 캐싱 타이밍 갭은 ULNPMassAgentComponent로 보정 (PIE 2인 검증: players=2, gap=0cm)
    - Phase 7: LootPod MassReplication + ALNPLootPod bReplicates (이중 복제)
- [ ] **승리 조건 및 세션 관리**
    - 메달(가칭) 4개 수집 시 승리. 게임 시작/종료/결과 처리 흐름.

---

## Phase 6: Polish (최종 폴리싱)

- [ ] **HUD 추가 요소** (인벤토리, 미니맵, 점수 등)
- [ ] **VFX(Niagara) 및 사운드 통합**
- [ ] **게임플레이 밸런싱**
