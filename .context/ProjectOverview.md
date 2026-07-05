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
| **Camera** | Gameplay Camera | — |
| **World** | PCG + Geometry Script + Level Instance | `BP_OctantGenerator`, `UPCGOctantThemeSamplerSettings`, `ULNPOctantSpawnSubsystem` |
| **Surface Query** | SurfaceCacheSubsystem (커스텀) | `ULNPSurfaceCacheSubsystem` |
| **Interaction** | SmartObject + MassEntity | `ALNPLootPod`, `ULNPLootingProcessor` |
| **AI/Entity** | MassEntity + StateTree | `ULNPEnemyMovementProcessor`, `ULNPTargetingSubsystem` |
| **Combat** | GAS + MassEntity | `ULNPEquipmentComponent`, `ULNPAbility_RangedAttack`, `ULNPProjectileHitDetectionProcessor` |
| **Networking** | Iris Replication System | (구현 예정) |
| **UI** | MVVM Plugin | `ULNPHudViewModel`, `ULNPHudWidget` |

---

## 2. Core Systems

### Custom Gravity
- Mover 기능을 활용하여 `Fixed`, `RadialInward`, `RadialOutward` 3가지 중력 모드 구현.
- `ULNPPawnGravityComponent`가 `RadialOutward` 모드로 Dyson Sphere 내벽 중력 적용. Pawn 위치마다 달라지는 Up/Gravity Direction을 매 프레임 갱신하여 화면 기울어짐 방지.

### Enemy NPC
- 모든 NPC는 MassEntity(Low LOD) ↔ Actor(High LOD) 하이브리드 구조. 전투 진입(Confirmed/Combat) 시 `ULNPEnemyLODOverrideProcessor`가 High LOD Actor로 자동 전환.
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
- Motion Matching 기반 Locomotion에 무기별 Linked Anim Layer를 블렌딩하여 3종 무기(Pistol·Rifle·LongSword) 구현.
- Pistol·Rifle은 Aim Offset 적용. 발사 직전 카메라 방향 LineTrace로 화면 중앙 조준점을 검출하여 해당 방향으로 Projectile 발사.

### UMG MVVM Plugin
- UMG MVVM Plugin 기반으로 C++ ViewModel(`ULNPHudViewModel`)과 View Binding을 분리. ASC 델리게이트로 속성 변화를 ViewModel에 바인딩하고 Blueprint Widget이 이를 구독.

### World Generation
- Geometry Script로 Octant(1/8 구체) 지형용 Static Mesh 생성 (`BP_OctantGenerator`). `IDetailCustomization` 구현으로 에디터 Detail 뷰 커스터마이징.
- PCG로 프랍 배치한 결과물을 Level Instance로 저장. 게임 실행 시 `ULNPOctantSpawnSubsystem`이 사전 생성된 Octant Level Instance 8개를 랜덤 선택·배치하여 완전한 구체 구성.

### Multiplayer Init Sequence
- `ALNPGameMode`가 `WorldGeneration → SurfaceBaking → EntitySpawning → Complete` 4단계를 순차 진행. `ALNPGameState`가 `ServerPhase`와 `OctantGenSeed`를 Replicate하여 클라이언트와 동기화. (→ [TechDesign_InitSequence.md](TechDesign_InitSequence.md))

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
| [초기화 시퀀스 기술 설계](TechDesign_InitSequence.md) | 서버/클라 4단계 초기화, 투-게이트 레이스 컨디션 해결, 폰 스폰 게이팅 |
| [표면 캐시 기술 설계](TechDesign_SurfaceCache.md) | 등장방형 그리드 사전 베이킹, Mass 워커 스레드 O(1) 안전 조회, NavMesh 대체 이유 |
| [CharacterMovement 기술 설계](TechDesign_CharacterMovement.md) | 구형 중력 3모드, 곡률 보정, 질주·가드·대시 시스템 |
| [전투 Animation 기술 설계](TechDesign_CombatAnimation.md) | Motion Matching 로코모션, 무기별 Linked Anim Layer 교체, Aim Offset·상하체 블랜딩·Guard 자세 분기 구현 완료 |
| [Ability System 게임 기획](GameDesign_Ability.md) | 무기·스킬·버프 아이템 구조, GAS 슬롯 관리, 구현 현황 |
| [Ability System 기술 설계](TechDesign_Ability.md) | ASC/AttributeSet 아키텍처, 발사체 Mass 프로세서 4종, 어빌리티 클래스 계층 상세 |
| [Enemy NPC 게임 기획](GameDesign_EnemyNPC.md) | 슬롯 기반 타겟팅, 행동 상태 (Idle/Alert/Chase/Attack), LOD 전환 |
| [Enemy NPC 기술 설계](TechDesign_EnemyNPC.md) | Fragment/Tag 구조, Mass 프로세서 9종, Actor 연동 (High LOD) |
| [Enemy NPC StateTree 기술 설계](TechDesign_EnemyNPC_StateTree.md) | StateTree 상태 계층 (Combat/Alert/Idle), Evaluator 및 Task C++ 구성 |
| [LootPod System 게임 기획](GameDesign_LootPod.md) | 루팅 흐름, 취소 조건, 보상 유형 |
| [LootPod System 기술 설계](TechDesign_LootPod.md) | MassEntity 구성, SmartObject 연동, 게이지·인터럽션·보상 미구현 상세 |
| [HUD 기술 설계](TechDesign_HUD.md) | MVVM ViewModel 구조, ASC 델리게이트 기반 갱신 흐름, Blueprint 바인딩 설정 |
| [HitDetection 기술 설계](TechDesign_HitDetection.md) | 근접 Swept Volume·원거리 Line Segment 판정 (근접·원거리·패링 연계 구현 완료, 공간 쿼리 최적화 미구현) |
| [Iris 기반 멀티플레이 설계](TechDesign_Networking.md) | 설계 원칙, MassEntity 네트워크 분류, 시스템별 복제 방안, 7단계 구현 계획 |
| [ParrySystem 게임 기획](GameDesign_ParrySystem.md) | 패링 성공 조건, 투사체 타입별 반사, 플레이어 경험 의도 |
| [ParrySystem 기술 설계](TechDesign_ParrySystem.md) | FLNPParryStateFragment, HitDetection 연계 판정 흐름, Mass-GAS 브릿지 방안 |
