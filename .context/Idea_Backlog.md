# LootNPop Idea Backlog

이 문서는 **LootNPop** 프로젝트의 핵심 시스템 구축 이후, 추가 스펙으로 검토할 수 있는 자유로운 아이디어들을 기록합니다.

---

## [AI & 대규모 엔티티 관련]

### 1. 군집 폭주 유도 기믹 (Popcorn Kernel)
- **개요:** 구체 내벽 전역에 주기적으로 스폰되는 거대한 에너지 핵 오브젝트.
- **메커니즘:** 
    - 활성화 시 수백 마리의 `MassEntity`가 스마트 오브젝트(SO) 슬롯을 점유하기 위해 핵으로 쇄도.
    - 적들이 뭉쳐 거대한 덩어리를 형성했을 때 플레이어가 타격하면 일제히 사방으로 사출되는 연출.
- **핵심 기술:** Smart Object(Dynamic Slots), MassEntity, StateTree(Frenzy State).

### 2. 환경 인지 및 우회 시스템 (Hazard Awareness)
- **개요:** 지형 기믹(가스 분출구, 전기 지대)의 상태에 따라 AI가 경로를 동적으로 변경.
- **메커니즘:** 
    - 기믹 오브젝트의 런타임 태그(`State.Danger`)를 감지하여 MassEntity가 해당 구역의 SO 쿼리를 배제함.
- **핵심 기술:** Smart Object Runtime Tags, Mass Processor Query Filtering.

---

## [물리 및 환경 연출 관련]

### 3. 궤도 기반 중력 런처 (Orbital Launcher)
- **개요:** 구형 표면의 곡률을 이용해 캐릭터를 반대편으로 빠르게 사출하는 가속 장치.
- **메커니즘:** 
    - 진입 시 모션 워핑으로 정렬 후, Mover 컴포넌트에 원심력을 고려한 고속 커브 속도 주입.
- **핵심 기술:** Mover Plugin, Motion Warping, Custom Gravity Calculation.


---

## [전투 및 시스템 관련]

### 5. 거대화 및 넉백 체인 (Giant & Chain Reaction)
- **개요:** 특정 버프 획득 시 캐릭터가 거대해지며, 적들을 볼링핀처럼 연쇄적으로 날려버림.
- **메커니즘:** 
    - GAS 어빌리티를 통해 엔티티에 충격 전달 시, 충격을 받은 엔티티가 인접 엔티티에 다시 힘을 전달.
- **핵심 기술:** GAS, MassEntity Neighbor Query.

---

## [월드 순환 관련]

### LootPod 리스폰 (2026-07-12 — 정규 스펙에서 제외)
- **개요:** 특정 구역의 Pod가 모두 소모되면 일정 시간 후 새로운 위치에 리스폰. (기존 [GameDesign_LootPod.md](GameDesign_LootPod.md) §5 스펙이었으나 제외 결정)
- **재검토 시점:** 세션 길이·월드 순환 구조가 확정된 뒤 — 리스폰 없이 "유한한 Pod를 다 까면 라운드 종료" 구조가 될 수도 있어, 게임 루프 확정 전에는 설계 불가.
- **핵심 기술 (검토 시):** ULNPMassSpawnSubsystem 결정론적 스폰 재사용, 구역별 소모 카운터, 리스폰 위치 선정(기존 SurfaceCache 투영).

---

### 적 NPC GAS 버프 지원 (2026-07-27 — 이번 범위에서 제외)
- **개요:** 플레이어와 마찬가지로 적 NPC도 GAS 버프/디버프를 받게 한다. 근처 적에게 거는 슬로우·방어 저하 등.
- **현재 상태 (조사 완료):** `ALNPEnemyCharacter`는 **이미 ASC + `ULNPBaseAttributeSet`을 보유**한다
  (`LNPEnemyCharacter.cpp:26-30`). 그래서 High LOD 적에게 GE를 걸면 지금도 작동한다.
- **막히는 지점:** 적 Actor는 Low↔High LOD 전환마다 **풀에서 스폰/반납**된다. 권위 있는 Health는
  **Mass Fragment**에 있고 High LOD 활성화 때 Fragment→AttributeSet으로 밀어넣는다
  (`AttributeSet->SetHealth(InHealth)`, 139행). 따라서 적 ASC에 건 GE는 **Actor가 풀로 돌아가면 소멸**하고,
  재사용된 ASC가 다른 엔티티에 이전 GE를 흘릴 위험도 있다 (97행 `ClearAllAbilities`는 어빌리티만 지운다).
- **지금도 가능한 것:** High LOD 한정·수 초짜리 단기 효과. 단, 풀 반납 시 활성 GE를 지우는 정리 코드는 필요.
- **작업 필요:** 지속 버프는 버프 상태를 **Mass Fragment에 저장**하고 High LOD 활성화 때 ASC에 재적용해야 한다
  — Health가 이미 하는 것과 동일한 패턴이라 범위는 명확하다.
- **재검토 시점:** 적에게 거는 상태이상(슬로우·방어 저하 등)이 기획으로 확정될 때.

---

## [메모 및 낙서장]
- [ ] 소셜 기능용 MVVM 기반 실시간 리더보드 연출.
- [ ] Iris를 활용한 수천 개 파편 데이터 최적화 동기화 실험.
### LootPod 단말기 상호작용 모션 워핑 (2026-07-10)
- **개요:** LootPod 상호작용(F키) 시 "단말기를 직접 조작한다"는 컨셉에 맞춰, 캐릭터를 단말기 앞 정위치·정방향으로 모션 워핑 정렬 후 조작 애니메이션 재생.
- **메커니즘:**
    - 상호작용 반경이 초근접(단말기 조작 거리)이므로 워핑 이동량이 작아 위화감 없음.
    - Pod 전방(현재 CanInteract 각도 조건의 기준 방향)에 워프 타겟 소켓 배치.
- **핵심 기술:** Motion Warping, 구면 중력 Up 벡터 보정 (기존 Orbital Launcher 아이디어와 동일 계열).
