# LootNPop 프로젝트 개요

**LootNPop**은 Unreal Engine 5.7 최신 스택을 활용한 경쾌한 분위기의 멀티플레이어 파티 게임. 거대한 구체의 내부 표면(Dyson Sphere)이 플레이 공간이며, 수백~수천 규모의 MassEntity 적을 상대로 LootPod를 쟁취하는 것이 핵심 루프. 핵심 로직은 가급적 C++로 구현.

---

## 1. 핵심 기술 스택

| 구분 | 기술 스택 | 주요 구현 클래스 |
|:---|:---|:---|
| **Input** | Enhanced Input | `ALNPCharacterBase` |
| **Movement** | Mover Plugin | `ULNPCharacterMoverComponent`, `ULNPPawnGravityComponent` |
| **World** | PCG + Level Instance + World Partition | `UPCGSphereWorldSettings`, `ULNPOctantSpawnSubsystem` |
| **Surface Query** | SurfaceCacheSubsystem (커스텀) | `ULNPSurfaceCacheSubsystem` |
| **Interaction** | SmartObject + MassEntity | `ALNPLootPod`, `ULNPLootingProcessor` |
| **AI/Entity** | MassEntity + StateTree | `ULNPEnemyMovementProcessor`, `ULNPTargetingSubsystem` |
| **Combat** | GAS + MassEntity | (구현 예정) |
| **Networking** | Iris Replication System | (구현 예정) |
| **UI** | MVVM Plugin | (구현 예정) |

---

## 2. 기술적 가이드라인

### 이동 및 중력
- `ULNPPawnGravityComponent`가 `RadialOutward` 모드로 Dyson Sphere 내벽 중력 구현. 위치마다 달라지는 Up 벡터를 매 프레임 컨트롤러 회전에 누적 보정하여 화면 기울어짐 방지.

### 구형 표면 쿼리
- NavMesh는 Z축 고정 설계로 구형 환경에 적용 불가. Mass 프로세서 내 인라인 라인 트레이스는 스레드 안전성 문제로 기각. (→ [DiscardedApproaches.md](DiscardedApproaches.md))
- 대신 게임 시작 전 `ULNPSurfaceCacheSubsystem`이 구형 내벽 전체를 등장방형 그리드로 사전 계산(Baking). `bBakingComplete` 이후 읽기 전용이므로 Mass 워커 스레드에서 O(1) 안전 조회 가능.

### 전투 판정
- 물리 엔진 콜리전 이벤트 미사용. 모든 히트 판정은 `FMassProcessor::Execute()` 내에서 수학적으로 일괄 계산(Swept Volume, Line Segment 거리).

### 대규모 엔티티
- MassEntity + StateTree로 수백~수천 개 NPC를 게임스레드 부담 없이 처리. SmartObject로 상호작용 쿼리, Mass 프로세서로 상태 갱신.

### 멀티플레이어 초기화
- `ALNPGameMode`(서버 전용)가 `WorldGeneration → SurfaceBaking → EntitySpawning → Complete` 4단계를 순차 진행. `ALNPGameState`가 `ServerPhase`와 `OctantGenSeed`를 복제하여 클라이언트와 동기화. (→ [TechDesign_InitSequence.md](TechDesign_InitSequence.md))

---

## 3. 게임플레이 개요

### 3.1 월드 구성 (Dyson Sphere)

- **메인 월드:** 거대한 구체의 내부 표면이 주 플레이 공간. 중력은 구 중심 반대 방향(원심).
- **부유 지형:** 구 내부 공중에 떠 있는 작은 구체형 행성들. 중력은 구 중심 방향(인력). 플레이어는 외부 표면에서 활동.
- **8분할 Octant:** 전체 구체를 8개 Level Instance로 관리. 결정론적 시드로 게임마다 다른 조합 생성.

### 3.2 핵심 루프: "Find → Loot → Pop!"

- **LootPod** 발견 → 적의 방해를 버티며 루팅 → Pod 파열 시 무기·버프·스킬 획득 → 획득한 힘으로 적을 날려버림.
- 피격 시 보유 아이템을 모두 바닥에 드랍. 구형 지형 곡률에 따른 포물선 궤적으로 날아감.
- 승리 조건: 메달(가칭) 4개 수집.

### 3.3 난이도 연계

- 활성 LootPod 수가 줄어들수록 적 NPC가 점진적으로 강화. 자연스러운 게임 후반 긴장감 형성.

### 3.4 전투 분위기

- 캐릭터 거대화, 초고속 이동, 광역 넉백 등 **과장된 물리 연출** 지향.
- 패링 성공 시 공격자를 포물선으로 날려버리는 시원한 피드백.

---

## 4. 상세 문서 인덱스

| 문서 | 내용 |
|:---|:---|
| [개발 계획](DevelopmentPlan.md) | Phase별 구현 현황 및 마일스톤 |
| [폐기된 시도들](DiscardedApproaches.md) | 시도 후 제외된 기술적 접근과 사유 |
| [추가 스펙 아이디어](Idea_Backlog.md) | 정규 스펙으로 채택되기 전 아이디어 후보 모음 |
| [WorldGeneration 기술 설계](TechDesign_WorldGeneration.md) | Octant 분할, PCG 지형 생성, 런타임 스폰 흐름 |
| [초기화 시퀀스 기술 설계](TechDesign_InitSequence.md) | 서버/클라 4단계 초기화, 레이스 컨디션 해결, 폰 스폰 게이팅 |
| [표면 캐시 기술 설계](TechDesign_SurfaceCache.md) | 등장방형 베이킹, Mass 스레드 안전 조회, NavMesh 대체 이유 |
| [CharacterMovement 기술 설계](TechDesign_CharacterMovement.md) | 구형 중력, 곡률 보정, 질주/대시 시스템 |
| [Enemy NPC 게임 기획](GameDesign_EnemyNPC.md) | 슬롯 기반 타겟팅, 행동 상태, LOD 전환 |
| [Enemy NPC 기술 설계](TechDesign_EnemyNPC.md) | Fragment/Tag 구조, Mass 프로세서, StateTree 연동 |
| [LootPod System 게임 기획](GameDesign_LootPod.md) | 루팅 흐름, 취소 조건, 보상 유형 |
| [LootPod System 기술 설계](TechDesign_LootPod.md) | MassEntity 구성, SmartObject 연동, 미구현 항목 |
| [HitDetection 기술 설계](TechDesign_HitDetection.md) | Swept Volume, Line Segment 판정, 공간 쿼리 최적화 |
| [ParrySystem 게임 기획](GameDesign_ParrySystem.md) | 패링 조건, 투사체 반사, 플레이어 경험 의도 |
| [ParrySystem 기술 설계](TechDesign_ParrySystem.md) | Fragment 구조, HitDetection 연계, Mass-GAS 브릿지 |
