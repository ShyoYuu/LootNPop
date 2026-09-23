# Surface Support·Navigation 확정 결정

> 상태: 확정 결정 원장
> 마지막 갱신: 2026-09-23
> 규칙: 결론만 기록하고 상세 근거와 구현은 링크된 문서가 소유한다.

## 결정 목록

| ID | 결정 | 상태 | 상세 근거 |
|:---|:---|:---|:---|
| D-001 | Mesh Terrain은 우선 런타임 시스템이 아니라 옥탄트 제작 도구로 사용한다. | 확정 | `research/MeshTerrain.md` |
| D-002 | 현재 비-World Partition 옥탄트 Level Instance 구조를 유지한다. | 확정 | `research/MeshTerrain.md` |
| D-003 | Surface 데이터는 머신별 런타임 베이크가 아니라 옥탄트별 에디터 베이크 에셋으로 제공한다. | 확정 | `design/SurfaceBaking.md` |
| D-004 | 부유섬은 필수이며 같은 방사 방향에 여러 Support Layer가 존재할 수 있다. | 확정 | `design/Architecture.md` |
| D-005 | 동굴을 도입하면 Mass NPC도 출입·추격할 수 있어야 한다. | 확정 | `design/MovementIntegration.md` |
| D-006 | 동굴은 우선 단일 corridor와 단순 출입구만 지원하며 복층·수직 통로·Y자 분기는 필수가 아니다. | 대체됨(D-035) | `design/SurfaceBaking.md` |
| D-007 | 정밀 월드 충돌은 기존 Chaos cooked collision과 scene query를 사용한다. | 확정 | `design/RuntimeCollision.md` |
| D-008 | 별도 BVH는 scene query 병목을 입증하기 전에는 추가하지 않는다. | 확정 | `research/ChaosSceneQueries.md` |
| D-009 | 경로 탐색은 Support Atlas와 별도 해상도의 Tiled Nav Grid를 사용한다. | 확정 | `design/GroundNavigation.md` |
| D-010 | 일반 A*를 먼저 구현하되 stable node ID, tile, portal, revision으로 계층형 탐색 확장을 준비한다. | 확정 | `design/GroundNavigation.md` |
| D-011 | 지상 NPC의 traversal은 Walk만 지원한다. | 확정 | `design/GroundNavigation.md` |
| D-012 | 섬 간 추격은 자연 지형·다리·완전히 정지한 기둥 등 실제 보행면이 연결될 때만 허용한다. | 확정 | `design/DynamicTerrain.md` |
| D-013 | NPC는 움직이는 패널을 계획적으로 이용하지 않지만 우연히 착지하면 패널과 함께 이동할 수 있다. | 확정 | `design/DynamicTerrain.md` |
| D-014 | 파괴 잔해는 새 보행면을 만들지 않는다. 파괴는 기존 길이나 blocker를 열고 닫을 수 있다. | 확정 | `design/DynamicTerrain.md` |
| D-015 | 정해진 안정 상태에 도달한 기둥·기믹 다리는 새 Traversal Link를 열 수 있다. | 확정 | `design/DynamicTerrain.md` |
| D-016 | 다른 정적 NavComponent에 착지한 NPC는 가장 가까운 도달 가능한 활성 Pod로 재귀속한다. | 확정 | `design/MovementIntegration.md` |
| D-017 | 타게팅은 Nav 연결 여부와 분리한다. LoS와 사거리가 맞으면 다른 섬의 원거리 NPC도 사격할 수 있다. | 확정 | `design/MovementIntegration.md` |
| D-018 | 전술 사격 위치 탐색은 필수가 아니며 플레이테스트 문제가 있을 때만 검토한다. | 확정 | `design/MovementIntegration.md` |
| D-019 | 완전 비행 NPC는 지상 Nav와 별도 도메인에서 3D steering/local planner로 시작한다. | 확정 | `design/MovementIntegration.md` |
| D-020 | 투사체는 exact segment query 기준선을 먼저 만들고 Support collision horizon은 후속 최적화로 둔다. | 확정 | `design/RuntimeCollision.md` |
| D-021 | 지형 의미는 GameplayTag나 Actor 종류가 아니라 source `UPrimitiveComponent`의 역할·수명주기 Component Tag 조합으로 authoring한다. | 확정 | `design/TerrainContract.md` |
| D-022 | Support source 검증과 정밀 world query는 각각 `LNPSurfaceSupport`, `LNPWorldExact` trace channel을 사용하며 기존 소비자는 단계적으로 전환한다. | 확정 | `design/TerrainContract.md` |
| D-023 | 모든 후속 Phase의 공통 기능 검증은 `/Game/Maps/SurfaceNavigation/L_SurfaceRegression`의 고정 fixture와 oracle을 재사용한다. | 확정 | `design/RegressionMap.md` |
| D-024 | 옥탄트 지형의 기본 제작 경로는 비-WP 일반 Static Mesh + Sphere Height Sculpt로 한다. WP Mesh Terrain은 Boolean 등 고유 modifier가 필요한 특수 제작의 보조 경로로만 사용하고, runtime에는 독립 Static Mesh와 component metadata만 전달한다. | 확정 | `research/MeshTerrain.md` |
| D-025 | 지면 조회는 Support snapshot이 기본이다. exact scene query는 캐시가 표현하지 못하는 대상, risk·edge 결과, 상태 전환 순간에만 사용하며, 플레이어에게서 먼 개체는 risk 구간에서도 거친 지지면을 쓰고 정확성 필수 전환만 exact로 처리한다. PureEntity를 포함한 Mass worker는 동기 scene query만, 게임 스레드는 결과가 필요한 시점에 따라 동기·비동기를 선택한다. | 확정 | `design/RuntimeCollision.md` |
| D-026 | 옥탄트 LVI에는 정적 지형과 배치 마커만 둔다. 움직이는 패널·상태형 기둥·파괴 조각·훅 앵커 같은 동적 요소는 서버가 마커 위치에 스폰하는 복제 Actor이며 런타임 식별자는 `(slot, MarkerId)`다. | 확정 | `design/DynamicTerrain.md` |
| D-027 | 동적 지형의 움직임은 복제된 경로·시작 시각과 서버 시간의 결정론적 함수로 계산한다. 동적 지형을 물리 시뮬레이션으로 구동하지 않는다. | 확정 | `design/DynamicTerrain.md` |
| D-028 | 파괴 바닥·상태형 기둥이 여는 Support·Nav·Link는 마커 로컬 공간의 Conditional Patch로 에디터에서 미리 베이크하고, Runtime Overlay는 patch 활성화와 지역 revision만 담당한다. | 확정 | `design/DynamicTerrain.md` |
| D-029 | SurfaceData stale 검출은 cook·CI 단계의 차단 오류로 수행한다. 런타임은 source hash를 재계산하지 않고 `DataVersion`과 Level 식별만 확인한다. | 확정 | `design/Architecture.md` |
| D-030 | 옥탄트 이음매는 기준 반지름에 고정된 단일 대칭 프로필이다. 부유섬·동굴·마커 영향 범위는 옥탄트 경계를 넘지 않는다. | 확정 | `design/TerrainContract.md` |
| D-031 | 모든 Phase 완료 조건에 `-game` 리슨 서버 2P 스모크를 포함한다. | 확정 | `Roadmap.md` |
| D-032 | Support 캐시 도입 전에 exact 전용 부유섬 프로토타입으로 플레이 감각과 exact 한계치를 실측하고, 캐시 도입 뒤 같은 시나리오로 한계치 증가를 다시 측정한다. | 확정 | `Roadmap.md` |
| D-033 | 대규모 추격 경로는 목표별 flow field와 계층형 A*를 모두 구현하고 같은 시나리오에서 실측 비교해 채택한다. | 확정 | `design/GroundNavigation.md` |
| D-034 | 베이커 핵심 계산은 collision 삼각형을 입력으로 받는 순수 함수로 runtime 모듈에 둔다. source 수집·저장·검증 UI는 Editor 모듈이 담당한다. | 확정 | `design/SurfaceBaking.md` |
| D-035 | 지하 공간은 반구·직육면체 같은 재사용 공동 모듈과 통로 조각의 키트로 만들고, 옥탄트에는 공동으로 들어가는 통로 1~2개만 뚫는다. 복층·수직 통로·분기는 지원하지 않는다. | 확정 | `design/TerrainContract.md` |

## 변경 규칙

- 확정 결정을 바꿀 때 기존 행을 조용히 덮어쓰지 않는다.
- 상태를 `대체됨`으로 바꾸고 새 ID를 추가한다.
- 변경 이유와 영향을 관련 설계 문서 및 현재 Phase 로그에 기록한다.
- 단순 구현 선택이나 임시 디버그 값은 이 원장에 추가하지 않는다.
