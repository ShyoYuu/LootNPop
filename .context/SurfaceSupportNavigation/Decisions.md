# Surface Support·Navigation 확정 결정

> 상태: 확정 결정 원장
> 마지막 갱신: 2026-11-17
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
| D-011 | 지상 NPC의 traversal은 Walk만 지원한다. 적용 범위는 `GroundSupport` 도메인이다(D-066). | 확정 | `design/GroundNavigation.md` |
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
| D-023 | 모든 후속 Phase의 공통 기능 검증은 `/Game/Maps/SurfaceNavigation/L_SurfaceRegression`의 고정 fixture와 oracle을 재사용한다. | 대체됨(D-056) | `design/RegressionMap.md` |
| D-024 | 옥탄트 지형의 기본 제작 경로는 비-WP 일반 Static Mesh + Sphere Height Sculpt로 한다. WP Mesh Terrain은 Boolean 등 고유 modifier가 필요한 특수 제작의 보조 경로로만 사용하고, runtime에는 독립 Static Mesh와 component metadata만 전달한다. | 확정 | `research/MeshTerrain.md` |
| D-025 | 지면 조회는 Support snapshot이 기본이다. exact scene query는 캐시가 표현하지 못하는 대상, risk·edge 결과, 상태 전환 순간에만 사용하며, 플레이어에게서 먼 개체는 risk 구간에서도 거친 지지면을 쓰고 정확성 필수 전환만 exact로 처리한다. PureEntity를 포함한 Mass worker는 동기 scene query만, 게임 스레드는 결과가 필요한 시점에 따라 동기·비동기를 선택한다. | 확정 | `design/RuntimeCollision.md` |
| D-026 | 옥탄트 LVI에는 정적 지형과 배치 마커만 둔다. 움직이는 패널·상태형 기둥·파괴 조각·훅 앵커 같은 동적 요소는 서버가 마커 위치에 스폰하는 복제 Actor이며 런타임 식별자는 `(slot, MarkerId)`다. | 확정 | `design/DynamicTerrain.md` |
| D-027 | 동적 지형의 움직임은 복제된 경로·시작 시각과 서버 시간의 결정론적 함수로 계산한다. 동적 지형을 물리 시뮬레이션으로 구동하지 않는다. | 확정 | `design/DynamicTerrain.md` |
| D-028 | 파괴 바닥·상태형 기둥이 여는 Support·Nav·Link는 마커 로컬 공간의 Conditional Patch로 에디터에서 미리 베이크하고, Runtime Overlay는 patch 활성화와 지역 revision만 담당한다. | 확정 | `design/DynamicTerrain.md` |
| D-029 | SurfaceData stale 검출은 cook·CI 단계의 차단 오류로 수행한다. 런타임은 source hash를 재계산하지 않고 `DataVersion`과 Level 식별만 확인한다. | 확정 | `design/Architecture.md` |
| D-030 | 옥탄트 이음매는 기준 반지름에 고정된 단일 대칭 프로필이다. 부유섬·동굴·마커 영향 범위는 옥탄트 경계를 넘지 않는다. | 확정 | `design/TerrainContract.md` |
| D-031 | 모든 Phase 완료 조건에 `-game` 리슨 서버 2P 스모크를 포함한다. | 확정 | `Roadmap.md` |
| D-032 | Support 캐시 도입 전에 exact 전용 부유섬 프로토타입으로 플레이 감각과 exact 한계치를 실측하고, 캐시 도입 뒤 같은 시나리오로 한계치 증가를 다시 측정한다. | 확정 | `Roadmap.md` |
| D-033 | 대규모 추격 경로는 목표별 flow field와 계층형 A*를 모두 구현하고 같은 시나리오에서 실측 비교해 채택한다. | 대체됨(D-044) | `design/GroundNavigation.md` |
| D-034 | 베이커 핵심 계산은 collision 삼각형을 입력으로 받는 순수 함수로 runtime 모듈에 둔다. source 수집·저장·검증 UI는 Editor 모듈이 담당한다. | 확정 | `design/SurfaceBaking.md` |
| D-035 | 지하 공간은 반구·직육면체 같은 재사용 공동 모듈과 통로 조각의 키트로 만들고, 옥탄트에는 공동으로 들어가는 통로 1~2개만 뚫는다. 복층·수직 통로·분기는 지원하지 않는다. | 대체됨(D-071) | `design/TerrainContract.md` |
| D-036 | `LNPWorldExact` 소비자 전환 전에 production 지형의 exact collision response를 먼저 마이그레이션한다. Surface 베이크용 Component Tag 전환은 Phase 4에 남길 수 있지만 exact response 마이그레이션은 Phase 3 Gate -1이다. | 확정 | `design/TerrainContract.md` |
| D-037 | worker exact hit는 UObject를 역참조하지 않고 사전 게시된 hit identity registry로 의미를 해석한다. 키는 component 하나가 아니라 가능한 경우 shape/component identity, face index, ISM instance index를 포함하며 Surface Layer·DynamicSupport ID로 변환한다. | 확정 | `design/RuntimeCollision.md` |
| D-038 | 원거리 grounded 개체는 risk·edge 결과를 거친 지지면으로 임의 통과하지 않는다. 안전한 Nav cell 내부에서만 coarse support를 쓰고, 불확실한 경계는 Nav edge로 차단하거나 실제 전환 프레임에 exact를 수행한다. | 확정 | `design/RuntimeCollision.md` |
| D-039 | Support-only proxy는 대응 exact geometry와 명시적으로 연결된 경우에만 playable Support로 허용한다. Destructible collision profile은 Support-only, Blocker-only, Support+Blocker 의미를 구분한다. | 확정 | `design/TerrainContract.md` |
| D-040 | 지상 A* 기본 휴리스틱은 모든 Walk Link에서 하한임이 보장되는 3D chord distance를 사용한다. 다중 프레임 요청과 cache는 snapshot generation, connectivity graph version, 지역 revision, agent/cost profile로 검증한다. | 확정 | `design/GroundNavigation.md` |
| D-041 | Conditional Patch는 임의 로컬 Nav Grid를 런타임에 병합하지 않는다. 배치된 마커별로 base Atlas·Nav Tile에 대한 활성화 데이터와 명시적 edge를 미리 베이크하며 marker authoring 전체를 stale hash에 포함한다. | 확정 | `design/DynamicTerrain.md` |
| D-042 | 완전 비행 NPC Phase 3c는 Phase 3 exact query 뒤 시작할 수 있는 병렬 분기이며 Phase 4a의 선행 조건이 아니다. | 확정 | `Roadmap.md` |
| D-043 | 옥탄트 definition 선택은 slot 순서 greedy가 아니라 결정론적 제약 할당으로 현재 batch의 고유 definition 수를 최대화한다. SurfaceData 게시 전에는 seam signature와 계산 seam hash 호환성도 검증한다. | 확정 | `design/DataModel.md` |
| D-044 | 대규모 추격 경로는 동일 benchmark harness에서 목표별 flow field와 계층형 A*의 최소 기능 프로토타입을 비교하고, 채택 기준을 통과한 방식만 production 수준으로 통합한다. | 확정 | `design/GroundNavigation.md` |
| D-045 | 서버가 런타임에 배치하지만 스폰 뒤 transform·형상이 변하지 않는 장치(훅 앵커·스프링 런처)는 월드 의미상 정적이다. `Static` 수명주기와 `LNPStatic*` profile을 쓰고 Runtime Overlay revision을 만들지 않는다. | 확정 | `design/TerrainContract.md` |
| D-046 | Mesh Terrain으로 만드는 부유섬·동굴 옥탄트부터 기준 지각 반지름을 30,000cm로 올린다. 이음매가 기준 반지름에 고정되므로(D-030) 한 월드의 모든 slot은 같은 반지름이어야 하며, 25,000cm 옥탄트와 섞지 않는다. | 확정 | `design/TerrainContract.md` §7 |
| D-047 | Mass 엔티티 기반이고 Actor가 LOD로 승격되는 필드 상호작용 오브젝트는 엔티티 수명에 묶인 collision proxy ISM(단순 캡슐, `LNPStaticBlocker`)을 서버·클라이언트가 각자 유지한다. 승격 Actor는 충돌을 갖지 않는다. LootPod가 기준 구현이다. | 확정 | `design/TerrainContract.md` §2 |
| D-048 | 부유섬은 옥탄트 LVI 안에 레벨 디자이너가 배치하는 옥탄트의 일부다. 옥탄트와 섬을 따로 만들어 런타임에 조합하지 않는다. 섬은 옥탄트 경계면에 걸치지 않고, 기본 보행면은 월드 구와 중심을 공유하는 구면 곡면이다. | 확정 | `phases/Phase03b_ExactFloatingIslandPrototype.md` §3.1 |
| D-049 | PureEntity exact 이동은 접지 시 수평 capsule sweep + 하향 support probe, 공중 시 이전→제안 위치 capsule sweep이다. 걸을 수 없는 hit는 미끄러지고 Unknown hit에는 착지하지 않는다. Phase 6 exact 폴백으로 재사용한다. | 확정 | `phases/Phase03b_ExactFloatingIslandPrototype.md` §3.3 |
| D-050 | 움직이는 패널 Actor 틱을 Mass PrePhysics 페이즈 tick function의 선행 조건으로 걸어, PrePhysics exact query가 패널 자세 갱신 뒤에 돌게 한다. 보장이 안 되면 이동 프로세서를 PostPhysics로 옮기는 안을 별도 세션에서 검토한다. | 확정 | `phases/Phase03b_ExactFloatingIslandPrototype.md` §3.5 |
| D-051 | 섬과 지각을 잇는 경사로는 별도 메시와 지각 일체형 언덕 두 방법을 모두 허용하고 옥탄트마다 고른다. | 확정 | `design/TerrainContract.md` §5 |
| D-052 | 완전 비행 NPC 1차는 PureEntity 원거리 사격형이다. 같은 `ULNPEnemyTrait`에 `NavigationDomain`과 비행 태그를 더하고, 이동만 별도 비행 이동 프로세서가 맡는다. 발사 전 LoS를 검사한다. | 확정 | `phases/Phase03c_FlyingNpcFoundation.md` §3.1·§3.2 |
| D-053 | 비행 NPC 고도는 교전 중 타겟 상대 대역, 비교전 중 Home 기준 대역이다. | 확정 | `phases/Phase03c_FlyingNpcFoundation.md` §3.4 |
| D-054 | 비행 적 총수는 200이다. Pod 편성 타입을 나눠 비행 적이 없는 Pod와 3~6기를 편성한 Pod를 둔다. 비행 100마리당 서버 프레임 약 1.5ms로 본다. 이 총수는 활동 대역이 없는 조건의 값이며 Phase 13 뒤 재측정한다(D-069). | 대체됨(D-074) | `history/Phase03c_Log.md` 2026-09-27 |
| D-055 | 옥탄트의 지각 Layer는 세 이음매 변 모두에 닿는 유일한 `Support+Static` 컴포넌트로 식별한다. 새 태그를 두지 않으며, 해당 컴포넌트가 0개나 2개 이상이면 베이크 오류다. | 확정 | `phases/Phase04a_CrustAtlasAndSeams.md` §3.2 |
| D-056 | 공통 기능 검증의 정적 사례는 fixture LVI `LVI_Octant_Fixture_Regression`을 8 slot 회전으로 합성해 검사하고, LVI에 둘 수 없는 동적 사례만 `L_SurfaceRegression` 일반 레벨에 둔다. 두 곳의 fixture와 oracle을 모든 후속 Phase가 재사용한다. | 확정 | `design/RegressionMap.md` §1 |
| D-057 | 섬·동굴 sparse Atlas는 지각과 같은 octahedral 격자 계열(분할 수 = 지각 N의 정수배)을 쓰고, Layer마다 행별 i 구간(row span)만 저장한다. | 확정 | `phases/Phase04b_MultiLayerSupport.md` §3.2 |
| D-058 | ISM·HISM Support와 Support-only proxy(D-039)는 콘텐츠가 필요해질 때까지 구현하지 않고 베이크 오류로 막는다. Ridge PCG 바위도 Blocker 전용으로 둔다. 해제에는 instance별 source와 볼록 충돌 Support(볼록 hit은 FaceIndex가 없음)가 함께 필요하고, 얻는 것은 낮은 바위 위로 올라서는 것뿐이다. | 확정(2026-11-16 Phase16 2단계 조사 후 재확인) | `phases/Phase04b_MultiLayerSupport.md` §3.1, `phases/Phase16_SupportlessBlockerStep.md` §4.1 |
| D-059 | Mass 초기 스폰의 총량은 `DA_MassSpawnConfig`가 소유한다. LVI의 수동 Pod 세트 앵커를 Spawn stream에 베이크해 먼저 소비하고, 부족분만 베이크된 절차 후보로 채운다. 수동 point 수는 총량을 늘리지 않는다. | 확정 | `phases/Phase05_RuntimeLoader.md` §3.3·§3.4 |
| D-060 | 이음매 줄 Nav node와 이음매 방향 edge는 양쪽 slot 사본이 모두 통과한 경우에만 유효하다. 각 옥탄트 베이크는 이웃 지오메트리를 보지 못하므로 사본 간 clearance 차이는 게시 실패가 아니라 막힘으로 합치고 snapshot에 기록한다. 지면 반지름·법선·해상도·agent profile 불일치는 계속 게시 실패다. 이음매 근처 정적 배치 금지는 기술 제한이 아닌 콘텐츠 규칙이며, 동적 스폰 오브젝트는 이음매 위에도 둘 수 있다. | 확정 | `phases/Phase07a_NavDataFoundation.md` §3.4 |
| D-061 | 스폰 뒤 움직이지 않는 Pod가 덮는 Nav node는 Phase 7b의 최소 runtime overlay로 막는다. 스폰 때 막고 Pod 소멸 때 풀며 Tile revision으로 경로를 다시 계획한다. 이음매 위 Pod는 양쪽 사본을 함께 막는다. overlay는 A*만 막고 ReachabilityGroup은 바꾸지 않는다. Phase 8은 같은 overlay를 확장한다. | 확정 | `phases/Phase07b_PathExecution.md` §3.6 |
| D-062 | 경로·도달성 스냅은 시작·목표 각각 가장 가까운 walkable node를 우선한다. 두 node의 ReachabilityGroup이 다를 때만 양쪽 스냅 반경 안에서 공통 group의 (시작, 목표) 짝을 스냅 거리 합 최소로 다시 고르고, 없으면 A* 없이 도달 불가다. component 크기 기준값은 두지 않으며 근접 슬롯 도달성과 Pod 재귀속도 같은 규칙을 쓴다. | 확정 | `phases/Phase07b_PathExecution.md` §3.3 |
| D-063 | 도달 불가 대상의 지상 적은 슬롯을 받지 않지만 제자리 대신 접근점(자기 ReachabilityGroup의 6방향 내부 node 중 목표에 가장 가까운 node, 초기 반경 3,000cm)까지 경로로 이동한다. 근접은 Alert로 서고, 원거리는 사거리에 들거나 접근점에 닿을 때까지 이동한다. 상태 머신·슬롯·Alert 인내 규칙은 바꾸지 않는다. | 확정 | `design/MovementIntegration.md` "근접 NPC", `phases/Phase07b_PathExecution.md` §3.3·§3.8 |
| D-064 | 한 Support source의 walkable 연결 성분이 같은 방향에서 자기 위를 덮으면(접힌 sheet) 베이커가 서로 겹치지 않는 sub-sheet로 자동 분할하고 각각을 Layer로 둔다. 콘텐츠 메시를 수동으로 쪼개거나 분할 툴을 만들지 않는다. 지각 overhang은 계속 베이크 오류다. | 확정(Phase 7c 단위 1 구현 완료) | `design/SurfaceBaking.md` "접힌 sheet 자동 분할" |
| D-065 | 서로 다른 Layer component 쌍 사이 portal은 쌍마다 하나가 아니라 최소 간격을 둔 여러 개를 저장한다. 입구가 여럿인 건물·계단이 둘인 층에서 A*가 한 연결로만 우회하지 않게 한다. | 확정(Phase 7c 단위 2 구현 완료) | `design/GroundNavigation.md` |
| D-066 | 벽 타기 NPC는 `SurfaceCrawl` 이동 도메인이다. Support·Nav를 쓰지 않고 exact 표면(벽·천장·섬 밑면 포함)을 기어 다니며, 스스로 점프하지 않는다. 넉백 낙하로는 어디든 착지할 수 있다. 공격은 원거리 투사체만, 인지 시야각은 360°다. 재귀속은 Home 쪽으로 기어가다 일정 시간 가까워지지 않으면 직선거리로 가장 가까운 활성 Pod로 바꾼다. D-011은 `GroundSupport` 도메인에만 적용된다. | 확정(Phase 14 구현) | `design/MovementIntegration.md` "벽 타기 NPC" |
| D-067 | Pod에 귀속된 PureEntity는 Pod 단위 활동 대역(휴면·배회·활성)을 가진다. 휴면은 복제 컬 거리 바깥에서 제자리에 멈추고 이동·분리·배회·인지 비용을 내지 않는다. 플레이어 접근·무리 피격·Pod 루팅이 Pod 무리 전체를 깨운다. Pod에 귀속되지 않은 NPC(향후 보스)는 항상 활성이며 서버가 시뮬레이션한다. 대역 경계는 상수가 아니라 기존 거리에서 유도한다. 휴면 경계는 적 복제 컬 거리에 여유를 더한 값(현재 약 130m), 활성 경계는 적 Actor 스폰 거리(현재 60m)다. | 확정(Phase 13 구현) | `design/MovementIntegration.md` "활동 대역" |
| D-068 | 배회 대역의 이동은 서버가 정한 구간(다음 지점·도착 시각·정지 시간)을 구간 시작 때 한 번 복제하고 서버·클라이언트가 같은 보간으로 위치를 계산한다. 구간 중에는 위치 갱신을 보내지 않는다. 클라이언트가 경로까지 생성하는 완전 결정론은 측정에서 대역폭 문제가 확인될 때만 도입한다. 벽 타기 NPC는 같은 표면 안의 짧은 구간과 긴 정지로 같은 방식을 쓴다. | 확정(Phase 13 구현) | `design/MovementIntegration.md` "구간 배회 복제" |
| D-069 | 비행 진형(리더만 복제하고 멤버 위치를 추론)은 채택하지 않는다. 멤버별 락온·HP·넉백을 유지하려면 전투 규칙 전반에 예외가 생기며, 비용 절감은 D-067·D-068이 맡는다. D-054 총수는 Phase 13 뒤 재측정해 새 결정으로 대체한다. | 기각 | `../DiscardedApproaches.md` [Case 06] |
| D-070 | 입체 지형 레벨 디자인 규칙: 계단의 충돌은 계단 전체를 덮는 경사로 형상 메시(`Support+Blocker+Static`)가 맡고 보이는 계단 칸은 `Decoration`이다. 문·통로 폭은 200cm 이상이다. 두 규칙 모두 콘텐츠 규칙이며 베이크 오류로 막지 않는다. | 확정 | `design/TerrainContract.md` §6-1 |
| D-071 | 지하 공간은 Floor·Shell을 나눈 닫힌 메시로 만들되 모양·크기를 재사용 모듈로 제한하지 않는다. 대형 공동, 공동 안 복층 구조물(D-064), 분기, 입구 여러 개를 허용한다. 입구는 지상 NPC가 오가는 긴 경사로와 Nav로 잇지 않는 수직 통로(플레이어는 낙하·마커 그래플 앵커)이며 옥탄트마다 조합을 달리한다. 지하 공간은 옥탄트 경계·꼭짓점 근처에 두지 않고 옥탄트 중심 방향에 두며, 깊이는 베이커의 int16 캡 검사가 막는다. D-035를 대체한다. 복층·분기를 뺀 원래 사유(D-006, SurfaceCache 시절 범위 절단)는 다층 Atlas(D-057)와 접힌 sheet 분할(D-064)로 남아 있지 않다. | 확정(Phase 7c 검증) | `design/TerrainContract.md` §6 |
| D-072 | 비지각 Layer 해상도는 source마다 정한다. 기본은 25cm(m=4)이고 Support 컴포넌트에 `LNP.Surface.CoarseSupport` 태그가 있으면 지각과 같은 100cm(m=1)로 굽는다. 지각을 연장한 넓고 완만한 공동 바닥용이며, 그 위 구조물은 기본 해상도를 쓴다. codec v2는 Layer마다 분할 수를 저장하므로 형식은 바뀌지 않는다. D-057의 정수배 규칙은 유지한다. | 확정(Phase 7c 단위 1 구현 완료) | `design/SurfaceBaking.md` "해상도 정책" |
| D-073 | 비행 적을 편성한 Pod는 위쪽 여유(headroom)가 충분한 Pod 후보에만 둔다. 베이커가 Spawn 후보마다 위쪽 exact sweep으로 headroom을 기록하고, 런타임 할당이 비행 Config(`IdleAltitude` 상한 + clearance + 여유)에서 유도한 기준과 비교한다. 지하·지상을 구분하지 않는다. 비행 적이 천장 낮은 공간으로 추격하지 못하는 것은 허용한다. | 확정(Phase 7c 단위 3 구현 완료) | `design/MovementIntegration.md` "완전 비행 NPC" |

| D-074 | Pod 귀속 PureEntity 비행 적의 임시 월드 편성 목표는 600이다. 활동 대역·구간 배회 복제(D-067·D-068)를 전제로 하며 동시 활성·동시 사격 상한이나 최대 총수 보장은 아니다. 지상/비행/벽보행의 최종 총수·비율과 동시 활성 예산은 Phase 14 구현 뒤 같은 활동·교전·viewer 조건의 통합 측정으로 결정한다. 비행 900~1,200은 확대 후보로만 남긴다. D-054를 대체한다. | 대체됨(D-076) | `history/Phase13_Log.md` 2026-10-05 마무리, `design/MovementIntegration.md` "예산" |
| D-075 | 벽보행 PureEntity 비용 예산: 전량 활성 링 조건에서 100기당 서버 frame P95 약 +0.45ms, exact 약 1회/개체·프레임·호출당 5~6us, 관통 0. 비행(100기당 1.0~1.5ms)보다 서버 비용이 작다. 게스트 수신량은 반복 편차가 커 개체당 값으로 단정하지 않는다. 측정 지형(Meadow)은 트인 지각이라 벽·천장 비율이 낮다. 최종 총수는 Phase 14 단위 6c 통합 측정으로 정한다. | 확정(Phase 14 단위 6b) | `history/Phase14_Log.md` 2026-10-06 단위 6b |
| D-076 | 월드 적 편성은 지상·비행·벽보행 1:1:1, 종류별 Pod 귀속 PureEntity 1,080(120 Pod × Pod당 9: 지상은 근접 6·원거리 3)이다. Pod 주변 스폰 반경은 1,500cm다(1,000cm에서는 지면 자리가 모자라 지상이 41%만 배치된다). 활동 대역을 전제로 하며 비용은 활성 수가 정한다 — 측정 조건의 활성 약 400에서 서버 frame P95 10.8ms, 전원 활성은 약 2,000기(16.1ms)가 경계이므로 동시 활성 상한을 약 2,000으로 본다. 총수 4,160(종류별 약 1,400)도 Production은 통과했으나 여유가 작아 확대 후보로만 둔다. D-074를 대체한다. | 확정(Phase 14 단위 6c) | `history/Phase14_Log.md` 2026-10-06 단위 6c |
| D-077 | 벽보행 NPC는 움직이는 패널에 붙으면(윗면·옆면·밑면) 함께 운반되고, 패널과 다른 면 사이에 끼어 겹침을 풀 수 없으면 압사한다(체력 0, 기존 사망 경로). 패널에 깔린 개체가 패널 밑면에 붙어 함께 올라가는 동작은 의도된 결과로 둔다. 지상·비행 NPC의 압사는 백로그 후보다. | 확정(Phase 14 단위 6e, 사용자 결정) | `design/MovementIntegration.md` "벽 타기 NPC" |
| D-078 | 휴면 벽보행이 붙은 정적 면을 움직이는 패널이 접안하며 덮는 겹침은 수용한다. 휴면 개체에는 정지 겹침 검사·압사를 돌리지 않는다(D-067 유지). 모든 플레이어에게서 약 130m 밖에서만 생기며, 깨어난 정지 개체는 활성 쪽 패널 근처 검사가 밀어내거나 압사시킨다. 관통 Gate는 활성 개체로 판정하고 휴면 몫은 따로 보고한다. | 확정(Phase 9 V011, 사용자 결정) | `phases/Phase09_PerfMemoryV011.md` §3 |
| D-079 | 경로 cache 공유는 경로 길이 비율을 제한하는 합류 규칙을 따른다. 시작 node에서 공유 경로의 앞쪽 4개 waypoint 중 직선 보행 가능하고 합류 거리 a ≤ 0.1 × 합류점 이후 경로 길이 s인 첫 waypoint에 합류한다(요청별 비율 ≤ (1+r)/(1−r)). Chase 대기열은 같은 tick 제출분 안에서 직선 거리 오름차순이다. 몰림 시 예산 적응은 하지 않는다. 이 A*가 Phase 10 비교의 기준선이다. | 확정(Phase 10 단위 2) | `design/GroundNavigation.md` "경로 공유"·"실행 위치", `history/Phase10_Log.md` 2026-10-13 단위 2 |
| D-080 | 대규모 추격 경로의 flow field·계층형 A*는 채택하지 않는다. 지상 NPC 경로는 D-079 개선 A*(scheduler cache·앞쪽 합류·Chase 거리 순서)로 확정한다. 같은 재생 비교에서 flow field는 기준 2(burst P95 −50%)·5(field 통째 재빌드), 계층형은 기준 1·2·3을 넘지 못했고, production 경로 비용은 병목이 아니다. 두 프로토타입과 재생기는 남겨 두며, 길찾기 최적화가 필요해지면 flow field를 첫 후보로 먼 추격자 없는 조건·움직이는 목표에서 다시 잰다. | 확정(Phase 10 단위 5, 사용자 결정) | `design/GroundNavigation.md` "Phase 10 판정", `history/Phase10_Log.md` 2026-10-13 단위 5 |
| D-081 | SurfaceData stale 입력은 베이커가 실제로 읽는 것만이다. 요소 클래스 BP의 saved hash(`PlacementClass`)는 넣지 않는다 — 베이커는 클래스를 로드·스폰하지 않고 마커에 적힌 `PatchSources`만 읽는다. Conditional Patch가 없는 런타임 전용 마커(그래플 앵커·런처·윈드필드)는 semantic row에서 빠지고, 베이크에 기여하지 않는 액터(런타임 전용 마커·장식·검토점)의 external actor package도 manifest에서 뺀다. 장치가 베이크에 영향을 줘야 하면 BP 내부가 아니라 마커에 명시한 데이터로 넘긴다. D-041의 "marker authoring 전체"를 patch 마커로 좁힌다. 근거: 2026-11-08 `BP_GrappleAnchor` 재저장 하나로 fixture 2개와 Meadow DA가 모두 낡았다. | 확정(사용자 결정) | `design/DataModel.md` "SourceManifest", `design/DynamicTerrain.md` §5 |
| D-082 | 지상 적은 Support 없는 Blocker(Blocker 역할이고 Support 역할·Support face 표·patch face 표가 모두 없는 registry source)를 높이와 무관하게 단차로 넘지 않는다. Support Atlas는 Blocker를 모르므로, 캐시 접지 결과의 캡슐(반지름·반높이 −1cm)이 이런 Blocker와 겹치면 hit identity registry 역할 조회로 거부하고 이번 프레임 제자리에 선다(exact probe의 `Rejected`와 같음). 이미 겹친 채 출발한 개체는 빠져나가도록 막지 않는다. Nav 베이크·배회 구간·exact 경로는 이미 같은 규칙이다. | 대체됨(D-087) | `phases/Phase16_SupportlessBlockerStep.md` §3, `design/MovementIntegration.md` "grounded 이동" |
| D-084 | Nav node 위치는 Nav 전용 보간(`LNPSupportAtlas::QueryNavLayer`)으로 만든다. 세 Atlas 꼭짓점이 모두 Valid·Walkable이면 NeedsExact여도 보간한다. NeedsExact는 캐시 접지의 정밀 경고라 걸을 수 있는 꺾임 띠를 Nav에서 빼면 벽 근처 NPC가 NoNode로 멈춘다. 베이크·런타임 graph 구성·Nav 투영·Conditional Patch base node가 같은 조회를 쓰며, 엄격 보간이 성공하는 곳에서는 결과가 같다. 지각 이음매 node는 런타임 이음매 대조 때문에 엄격 규칙을 유지한다. 엄격 보간 node가 하나도 없는 같은 Layer 연결 조각(대부분 edge가 끊긴 1~2칸)은 최근접 투영을 고립 섬으로 보내므로 베이크에서 버린다. 접지·지면 판정(`QueryLayer`/`QueryLayers`)은 바꾸지 않는다. 베이커 schema 10. | 확정(Phase 16 단위1, 사용자 결정) | `phases/Phase16_SupportlessBlockerStep.md` §3.6, `history/Phase16_Log.md` 2026-11-16 단위1 |
| D-085 | 기본 투영 반경(300cm) 안에 Nav node가 없는 지상 위치는 **보행 anchor**로 Nav에 잇는다. 같은 slot·Layer에서 600cm 안의 가까운 node(최대4개)부터 실제 접지 이동(`StepGrounded`, cache 우선·exact 폴백)을 캡슐 반지름 걸음으로 반복해 걸어서 이어지는지 검증하고, 통과한 node만 쓴다. 적은 node까지 걸어 돌아가고(시작점·도달성·슬롯·경로 요청), 플레이어는 node에 선 적이 발 150cm 안까지 걸어온다(목표). 플레이어의 `Surface`·`GroundPoint`는 물리 바닥이고 Nav 소속(`Node`)과 분리한다. Nav 밖 적이 목표로 곧게 가는 것은 현재 위치→MoveTarget 발의 실제 이동이 검증될 때만이고, 아니면 anchor로 먼저 돌아간다. 검증 실패(벽·Support 없는 Blocker·지지면 끊김·다른 층)와 600cm 밖은 기존 NoNode·도달 불가 규칙이다. 스냅·순간 이동을 하지 않고 분리력은 바꾸지 않는다(D-083). 검증 결과는 0.5초 재사용한다. | 확정(Phase16 단위2) | `phases/Phase16_SupportlessBlockerStep.md` §3.6, `history/Phase16_Log.md` |
| D-083 | 벽 근처 Nav 이탈·근접 추격 문제를 분리력 억제/제거로 해결하지 않는다. 군집의 NPC 겹침 해소가 악화되어서는 안 된다. 실제 NPC가 서고 걸어서 연결되는 정적 바닥의 Nav 커버리지를 바로잡고, 플레이어의 물리 지면 정체성과 Nav 투영을 분리하며 근접 적의 도달 가능한 접근·공격 위치를 처리한다. 두 조치 모두 필요하다. 분리력을 뺀 재시도 실험은 실제 PIE 정지가 남았고 사용자에게 기각됐으며, 남은 비채택 실험 정리부터 새 세션에서 진행한다. D-082의 실제 Blocker 비관통 규칙은 유지한다. | 확정(2026-11-16 사용자 결정, 근본 수정 구현 대기) | `phases/Phase16_SupportlessBlockerStep.md` §3.5~3.7, `../DiscardedApproaches.md` Case09 |
| D-086 | 지상 PureEntity 원거리 NPC의 사선 확보는 차단 중 기존 Nav 추격을 재개하고 사선이 트이면 사거리 안에서 그 자리를 유지하는 방식으로 한다. 공용 공격 게이트의0.25초 재검사를 재사용하고 이동에서 중복 검사·새 사격 위치 탐색을 추가하지 않는다. 짧은 옆걸음은 추후 플레이에서 필요할 때만 고려하고, 여러 사격 위치를 평가하는 전술 탐색은 게임 성격에 과하므로 제외한다. | 확정(2026-11-16 사용자 결정, 기능 구현·검증 완료) | `phases/Phase16_GroundRangedLineOfSight.md` |
| D-087 | 지상 적은 Support 없는 Blocker를 여전히 관통하거나 턱으로 넘지 않는다(D-082 판정 유지). 캐시 접지가 이 겹침으로 거부된 프레임에는 이번 이동을 접평면에서 ±45°, ±90° 돌린 옆걸음 후보를 작은 각도부터 같은 올린 sweep → 캐시 접지 → Blocker 겹침 검사로 시험하고 통과한 후보로 움직인다. 좌우가 모두 통과하면 원래 방향 진척이 큰 쪽, 같으면 원래 목표에서 캡슐 반지름만큼 비킨 자리가 트인 쪽(Blocker가 먼저 끝나는 쪽), 그래도 같으면 좌다. 상태는 저장하지 않는다. 모든 후보가 실패하면 제자리(`Rejected`)다. 이미 겹친 채 출발한 개체의 탈출 규칙, exact 폴백 경로, 분리력(D-083)은 그대로다. 같은 `StepGrounded`를 쓰는 보행 anchor 검증(D-085)도 같은 옆걸음으로 검증한다. | 확정(Phase16 후속 단위1, 사용자 승인) | `phases/Phase16_SmallBlockerDetour.md` §2, `design/MovementIntegration.md` "grounded 이동" |
| D-088 | 지상 적의 경로 추종은 격자 직선 검사(`IsDirectWalkable`)로 waypoint를 건너뛴 뒤, 그 검사가 멈춘 자리의 다음 waypoint를 실제 보행(`WalkReaches`, cache 우선·exact 폴백·D-087 옆걸음, 걸음 수 1.5배 상한·같은 slot·Layer)으로 검증해 통과하면 건너뛴다(Nav 구멍을 가로지르는 지름길). 한 번에 최대 2개를 가까운 것부터 보고, 후보까지 발 거리 800cm 이내이면서 현재 waypoint를 거치는 길보다 50cm 이상 짧을 때만 걷는다. 도달 판정은 50cm다. 걸음 검증 뒤 같은 개체는 0.5초(`WalkAnchorRetrySeconds`) 동안 다시 걷지 않고, 같은 경로(`RequestSerial`)·같은 waypoint의 실패는 다시 걷지 않는다. 검증으로 건너뛴 waypoint를 따라가는 동안은 투영 반경 밖이어도 보행 anchor로 되돌리지 않는다. Nav·overlay는 보지 않는다(Pod·마커는 실제 충돌이 막는다). A*·`SimplifyPath`·cache 합류(D-079)·Alert·도달 불가 접근(D-063)·보행 anchor(D-085)·분리력(D-083)은 그대로다. | 확정(Phase16 후속 단위2, 사용자 승인) | `phases/Phase16_SmallBlockerDetour.md` §3, `design/GroundNavigation.md` "경로 추종"
| D-089 | 베이커 Nav clearance 캡슐(정적 node·같은 Layer edge·Layer 간 portal·Conditional Patch base node 차단과 Patch Layer node)은 아래쪽만 10cm 줄인다(중심을 Up으로 5cm 올리고 반높이를 5cm 줄임, 머리 끝은 그대로). node 위치는 Atlas 보간이라 실제 지면보다 몇 cm 낮을 수 있어, 반지름 50cm 안의 작은 지형 요철에 캡슐 바닥만 닿아 평지 node·edge가 빠졌기 때문이다. 캡슐 전체를 올리지 않는 것은 낮은 천장·동굴 통로 node가 새로 빠지지 않게 하기 위해서다. 바닥 위로 10cm 넘게 솟은 Blocker·벽은 계속 막고, 경사·NonWalkable 판정·지각 이음매 엄격 보간(D-084)·양쪽 사본 대조(D-060)·고립 조각 제거(D-084)는 그대로다. 10cm보다 낮은 Support 없는 Blocker는 Nav에 남고 실제 이동은 옆걸음(D-087)이 돈다. 베이크 보고는 이 허용으로만 통과한 node 수와 그중 실제 NPC 캡슐(반지름35)이 서는 수를 남긴다. 베이커 schema 11. | 확정(Phase16 후속 단위3, 사용자 승인) | `phases/Phase16_SmallBlockerDetour.md` §4, `design/SurfaceBaking.md` "Nav node clearance" |

## 변경 규칙

- 확정 결정을 바꿀 때 기존 행을 조용히 덮어쓰지 않는다.
- 상태를 `대체됨`으로 바꾸고 새 ID를 추가한다.
- 변경 이유와 영향을 관련 설계 문서 및 현재 Phase 로그에 기록한다.
- 단순 구현 선택이나 임시 디버그 값은 이 원장에 추가하지 않는다.
