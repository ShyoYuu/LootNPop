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

## [메모 및 낙서장]
- [ ] 소셜 기능용 MVVM 기반 실시간 리더보드 연출.
- [ ] Iris를 활용한 수천 개 파편 데이터 최적화 동기화 실험.