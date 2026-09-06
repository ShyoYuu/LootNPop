# LootNPop 프로젝트 개요

**LootNPop**은 Unreal Engine 최신 스택을 활용한 경쾌한 분위기의 멀티플레이어 파티 게임.  
거대한 구체의 내부 표면(Dyson Sphere)이 플레이 공간이며, 수백~수천 규모의 MassEntity 적을 상대로 LootPod를 쟁취하는 것이 핵심 루프.  
핵심 로직은 가급적 C++로 구현.

---

## 1. Tech Stack

| 구분 | 기술 스택 | 주요 구현 클래스 |
|:---|:---|:---|
| **Input** | Enhanced Input | `ALNPCharacterBase`, `ULNPInputHandlerComponent` |
| **Movement** | Mover 2.0 | `ULNPCharacterMoverComponent`, `ULNPPawnGravityComponent` |
| **Animation** | Motion Matching + Linked Anim Layers | `ULNPAnimInstance` |
| **Camera** | Gameplay Camera | `ULNPGravityRollCorrectionCameraNode` |
| **World** | PCG + Geometry Script + Level Instance | `BP_OctantGenerator`, `ULNPOctantThemeSamplerSettings`, `ULNPOctantSpawnSubsystem` |
| **Surface Query** | SurfaceCacheSubsystem (커스텀) | `ULNPSurfaceCacheSubsystem` |
| **Interaction** | MassEntity + 인터랙터블 레지스트리 | `ALNPLootPod`, `ALNPLootDice`, `ULNPInteractableRegistrySubsystem`, `ULNPInteractionComponent` |
| **AI/Entity** | MassEntity + StateTree | `ULNPEnemyMovementProcessor`, `ULNPTargetingSubsystem` |
| **Combat** | GAS + MassEntity | `ULNPEquipmentComponent`, `ULNPAbility_RangedAttack`, `ULNPProjectileHitDetectionProcessor` |
| **Item / Inventory** | UObject 인스턴스 + FastArray Delta Replication | `ULNPInventoryComponent`, `ULNPInventoryItemInstance`, `FLNPGameplayTagStackContainer` |
| **Networking** | Iris Replication + MassReplication | `ULNPGhostProjectileSubsystem`, `ULNPSpawnOnlyReplicatorBase`, `ULNPMassAgentComponent` |
| **UI** | CommonUI + MVVM Plugin | `ULNPHudViewModel`, `ULNPHudWidget`, `ULNPMenuRootWidget`, `ULNPStatsViewModel` |

---

## 2. Core Systems

### Custom Gravity
- Mover 기능을 활용하여 `Fixed`, `RadialInward`, `RadialOutward` 3가지 중력 모드 구현.
- `ULNPPawnGravityComponent`가 `RadialOutward` 모드로 Dyson Sphere 내벽 중력 적용. Pawn 위치마다 달라지는 Up/Gravity Direction을 매 프레임 갱신하여 화면 기울어짐 방지.

### Enemy NPC
- 모든 NPC는 MassEntity(Low LOD) ↔ Actor(High LOD) 하이브리드 구조. 전투 진입(Confirmed/Combat) 시 `ULNPEnemyLODOverrideProcessor`가 High LOD Actor로 자동 전환 — 단 `ULNPEnemyConfig::CombatMode`가 `PureEntity`인 개체는 승격하지 않고 Mass 프로세서가 직접 공격한다.
- 플레이어별 어그로 관리는 `ULNPTargetingSubsystem` 담당. GAS 어빌리티는 Actor 상태에서만 실행. AI는 StateTree 기반으로 구현.

### Hit Detection
- 물리 엔진 콜리전 이벤트 미사용. 모든 Hit·Guard·Parry 판정은 Mass Processor 내에서 수학적으로 일괄 계산 (`ULNPProjectileHitDetectionProcessor`, `ULNPWeaponTraceHitDetectionProcessor`).
- 원거리: 직전/현재 프레임 Projectile 위치로 선분 생성 후 주변 캡슐과의 거리 계산.
- 근거리: Anim Notify State(`UANS_LNPMeleeHitWindow`)가 매 프레임 무기 위치 동기화 후 4점 삼각형 2개로 캡슐 거리 계산.
- Hit 판정 시 Fragment에 담긴 `UGameplayEffect` 포인터를 피격자에 적용.

### Surface Cache Baking
- Enemy NPC와 Projectile이 모두 MassEntity이므로 Mass Worker Thread에서 지표면 쿼리가 가능해야 함.
- `ULNPSurfaceCacheSubsystem`이 게임 시작 전 구형 내벽 전체를 등장방형 그리드로 사전 베이킹. 베이킹 완료 후 배열이 읽기 전용으로 확정되므로 Mass Worker Thread에서 Lock 없이 O(1) 안전 조회 가능.

### Motion Matching + Linked Anim Layers
- Motion Matching 기반 Locomotion에 무기별 Linked Anim Layer를 블렌딩하여 4종 무기(Pistol·Rifle·Shotgun·LongSword) 구현.
- Pistol·Rifle·Shotgun은 Aim Offset 적용. 발사 직전 카메라 방향 LineTrace로 화면 중앙 조준점을 검출하여 해당 방향으로 Projectile 발사.

### LootPod & LootDice
- **LootPod**은 MassEntity(시뮬레이션) + Actor(비주얼·상호작용·복제) 하이브리드. 게이지·근접 판정은 서버 Mass 프로세서가 태그 교체 상태 머신(`Idle ↔ Looting → Popped`)으로 처리.
- 루팅 존 안의 **모든 플레이어 루팅 속도를 합산**해 게이지가 오르므로 협동하면 빨라지고, 전원 이탈 시 감쇠하다 0에 도달해야 완전 취소된다. 상호작용(150cm)과 루팅 존(500cm) 반경은 분리.
- 상호작용 탐색에 쓰던 SmartObject 공간 쿼리는 Mass 액터 풀링과 충돌해 폐기하고, `ULNPInteractableRegistrySubsystem`(살아 있는 인터랙터블 레지스트리) 순회로 대체. (→ [DiscardedApproaches.md](DiscardedApproaches.md) Case 03)
- **LootDice**는 보상의 월드 실체화 형태(주사위 픽업). Pod의 `Popped` 또는 인벤토리 드랍으로 생성되며, Mass가 아닌 **순수 Actor + 서버 권위 물리 + Iris `ReplicatedMovement`** 로 구현. (→ [TechDesign_LootDice.md](TechDesign_LootDice.md))

### Inventory & Items
- 공유 DataAsset 포인터 대신 **`UObject` 아이템 인스턴스**(`ULNPInventoryItemInstance`) 모델. 각 사본이 `FGuid ItemId` 정체성을 가져 장착본/보관본 구분과 인스턴스별 상태(레벨·랜덤 스탯·버프 잔여시간)를 담는다.
- 복제는 **FastArray 델타 복제 + 등록 서브오브젝트**(`bReplicateUsingRegisteredSubObjectList`, `COND_OwnerOnly`). Iris 환경에서 2인 PIE 실측 검증 완료.
- 스탯은 Lyra식 `FLNPGameplayTagStackContainer`(`Tag→int32`)에 저장. 버프는 서버가 권위 잔여시간을 세고 각 클라이언트는 스냅샷 기준 로컬 카운트다운으로 표시(시계 동기화 불필요). (→ [TechDesign_Inventory.md](TechDesign_Inventory.md))

### UMG MVVM Plugin
- UMG MVVM Plugin 기반으로 C++ ViewModel(`ULNPHudViewModel`)과 View Binding을 분리. ASC 델리게이트로 속성 변화를 ViewModel에 바인딩하고 Blueprint Widget이 이를 구독.

### In-Game Menu (CommonUI)
- 게임패드 우선 인게임 메뉴를 CommonUI로 구성. **캐릭터 스탯 / 인벤토리 / 환경설정** 3탭 구조이며, 반투명 팝업으로 띄워 메뉴 중에도 전투 상황을 인지할 수 있게 함.
- 위젯 계층은 `UCommonActivatableWidgetStack`(메뉴 열기/닫기) + `UCommonActivatableWidgetSwitcher`(탭 전환) 조합. `UCommonTabListWidgetBase`가 L1/R1 탭 이동을 담당. 하단 힌트 바는 `UCommonBoundActionBar`를 버리고 커스텀 위젯(`ULNPMenuHintBarWidget`)으로 직접 구현해, 입력 방식(키보드↔게임패드)에 따라 키 글리프만 실시간 교체한다.
- Back(○) 전파는 **루트 위젯 하나만 핸들러**로 두고 활성 탭에 먼저 위임하여 `디테일 → Grid → 메뉴 닫기` 계층을 성립시킴 (탭이 Back을 항상 소비하는 문제 회피).
- 캐릭터 스탯 탭은 GAS 어그리게이터 평가식을 따라 스탯을 **합연산 결과 / 곱연산 증가량**으로 분해해 표시. `ULNPStatsViewModel`이 `FText` 필드 1개로 노출하고 `URichTextBlock` 인라인 마크업으로 색을 구분 (MVVM 바인딩 1줄). (→ [TechDesign_InGameMenu.md](TechDesign_InGameMenu.md))
- 인벤토리 탭은 좌측 `CommonTileView` Grid + 우측 디테일 패널 2분할. 아이템 데이터는 MVVM 리스트 바인딩 제약으로 C++가 직접 채움. 메뉴가 열린 동안 폰의 입력 매핑 컨텍스트를 제거해 게임플레이 입력을 전면 차단하고, 스탠드얼론에서만 일시정지.

### World Generation
- Geometry Script로 Octant(1/8 구체) 지형용 Static Mesh 생성 (`BP_OctantGenerator`). `IDetailCustomization` 구현으로 에디터 Detail 뷰 커스터마이징.
- PCG로 프랍 배치한 결과물을 Level Instance로 저장. 게임 실행 시 `ULNPOctantSpawnSubsystem`이 사전 생성된 Octant Level Instance 8개를 랜덤 선택·배치하여 완전한 구체 구성.

### Multiplayer Init Sequence
- `ALNPGameMode`가 `WorldGeneration → SurfaceBaking → EntitySpawning → Complete` 4단계를 순차 진행. `ALNPGameState`가 `ServerPhase`와 `OctantGenSeed`를 Replicate하여 클라이언트와 동기화. (→ [TechDesign_InitSequence.md](TechDesign_InitSequence.md))

### Multiplayer Networking
- Iris Replication + MassReplication 하이브리드. 서버 권위 판정과 클라이언트 코스메틱 예측(HitStop·VFX·몽타주)을 분리하여 치팅 불가 구조 유지.
- 서버 위치 되감기 Lag Compensation(RTT/2, 최대 200ms), Ghost Projectile 클라이언트 예측, 발사체 Spawn-Only 방송 + Dead Reckoning 외삽 스폰 구현.
- 서버 소유 엔티티(Enemy·Player·LootPod)는 Mass bubble + Actor 복제 이중화, `UMassAgentComponent` NetID 핸드셰이크로 클라이언트 퍼펫 링크 자동 성립.
- 세 타입은 **하나의 통합 버블**(`ALNPMassClientBubbleInfo`)을 공유한다 — 엔진의 파괴 처리 경로가 타입을 구분하지 않아 버블이 2개 이상이면 타 타입 엔트리를 자기 핸들로 제거하려다 크래시하기 때문. 자세는 대역폭 추가 없이 **접평면 로컬 Yaw**로 인코딩해 구 내벽에서 엔티티가 눕는 문제를 해소. (→ [TechDesign_Networking.md](TechDesign_Networking.md))

---

## 3. Gameplay Overview

### 3.1 월드 구성 (Dyson Sphere)

- **메인 월드:** 거대한 구체의 내부 표면이 주 플레이 공간. 중력은 구 중심 반대 방향(원심).
- **8분할 Octant:** 전체 구체를 8개 Level Instance로 관리. 결정론적 시드로 게임마다 다른 조합 생성.

### 3.2 핵심 루프: "Find → Loot → Pop!"

- **LootPod** 발견 → 적의 방해를 버티며 루팅 → 루팅 성공 시 무기·버프·스킬 획득.
- HP가 0이 되면 보유 아이템을 모두 드랍하면서 구형 지형 곡률에 따른 포물선 궤적으로 날아감.
- 승리 조건: 메달(가칭) 4개 수집.

### 3.3 난이도 연계

- 남은 LootPod 수가 줄어들수록 적 NPC가 점진적으로 강화. 자연스러운 게임 후반 긴장감 형성.

### 3.4 전투 분위기

- 캐릭터 거대화, 초고속 이동, 광역 넉백 등 **과장된 물리 연출** 지향.
- 패링 성공 시 공격자를 포물선으로 날려버리는 시원한 피드백.

---

## 4. Documentation Index

| 문서 | 내용 |
|:---|:---|
| [개발 계획](DevelopmentPlan.md) | Phase별 구현 현황 및 마일스톤 |
| [폐기된 시도들](DiscardedApproaches.md) | 시도 후 제외된 기술적 접근과 사유 |
| [추가 스펙 아이디어](Idea_Backlog.md) | 정규 스펙으로 채택되기 전 아이디어 후보 모음 |
| [WorldGeneration 기술 설계](TechDesign_WorldGeneration.md) | Octant 8분할 전략, PCG 지형 생성, 결정론적 런타임 스폰 흐름 |
| [Octant Level Instance 제작 가이드](Guide_OctantLevelInstance.md) | Octant Level Instance 에셋 신규 제작 절차 |
| [커스텀 Slate 위젯 제작 가이드](Guide_CustomSlateWidget.md) | 베이스 선택, TSlateAttribute, 스타일 분리, UMG 래퍼 |
| [초기화 시퀀스 기술 설계](TechDesign_InitSequence.md) | 서버/클라 4단계 초기화, 투-게이트 레이스 컨디션 해결, 폰 스폰 게이팅 |
| [표면 캐시 기술 설계](TechDesign_SurfaceCache.md) | 등장방형 그리드 사전 베이킹, Mass 워커 스레드 O(1) 안전 조회, NavMesh 대체 이유 |
| [CharacterMovement 기술 설계](TechDesign_CharacterMovement.md) | 구형 중력 3모드, 곡률 보정, 컨트롤 회전 파이프라인, 카메라 리그 노드 순서 제약, 질주·가드·대시·ADS 시스템 |
| [전투 Animation 기술 설계](TechDesign_CombatAnimation.md) | Motion Matching 로코모션, 무기별 Linked Anim Layer 교체, Aim Offset·왼손 Two Bone IK·Guard 자세 분기, 몽타주 ANS 구간 제어와 경직 차단 소유권 구분, 근접 공격 타겟 보정(Motion Warping) |
| [Ability System 게임 기획](GameDesign_Ability.md) | 무기·스킬·버프 아이템 구조, GAS 슬롯 관리, 합/곱 이원 스텟 체계, 구현 현황 |
| [Ability System 기술 설계](TechDesign_Ability.md) | ASC/AttributeSet 아키텍처, 합/곱 2채널 스탯 파이프라인, 발사체 Mass 프로세서 4종, 어빌리티 클래스 계층 |
| [경직 시스템 게임 기획](GameDesign_Poise.md) | 경직 시스템 — 누적/자연회복 원칙, 그로기·다운 2단계, 딜 구간 비대칭, 가드 브레이크·패링 연계 |
| [경직 시스템 기술 설계](TechDesign_Poise.md) | FLNPPoiseFragment·ULNPPoiseProcessor, 상태 기반 그로기, 폰별 임계값, 유지시간 비례 보너스, 비복제 근거 |
| [Enemy NPC 게임 기획](GameDesign_EnemyNPC.md) | 슬롯 기반 타겟팅, 행동 상태 (Idle/Alert/Chase/Attack), LOD 전환 |
| [Enemy NPC 기술 설계](TechDesign_EnemyNPC.md) | Fragment/Tag 구조, Mass 프로세서 12종, Actor 연동 (High LOD), 넷 모드별 표현 소유권(게스트는 복제 Actor만) |
| [Enemy NPC StateTree 기술 설계](TechDesign_EnemyNPC_StateTree.md) | StateTree 상태 계층 (Combat/Alert/Idle), Evaluator 및 Task C++ 구성 |
| [Enemy NPC Low LOD 전투 기술 설계](TechDesign_EnemyNPC_LowLOD.md) | CombatMode 옵션(Actor 승격/순수 엔티티), 가상 칼날 근접 판정, 행동 상태 1바이트 복제, ISM↔ISKM 인스턴싱 애니메이션 |
| [LootPod System 게임 기획](GameDesign_LootPod.md) | 루팅 흐름, 존 사수(넉백) 취소 조건, 협동 루팅 속도, 보상 유형 |
| [LootPod System 기술 설계](TechDesign_LootPod.md) | MassEntity 구성, Pod 레지스트리 상호작용 탐색, 게이지·보상 드랍·Low LOD 빛기둥 |
| [LootDice System 게임 기획](GameDesign_LootDice.md) | 보상 아이템 주사위 굴림 컨셉, 아이콘 식별, 획득·인벤토리 드랍·소멸 기획 |
| [LootDice System 기술 설계](TechDesign_LootDice.md) | 서버 권위 물리 Actor, Iris FRepMovement 동기화, 구면 중력 AddForce, 획득·드랍 RPC |
| [인벤토리 기술 설계](TechDesign_Inventory.md) | 아이템 인스턴스 모델(UObject+FastArray+등록 서브오브젝트), GameplayTagStack 스탯, 장착/보관 분리, 버프 인스턴스 흐름 |
| [HUD 기술 설계](TechDesign_HUD.md) | MVVM ViewModel 구조, ASC 델리게이트 기반 갱신 흐름, 대시 쿨다운 파이 위젯, 적 HP 바 오버레이 설계 |
| [인게임 메뉴 게임 기획](GameDesign_InGameMenu.md) | 탭 구조, 게임패드 조작, 캐릭터 스탯·인벤토리·환경설정 탭 기획 |
| [인게임 메뉴 기술 설계](TechDesign_InGameMenu.md) | CommonUI 위젯 계층, Back 전파 규칙, 커스텀 힌트 바·입력 글리프, 스탯 합/곱 분해, 로컬라이제이션, PIE 2인 게임패드 라우팅 |
| [HitDetection 기술 설계](TechDesign_HitDetection.md) | 근접 Swept Volume·원거리 Line Segment 판정, 판정 캡슐 중심 규약 단일 헬퍼, 조준 원본 단일화(서버가 클라 조준점을 읽음), 공간 쿼리 최적화 미구현 |
| [멀티플레이 네트워킹 기술 설계](TechDesign_Networking.md) | Iris·MassReplication 하이브리드, Lag Compensation, 클라이언트 예측·Dead Reckoning, 대역폭 예산 규약(상한=안전판·엔티티당 비용·조용한 소실), 엔진 소스 분석 이슈 7건 |
| [네트워크 대역폭 가이드](Guide_NetBandwidth.md) | 비용 3축(버블·승격 Actor·절편), 페이로드 양자화 규약과 int16 월드 반지름 캡, 절제(ablation)와 사유별 계수 측정법·분모 규약, 반복된 실패 패턴 |
| [ParrySystem 게임 기획](GameDesign_ParrySystem.md) | 패링 성공 조건, 투사체 타입별 반사, 플레이어 경험 의도 |
| [ParrySystem 기술 설계](TechDesign_ParrySystem.md) | FLNPParryStateFragment, HitDetection 연계 판정 흐름, Mass-GAS 브릿지 방안 |
