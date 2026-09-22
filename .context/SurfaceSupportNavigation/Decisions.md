# Surface Support·Navigation 확정 결정

> 상태: 확정 결정 원장
> 마지막 갱신: 2026-09-21
> 규칙: 결론만 기록하고 상세 근거와 구현은 링크된 문서가 소유한다.

## 결정 목록

| ID | 결정 | 상태 | 상세 근거 |
|:---|:---|:---|:---|
| D-001 | Mesh Terrain은 우선 런타임 시스템이 아니라 옥탄트 제작 도구로 사용한다. | 확정 | `research/MeshTerrain.md` |
| D-002 | 현재 비-World Partition 옥탄트 Level Instance 구조를 유지한다. | 확정 | `research/MeshTerrain.md` |
| D-003 | Surface 데이터는 머신별 런타임 베이크가 아니라 옥탄트별 에디터 베이크 에셋으로 제공한다. | 확정 | `design/SurfaceBaking.md` |
| D-004 | 부유섬은 필수이며 같은 방사 방향에 여러 Support Layer가 존재할 수 있다. | 확정 | `design/Architecture.md` |
| D-005 | 동굴을 도입하면 Mass NPC도 출입·추격할 수 있어야 한다. | 확정 | `design/MovementIntegration.md` |
| D-006 | 동굴은 우선 단일 corridor와 단순 출입구만 지원하며 복층·수직 통로·Y자 분기는 필수가 아니다. | 확정 | `design/SurfaceBaking.md` |
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

## 변경 규칙

- 확정 결정을 바꿀 때 기존 행을 조용히 덮어쓰지 않는다.
- 상태를 `대체됨`으로 바꾸고 새 ID를 추가한다.
- 변경 이유와 영향을 관련 설계 문서 및 현재 Phase 로그에 기록한다.
- 단순 구현 선택이나 임시 디버그 값은 이 원장에 추가하지 않는다.
