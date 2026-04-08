# LootNPop 프로젝트 개요

**LootNPop**은 언리얼 엔진의 최신 스택을 활용한 경쾌한 분위기의 멀티플레이어 파티 게임입니다. 대규모 엔티티 처리와 정교한 이동 시스템을 핵심으로 합니다. 가급적 C++을 사용하여 핵심 로직을 구현하는 것을 원칙으로 합니다.

## 1. 핵심 기술 스택
| 구분 | 기술 스택 | 비고 |
| :--- | :--- | :--- |
| **Input** | Enhanced Input | 복합 액션 및 컨텍스트 제어 |
| **Movement** | Mover Plugin | 구형 지형 대응 커스텀 중력 구현 |
| **Combat** | GAS + MassEntity | 수학적 계산 기반 히트 판정 |
| **AI/Entity** | MassEntity + StateTree | 대규모 NPC 및 최적화된 로직 |
| **Network** | Iris Replication | 최신 복제 시스템 활용 |
| **World** | World Partition | 심리스 대형 지형 |
| **UI** | MVVM Plugin | 데이터 중심 UI 바인딩 |

## 2. 기술적 가이드라인 (Implementation Rules)
- Unreal Engine 버전은 5.7.
- **Movement:** Mover Plugin 기반의 Custom Gravity 시스템을 최우선으로 구현.
- **Gravity Logic:** 세부 지형(언덕 등)의 법선이 아닌, **거대 기하학적 구체(Sphere)의 중심점**을 기준으로 중력 방향 결정.
- **Combat:** 물리 엔진(Chaos)을 사용하지 않고 MassEntity 상에서 수학적(Dot Product, Swept Area 등)으로 피격/패링 판정.
- **Individual HitStop:** 개별 엔티티의 `ExecutionSpeed` 프래그먼트 또는 애니메이션 재생 속도를 조정하여 타격감 구현.

## 3. 상세 기획 및 메커니즘

### 3.1 월드 구성 (Inverted Sphere World)
- **메인 월드:** 거대한 구체의 **내부 표면**이 주 플레이 공간입니다. (Dyson Sphere 컨셉)
- **메인 중력:** 구체의 중심에서 바깥쪽 표면을 향해 작용합니다. 플레이어는 구 내부 벽면에 서서 활동합니다.
- **부유 지형:** 구 내부 공중에 떠 있는 작은 구체 형태의 행성들입니다.
- **부유 지형 중력:** 메인 월드와 반대로 구체 중심 방향(지구와 동일)으로 중력이 작용하며, 플레이어는 구 외부 표면에 서서 활동합니다.
- **심리스 환경:** World Partition을 통해 이 거대한 구형 공간 전체를 스트리밍 관리합니다.

### 3.2 게임 플레이 및 상호작용
- **LootPod (SmartObject):** 월드 곳곳(메인 내벽 및 부유 행성)에 랜덤 스폰되는 상호작용 객체입니다. 
- **획득 요소:** 스킬, 무기, 버프 또는 고정형 터렛 탑승 기믹 등을 포함하며, 상호작용 시 '팝' 하고 터지는 연출을 가집니다.
- **승리 조건:** 플레이어들이 '메달(가칭)' 4개를 모두 수집하면 승리.

### 3.3 대규모 AI 및 방해 기믹
- **MassEntity 적 NPC:** 수백~수천 단위의 대규모 적 유닛을 StateTree로 제어. 구형 지형에 맞춰 이동 로직 구현.
- **난이도 스케일링:** 월드에 남은 **LootPod**의 수가 줄어들수록 적 NPC가 점진적으로 강화됨.

### 3.4 전투 및 아이템 드랍
- **피격 효과:** 적에게 피격 시 물리 충돌(Launch)로 인해 멀리 튕겨 나가는 과장된 연출. (구형 지형의 곡률에 따른 궤적 발생)
- **드랍 시스템:** 피격 시 보유한 무기 및 버프를 모두 바닥에 드랍.

## 4. 게임 톤앤매너
- 유쾌하고 신나는 분위기의 멀티플레이어 파티 게임.
- 캐릭터 거대화, 초고속 이동 등 과장되고 재미있는 능력 위주의 플레이 지향.

## 99. 상세 문서 인덱스
- [개발 계획 (Milestone)](@DevelopmentPlan.md)
- [WorldGeneration 기술 설계](@TechDesign_WorldGeneration.md)
- [LootPod System 게임 기획](@GameDesign_LootPod.md)
- [LootPod System 기술 설계](@TechDesign_LootPod.md)
- [HitDetection 기술 설계](@TechDesign_HitDetection.md)
- [ParrySystem 게임 기획](@GameDesign_ParrySystem.md)
- [ParrySystem 기술 설계](@TechDesign_ParrySystem.md)