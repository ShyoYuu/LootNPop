# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 7 — 7a Nav 데이터 기반 완료(2026-10-01), 다음은 7b 경로 실행 착수
> 마지막 갱신: 2026-10-01

## 현재 목표

Phase 7은 2026-09-29에 7a Nav 데이터 기반부터 착수했다. 실행 계획은 `phases/Phase07a_NavDataFoundation.md`다. 구현 단위 0의 runtime 순수 자료구조·codec에 이어 구현 단위 1에서 coarse Grid, blocker dilation, 6방향 edge, local component, Layer portal, ordered seam endpoint와 editor payload 저장을 완료했고 `DataVersion=5`, `BakerSchemaVersion=4`를 활성화했다. 회귀 동굴은 exact Support polyline capsule sweep으로 지각↔통로와 공동↔통로 portal 두 개를 보존한다. 구현 단위 2(2026-10-01)에서 loader가 Nav/Traversal을 필수 decode하고, 순수 `LNPNavRuntime`이 8-slot runtime Layer ID·node ref·seam link·runtime StaticNavComponent를 조립해 Support와 같은 snapshot으로 게시한다. 이음매 줄 node·edge의 사본 간 clearance 차이는 D-060에 따라 막힘으로 합쳐 `BlockedSeamNodes`·`BlockedSeamEdges`에 기록한다(Meadow 392/16, 회귀 0/0). 구현 단위 3(2026-10-01)에서 ReachabilityGroup(초기 1:1)·`ConnectivityGraphVersion=1`, `LNPNavQuery`(handle 기준 node 투영·node 지면점·group·Stale 판정), `NavReport`·`DrawNav` 진단과 회귀 8-slot oracle을 추가했다. 스모크에서 단위 1 베이커의 edge step 결함(200cm 끝점 높이 차를 step 45cm와 비교해 약 12.7° 이상 경사를 끊음)을 찾아 step·경사 구분 규칙으로 고치고 `BakerSchemaVersion=5`로 재베이크했다. Meadow runtime component는 41,169→689다. 구현 단위 4(2026-10-01)에서 Development package 1P와 리슨 2P host/guest가 같은 Nav(component·group 689, version 1)를 게시하고 모든 probe를 통과해 **Phase 7a가 끝났다.** cooked load는 약 35ms, Nav resident 0.59MiB다. 7b 입력 통계는 아래 "7a → 7b 인계 통계"에 있다.

Phase 4b(부유섬·동굴 키트 다층 베이크)가 2026-09-27에 끝났다. 완료 증거는 `phases/Phase04b_MultiLayerSupport.md` §5와 `history/Phase04b_Log.md`에 있다. 다층 Atlas·face 표·codec v2 규약 원본은 `design/SurfaceBaking.md` "다층 Atlas 규약"이다.

Phase 5(런타임 로더와 SurfaceCache 교체)는 2026-09-28에 구현 단위 0~5와 종료 검증까지 완료했다. 정상 실행의 123만 runtime trace bake를 없애고 서버·클라이언트가 같은 immutable Support/Spawn snapshot을 게시한다. stale은 cook 전 CI 결정론 자동화와 게시 전 런타임 validation에서 차단한다. 실행·완료 증거는 `phases/Phase05_RuntimeLoader.md`와 `history/Phase05_Log.md`에 있다.

Phase 6은 2026-09-28에 착수해 2026-09-29 구현 단위 0~5와 종료 검증까지 완료했다. PureEntity grounded의 수평 blocker sweep은 유지하고 하향 support probe만 cache-first로 바꾸며, 모든 비확신 결과와 공중 착지는 Phase 3b exact 경로로 처리한다. `LNP.SurfaceNav.EnemySupportCache`로 exact-only/cache-first를 비교하고 `LNP.SurfaceNav.EnemyGround.Report`에서 적중·폴백 수를 본다. 현재 Layer handle이 없거나 snapshot generation이 다르면 임의 Layer를 고르지 않고 exact로 간다. 저장된 회귀 fixture로 지각·동굴 cache hit, 섬 가장자리 낙하, 정적 blocker를 exact-only와 비교한다. 공중 프레임은 이전 handle을 즉시 지우고 정적 Support 착지 exact identity로만 재획득한다. Idle 배회는 현재 handle의 다층 snapshot을 우선하고 비확신 결과만 exact로 검증하며, 다른 Layer 결과는 거부한다. ActorPromoted는 Mover floor identity·공중 속도를 Entity로 인계하며, PureEntity의 움직이는 패널 접촉은 별도 contact로 transform delta와 이탈 속도를 잇는다. DynamicSupport 게시 tick은 Mass PrePhysics의 선행 조건이다. `GetSurfacePoint` adapter, `ULNPSurfaceCacheSubsystem`, runtime bake 설정과 `EnemyExactGround` 비교 분기를 제거했다. Development package 재측정에서 exact-only 한계는 약 700마리, cache-first는 약 800마리였다. 700마리 cache-first는 grounded cache hit 90.60%, query/frame P50 902, 프레임 P95 14.65ms였다.

## 7a → 7b 인계 통계(2026-10-01, production Meadow 8-slot)

- node 541,832(slot당 67,729, Tile 349), grid edge 1,504,960, seam link 4,036, portal 32
- grid 차수 평균 약 5.55이고 73.1%가 6방향 전부 열린 node다(`NavReport`의 `gridDegree`). A* 분기 수는 사실상 6이다
- runtime component 689: 1 node 432, 2~10 200, 11~100 40, 101~1,000 8, 1,000 초과 9. 지각 주 component 510,352 node(94.2%)가 최악 탐색 공간이다
- Spawn 후보 9,990개 투영 P50 55.8·P90 82.9·P99 124.3·최대 290.5cm, 고립 node 적중 5·소형 component(≤10 node) 적중 25
- 목표 스냅 질문: 가장 가까운 node 규칙이 고립 node를 고른다. 에디터·패키지 2P 호스트 폰 모두 node 1개 component에 투영됐다

## Phase 5 구현 단위 3~5 결과(2026-09-28)

- Mass spawn이 8-slot Spawn snapshot을 직접 소비한다. 특정 세트 authored → 일반 authored → random 순으로 Pod를 배정하고, 같은 slot·`LocalLayerId`·반경의 enemy 후보를 고른다. 초기 `SurfaceHandle`, per-frame queue, Pod link, PodID, density와 부하 harness를 유지했다.
- production 네 세트는 74+20+16+10=120 Pod를 모두 random 후보에서 배치했고 shortfall은 0이었다. 세트별 requested/authored/generic/random/placed/shortfall과 enemy requested/placed를 로그로 남긴다.
- `ALNPGameMode`는 SurfaceData 게시 뒤 legacy SurfaceCache bake 없이 바로 Mass spawn으로 진행한다. 적 이동·Idle·부하 baseline·exact oracle의 Layer 0 호환 조회도 새 subsystem으로 옮겼다.
- Spawn clearance preview는 transient/editor-only 입력을 제외하고 Static Mesh 비동기 compile을 완료한 뒤 probe를 만든다. 이 수정으로 전체 자동화 순서에 따라 Meadow PCG blocker 반영이 달라지던 문제를 제거했고, production Meadow를 9,990 candidates·359,652 B로 다시 구웠다.
- 전체 자동화 59/59, 에디터 전체 빌드, Development BuildCookRun, 패키지 1P·리슨 2P를 통과했다. 1P load→publish는 17.85ms, serialized 2.86MiB, decoded resident 2.97MiB였다. 2P는 host 15.89ms·guest 14.44ms이며 양쪽 generation 1, query 8/8, bindings 88/88였다.

## Phase 5 구현 단위 2 결과(2026-09-28)

- `ALNPMassSpawnPoint`가 안정 GUID와 선택적 `TargetSpawnSetId`를 authoring하며, 복제·붙여넣기에는 새 GUID를 발급한다. 회귀 LVI에는 지각·섬·동굴 anchor 3개가 있다.
- production `DA_MassSpawnConfig`의 네 entry에 안정 `SpawnSetId`를 저장했다. 빈 ID·중복 ID와 unknown anchor 참조는 config/bake validation에서 차단된다.
- 베이커가 authored anchor를 Support Layer에 투영하고 yaw를 접평면에 보존한다. random candidate는 Support raster에서 결정론적으로 만들며 Pod·Enemy capsule clearance를 bake-only physics preview world에서 검증한다.
- Spawn codec v1을 도입해 `DataVersion=4`, `BakerSchemaVersion=3`으로 올렸다. 로더는 Spawn payload도 원자적 snapshot의 일부로 decode·검증하고 같은 asset을 쓰는 slot끼리 결과를 공유한다.
- SurfaceData 3개를 다시 구웠다. Spawn 레코드는 Crust 10,573개, Regression 10,619개(3 authored), Meadow 10,538개다.
- 이 단위에서 만든 Spawn stream은 구현 단위 3에서 production Mass spawn 입력으로 전환됐다.

## Phase 5 착수 사실(2026-09-28)

- **source key는 유지된다.** `LNP.SurfaceNav.ProbeSourceKeys`로 `Meadow_00` runtime slot 8개의 Support component 11개를 집계했다. 에디터 `-game`과 Win64 Development package 모두에서 11개 key가 slot마다 1회씩 나왔고, 저장된 베이크 key와 문자열이 전부 같았다. Phase 5 registry는 `<Actor FName>.<Component FName>`을 그대로 쓰고 게시 전 missing·duplicate key를 차단하면 된다. 로그는 `Saved/Logs/Phase05_SourceKeys_EditorGame.log`·`Phase05_SourceKeys_Package.log`.
- **게스트의 현재 production Surface point 소비는 사실상 없다.** `ULNPEnemyMovementProcessor`는 클라이언트에서 SurfaceCache 조회 전 return하고, Mass StateTree는 엔진상 `Server | Standalone`이며, Mass spawn은 GameMode가 서버에서만 시작한다. 게스트의 `ULNPTrajectoryGuideComponent`는 `GetSurfacePoint`의 점을 쓰지 않고 옥탄트 준비 신호로만 쓴다. `ALNPGameState`·`ALNPPlayerController`는 현재 로컬 베이크 시작·완료를 로딩 게이트로 쓴다. 따라서 현재 소비자만 보면 게스트 전체 Atlas는 필수가 아니지만, Roadmap의 클라이언트 로드 완료 조건과 추후 예측 query 대칭성을 유지하려면 클라이언트도 같은 immutable snapshot을 게시하는 편이 간단하다.
- `ProbeFaceIndex`에서 발견한 1건의 `MeshComponent` 미해석은 베이크 source가 아니라 slot 밖에 서버가 스폰한 스프링 런처(simple collision)다. face 표 검증이 slot Level의 베이크 source만 세도록 진단 명령을 보정했다.

## 착수 시 필수 문서

- `Roadmap.md` §3·§4("Phase 5와 6 사이")
- `design/Architecture.md`(런타임 초기화·snapshot 게시), `design/DataModel.md`(definition·SurfaceData·D-043 선택), `design/RuntimeCollision.md`(Surface query API·hit identity registry·face 표 해석 경로), `design/ValidationAndMigration.md`(소비자 전환·`SurfaceHandle`)
- 옥탄트 초기화 흐름: `../TechDesign_InitSequence.md`, 기존 캐시: `../TechDesign_SurfaceCache.md`

## 인계 기준선(3b·3c·4a·4b)

- 30,000cm `Meadow_00`: 섬 3개(큰 섬·섬 A 경사로 두 방식·섬 B), 간이 동굴, 월드 장치 마커. 8 slot 모두 이 definition
- PureEntity cache-first 이동(`Enemy/LNPEnemySurfaceMovement.*`)과 exact 이동 primitive(`Enemy/LNPEnemyExactMovement.*`, D-049). 접지는 cache-first가 필수이고 `EnemyExactLateralSweep`·`EnemyParallelMovement`만 측정 CVar로 남는다
- 패널 → DynamicSupport 게시 tick → Mass PrePhysics 선행 조건(D-050 C안). 게시와 Mass가 같은 패널 뒤에서 동시에 시작하지 않도록 완전한 체인으로 건다
- **exact 한계치: 단일 스레드 500마리(최악 조건 기준선), 병렬 약 700마리.** 접지 개체당 프레임 약 1.46 query·8us. Phase 4 캐시 적중률 목표와 Phase 6 재측정은 병렬 700 기준으로 읽는다
- 비행 드론(PureEntity, D-052·053·054): 총수 200, Pod 타입 분리. 비행 비용은 Support 캐시의 절감 대상이 아니다
- 부하 harness `-LNPLoadBaseline=N`·`-LNPLoadBaselineFlyers=F`, 측정 스크립트 `Scripts/Profiling/RunLoadBaselineMatrix.ps1`. 프레임 판정은 패키지 Development 호스트 `-nullrhi`
- Support Atlas: runtime `LNPSupportAtlas`(격자·row span·rasterize·codec v2·`QueryLayer`·`QueryLayers`), 지각 전용 `LNPCrustAtlas`(이음매 스냅 rasterize·seam 규약), Editor `LNPOctantSurfaceBaker`·`LNP.SurfaceNav.BakeOctant <LevelPath>`. 지각 N=735(100cm), 비지각 Layer m=4(25cm, 확정). 현재 `DataVersion` 5는 Navigation/Traversal codec v1까지 포함하며 세 저장 asset은 `BakerSchemaVersion=5` 최신 hash로 재베이크됐다. `Meadow_00` Support payload 2.63MB, Nav/Traversal payload 약 535KiB/14KiB다. 지각 조회 NeedsExact 2.35%, 반지름 P99 1.06cm. 비지각 Layer 반지름 P99 0.12cm, 조회 NeedsExact 20.6%(큰 섬 1.9%, 섬 B 계단 칸은 거의 전부). `QueryLayers`는 footprint 가장자리 띠도 후보로 본다. face→Layer 표는 source key(`<Actor FName>.<Component FName>`)와 external `FaceIndex`로 찾는다(`design/RuntimeCollision.md`)
- `DA_OctantSurface_Meadow_00`·`DA_OctantSurface_Fixture_Crust`는 LVI 옆에 저장돼 있다. `Bake.OctantBakeDeterministic`가 저장본과 현재 source의 일치를 검사하므로 LVI나 베이크 설정을 바꾸면 `BakeOctant`로 다시 굽는다
- 회귀 공간(D-056): 정적 사례는 `LVI_Octant_Fixture_Regression`(생성 `LNP.SurfaceNav.BuildRegressionFixture`, 배치 원본 `LNPRegressionFixture.h`)을 8 slot 합성으로 검사하고, 동적 3사례만 `L_SurfaceRegression`(30,000cm)에 있다. `DA_OctantSurface_Fixture_Regression`도 결정론 베이크 검사 대상이다
- 동굴 키트 greybox(`/Game/Maps/CaveKit`, `LNP.SurfaceNav.BuildCaveKit`, 치수 원본 `LNPCaveKit.h`): 직육면체 공동 + 경사 통로, Floor/Shell 분리, 규약 검사 `Bake.CaveKitContract`. `Meadow_00` 동굴은 (위도 15°, 방위 60°)에 있고 `LNP.SurfaceNav.PlaceCaveKit`으로 배치했다(지각 메시 입구 절단 포함)
- 자동화 `LootNPop.SurfaceNavigation` 72개(`Runtime.NavAssembly`가 8-slot Nav 조립·D-060 막힘·group·Stale을, `Nav.RegressionReachability`가 회귀 8-slot 도달성 oracle을, `Nav.ProductionSpawnProjection`이 Meadow Spawn 후보 투영을, `Nav.StepAndSlope`가 경사로·절벽 edge 규칙을 검사). `WorldCollision.LayerIdentity`가 저장된 SurfaceData로 exact face→Layer와 `QueryLayers` 일치를 보고, `Runtime.MassSpawnPlanning`이 Spawn 할당 규약을 검사한다. 자동화가 `SurfaceNavigationTests/MeshTerrain`의 `SM_BOptionExtracted`·`SM_COptionSphereSculpt`와 `Schema/DA_MinimalOctantSurfaceData`를 다시 저장하므로 커밋 전에 git으로 되돌린다
- 헤드리스 `-ExecCmds`는 쉼표로 명령을 나누고, 에디터 바이너리에서는 `Quit`로 종료되지 않는다(`Automation RunTests`는 종료함)
- 카메라 리그 `CR_ThirdPerson`에 `CollisionPush` 노드(`../TechDesign_CharacterMovement.md` §2.4)

## 바로 다음 작업

Phase 7b(경로 실행) 착수. `Roadmap.md` §Phase 7 내부 게이트와 `design/GroundNavigation.md`를 읽는다.

1. 위 인계 통계를 입력으로 `phases/Phase07b_PathExecution.md` 실행 계획을 만든다. 510k node 지각 component에서 chord heuristic A*의 frame당 확장 예산·다중 프레임 request lifecycle·path cache 키(generation·connectivity version 포함)·waypoint following과 PureEntity·Actor 전달 범위를 정한다.
2. 목표 스냅 정책(고립·소형 component node 처리)과 Pod 동적 Nav 차단(아래 이관 작업)의 7b/Phase 8 위치를 계획 문서에서 확정한다.

## 이관된 후속 작업

- production Terrain Contract Component Tag 마이그레이션은 `Meadow_00`만 끝났다(4a 입력). 다른 production 옥탄트를 pool에 넣을 때 같은 방식으로 한다.
- `Meadow_00` 지각 이음매 경계 정점에 `|d| < 5e-7cm` 부동소수점 잡음이 있다. 베이커가 스냅하므로 Atlas에는 영향이 없다. 출처(mesh 생성기·빌드)는 추적하지 않았다.
- `LNPOctantSourceCollector`의 tag/profile/channel 검증과 marker authoring hash를 Phase 4·8 스키마에 맞춰 보강한다. owned external package를 모두 hash해 decoration 저장도 stale이 되는 현재 보수 정책은 보고서에 명시하고, false stale이 실제 문제가 될 때만 필터링한다.
- C-option 실험 에셋과 테스트의 구형 `LNP.Terrain.*` Component Tag는 Phase 4 입력으로 재사용하기 전에 현재 `LNP.Surface.*` 계약으로 마이그레이션한다.
- 현재 slot 순서 greedy definition 선택은 여러 slot mask가 있는 production pool을 도입하기 전에 최대 고유 제약 할당으로 교체한다(D-043).
- int16 복제 캡은 좌표 성분마다 걸리므로 30,000cm 옥탄트의 꼭짓점(좌표축) 부근 여유가 약 2,767cm다. 동굴은 꼭짓점 부근을 피한다(`design/TerrainContract.md` §7).
- **Pod 등 동적 스폰 오브젝트의 Nav 차단(사용자 요구 2026-10-01):** Pod는 정적 Nav 베이크에 없으므로 지금은 이동 단계의 충돌 미끄러짐으로만 피한다. 경로가 Pod를 미리 돌아가도록, 스폰 뒤 움직이지 않는 Pod가 덮는 Nav node를 runtime overlay에서 막는다. 이음매 위 Pod도 seam link로 양쪽 사본을 함께 막을 수 있어야 한다. 7b 또는 Phase 8에서 위치를 정한다.
- **PCG 제외 구역(옥탄트 양산 전 필수, 사용자 결정 2026-09-27):** 2026-10-01부터 이음매 근처 제외 띠도 포함한다(콘텐츠 규칙, D-060). Meadow는 현재 이음매 clearance 탈락 49개(베이크 경고)이며 runtime에서 막힘으로 처리된다. PCG 프랍은 지각에만 광선을 쏘므로 동굴 입구 구멍에는 생기지 않지만 지붕 덮인 입구 옆·경사로 위에는 생길 수 있다. `Meadow_00`은 입구 주변 4개가 통행을 막지 않아 문제없지만 양산 옥탄트에서는 충분히 생길 수 있으므로 제외 구역을 만든다(`design/TerrainContract.md` §5 경사로와 같은 과제).
- **7b 목표 스냅 정책:** `ProjectToNode`는 가장 가까운 walkable node를 고르므로 edge 없는 고립 node(Meadow 8-slot 432개)도 반환한다. 에디터·패키지 2P 스모크 호스트 폰이 모두 그런 node에 투영됐고 Spawn 후보는 5/9,990이 고립 node에 투영된다. 7b에서 목표 스냅이 group 크기·edge 유무를 볼지 정한다.
- match 중 옥탄트 재생성이나 slot Level 언로드를 도입하면 그 직전에 Mass 처리를 멈추는 gate를 함께 만든다(`design/RuntimeCollision.md`).
- 스프링 런처는 개체별 발사 속도·각도 입력이 없다(`DA_WorldDeviceConfig` 전역값, 정점 약 2,530cm). 섬별 튜닝은 이 입력을 만든 뒤에 한다. 지금은 런처→섬 A, 앵커→섬 B·큰 섬으로 역할을 나눴다. 런처→섬 A 경로는 3b 기능 점검에서 육안 미확인이다.
- LootPod collision proxy(`LNPStaticBlocker`)가 Camera 채널을 Block해 카메라가 Pod 뒤에서 당겨진다. 거슬리면 proxy만 Camera Ignore로 바꾼다.

## 알려진 불확실성

- 3c 측정 중 부하 harness 호스트가 비동기 스폰 배치 직후 한 번 멈췄다(재현 안 됨, `history/Phase03c_Log.md`). 재발하면 작업 스레드 exact probe와 게임 스레드 경합부터 본다.
- 병렬 한계치가 750 → 약 700으로 내려온 원인(비 query 비용 증가로 추정)은 나누어 재지 않았다.
- exact query의 배치 단위, 관찰 거리 축의 근처 반경과 원거리 판정 주기는 아직 정하지 않았다.
- `LNPSurfaceSupport` 소비자 전환 시점은 Phase 5 착수 시 확정한다.
- 렌더링 호스트 프레임(300마리 P95 26ms, 1000마리 37ms, RTX 4060 Laptop)은 GPU 비용이며 Surface Navigation 범위 밖이다. 렌더링 트랙에서 다룬다.
- Development package의 기존 Lyra Mannequin material은 누락 Material Function 때문에 default material로 대체된다. Surface Navigation 검증과는 분리된 콘텐츠 문제다.

- `-game` 2P 스모크 게스트의 Mover SimulatedProxy 시작 위치 경고 중 월드 반지름을 한참 벗어난 위치(최대 약 1.5km)가 많다. 3c 스모크에도 있었으므로 Surface Navigation 회귀는 아니고 원인은 조사하지 않았다(`history/Phase04a_Log.md`).

- Meadow slot당 local StaticNavComponent 5,151개의 주원인은 edge step 결함이었다(2026-10-01 수정, 91개). 남은 고립 cell은 slot당 54개이며 프랍 dilation 사이 작은 틈으로 보인다. 개별 원인은 조사하지 않았다.

## 블로커

현재 확인된 블로커는 없다.

## 마지막 검증

2026-10-01 Phase 7a 구현 단위 4(7a 종료): 에디터 전체 빌드, 전체 SurfaceNavigation 72/72(`Saved/Logs/Phase07a_Unit4_AllSurfaceNavTests.log`), Win64 Development BuildCookRun(972 packages, 오류 0, `Phase07a_Unit4_BuildCookRun.log`)이 통과했다. 패키지 1P와 리슨 2P host/guest(`Phase07a_Unit4_Package1P.log`, `Phase07a_Unit4_Package2P_*.log`)가 같은 Nav(component·group 689, version 1, seam link 4,036, 막힘 392/16)를 게시했고 `ProbeSurfaceData`·`ProbePanels`·`ProbeFaceIndex`·`ProbeSourceKeys`·LoadBaseline PASS, ensure·assert 0이다. cooked load elapsed 34~35ms, validateBuild 21~24ms, serialized 3.39MiB, decoded resident 3.56MiB, Nav resident 0.59MiB.

2026-10-01 Phase 7a 구현 단위 3 완료: `LootNPopEditor Win64 Development` 전체 빌드, 전체 SurfaceNavigation 자동화 72/72(`Saved/Logs/Phase07a_Unit3_AllSurfaceNavTests.log`)가 통과했다. edge step 규칙 수정 뒤 세 SurfaceData를 `BakerSchemaVersion=5`로 재베이크했다(`Phase07a_Unit3_Rebake.log`, Meadow local component 5,151→91). 에디터 `-game` 1P와 리슨 2P host/guest가 같은 Nav(component·group 689, version 1, seam link 4,036, 막힘 392/16, resident 0.56MiB)를 게시했고 `ProbeSurfaceData`·`ProbePanels` PASS, ensure·crash 0이다(`Phase07a_Unit3_EditorGame.log`, `Phase07a_Unit3_2P_*.log`).

2026-10-01 Phase 7a 구현 단위 2 완료: `LootNPopEditor Win64 Development` 전체 빌드, 전체 SurfaceNavigation 자동화 69/69(`Saved/Logs/Phase07a_Unit2_AllSurfaceNavTests.log`)가 통과했다. 에디터 `-game` 1P와 리슨 2P host/guest에서 같은 Nav 결과(runtime component 41,169, seam link 4,036, 막힘 392/16)를 게시했고 `ProbeSurfaceData`·`ProbePanels` PASS, ensure·crash 0이다(`Phase07a_Unit2_EditorGame.log`, `Phase07a_Unit2_2P_*.log`). Nav resident는 0.74MiB, validateBuild는 20.6ms다.

2026-09-30 Phase 7a 구현 단위 1 완료: `DataVersion=5`·`BakerSchemaVersion=4`로 세 SurfaceData를 재베이크했다. Crust/Regression/Meadow Navigation payload는 553,104/560,328/547,584 B, Traversal payload는 14,447/14,503/55,058 B다. 회귀 fixture는 69,378 cells·7 components·portal 2개(지각↔통로, 공동↔통로)·seam 1,107개다. `LootNPopEditor Win64 Development` 전체 빌드, Nav 4/4(`Saved/Logs/Phase07a_Unit1_NavTests_Final.log`), 결정론/저장본/회귀 oracle 1/1(`Phase07a_Unit1_Deterministic_Final.log`)이 통과했다. 재베이크 로그는 `Phase07a_Unit1_Rebake_Final.log`다.

2026-09-29 Phase 6 구현 단위 5 및 Phase 종료: Win64 Development BuildCookRun이 972 packages cook·stage·pak·archive까지 성공했다. 같은 패키지의 리슨 2P `-nullrhi -corelimit=4`·투사체 500 조건에서 exact-only는 700마리 P95 16.17ms 통과·750마리 17.44ms 실패, cache-first는 800마리 16.46ms 통과·850마리 17.44ms 실패로 한계가 약 700→800마리로 늘었다. 700마리 cache-first는 grounded cache hit 90.60%, query/frame P50 1,524→902, exact CPU P95 7.354→3.082ms, 프레임 P95 16.17→14.65ms였다(`Saved/Profiling/Phase06`). Development package 1P 100마리도 frame/exact/lock, `ProbePanels`, `ProbeSurfaceData`가 모두 PASS했고 순서 위반·Unknown hit·Envelope escape·Layer jump·ensure·crash 0이었다(`Saved/Logs/Phase06_Unit5_1P.log`). Phase 6 완료.

2026-09-29 Phase 6 구현 단위 4: Enemy 이동·Idle 배회의 `GetSurfacePoint`와 `EnemyExactGround=0` 분기를 제거해 cache-first 또는 exact만 사용하게 했다. 진단·부하 도구의 Layer 0 비교는 immutable snapshot의 명시적 `QueryLayerZero`로 분리했다. `ULNPSurfaceCacheSubsystem` 구현, runtime bake 설정과 CVar를 삭제했다. `LootNPopEditor Win64 Development` 전체 빌드, EnemyMovement 5/5, ExactMovement 4/4, 전체 SurfaceNavigation 64/64가 통과했다(`Saved/Logs/Phase06_Unit4_*Tests.log`). 리슨 2P 100마리 스모크는 양쪽 `ProbePanels`·`ProbeSurfaceData` PASS, 순서 위반·Unknown hit·Envelope escape·Layer jump·ensure·crash 0으로 정상 종료했다. 서버 cache hit는 112,950/125,700(89.86%)이었다(`Phase06_Unit4_2P_Host.log`, `Phase06_Unit4_2P_Guest.log`). 호스트 프레임 P95 29.35ms는 에디터 2개 동시 실행 결과라 성능 Gate가 아니며 구현 단위 5의 Development package 측정으로 판정한다.

2026-09-29 Phase 6 구현 단위 3: Actor 활성 중 Mover floor hit을 registry로 해석해 정적 handle·동적 contact를 갱신하고, 공중 강등은 Mover 속도를 그대로 Entity에 인계한다. PureEntity에는 별도 DynamicSupport contact, 패널 transform delta, 이탈 선속도 상속을 추가했다. 합성 패널 exact 착지→운반 fixture를 포함한 EnemyMovement 5/5와 ExactMovement 4/4, `LootNPopEditor Win64 Development` 전체 빌드가 통과했다(`Saved/Logs/Phase06_Unit3_EnemyMovementTests.log`, `Phase06_Unit3_ExactMovementTests.log`). late-join 리슨 2P에서 발견한 DynamicSupport 게시 tick/worker snapshot 경쟁을 `패널 -> 게시 -> Mass PrePhysics` 선행 조건으로 수정했다. 재실행 결과 호스트·게스트 모두 `ProbePanels` 8/8, SurfaceData query 8/8·binding 88/88, 순서 위반 0, ensure·crash 0으로 정상 종료했다(`Phase06_Unit3_2P_Host.log`, `Phase06_Unit3_2P_Guest.log`).

2026-09-29 Phase 6 구현 단위 2: `LootNPopEditor Win64 Development` 전체 빌드 성공. EnemyMovement 3/3과 ExactMovement 4/4 통과(`Saved/Logs/Phase06_Unit2_EnemyMovementTests.log`, `Phase06_Unit2_ExactMovementTests.log`). 에디터 `-game` 100마리 capture는 cache hit 91.90%(226,992/247,012), exact fallback 20,020, 합성 넉백 270회·착지 266회, Layer jump·Unknown hit 0이며 frame/exact/lock과 face/source/snapshot probe가 모두 PASS(`Phase06_Unit2_EditorGame.log`).

2026-09-29 Phase 6 구현 단위 1: `LootNPopEditor Win64 Development` 전체 빌드 성공. EnemyMovement 자동화 3/3(새 `GroundedFixture` 포함)과 ExactMovement 회귀 4/4 통과(`Saved/Logs/Phase06_Unit1_EnemyMovementTests.log`, `Phase06_Unit1_ExactMovementTests.log`). 에디터 `-game` 100마리 capture는 cache hit 86.07%(167,090/194,138), exact fallback 27,048, Unknown hit 0, Layer jump 0이며 face/source/snapshot probe PASS(`Phase06_Unit1_EditorGame.log`). 에디터 빌드 프레임 P95 20.35ms는 패키지 성능 Gate가 아니고 exact P95 0.630ms·lock P95 0.025ms는 통과했다.

2026-09-29 Phase 6 구현 단위 0: `LootNPopEditor Win64 Development` 전체 빌드 성공. EnemyMovement 자동화 2/2(`CachedGroundDecision`, `SurfaceHandleIdentity`)와 기존 ExactMovement 회귀 4/4 통과(`Saved/Logs/Phase06_Unit0_EnemyMovementTests.log`, `Phase06_Unit0_ExactMovementTests.log`).

2026-09-28 Phase 5 종료: `LootNPopEditor Win64 Development`와 Win64 Development BuildCookRun 성공. 전체 자동화 59/59(`Saved/Logs/Phase05_Unit5_AllAutomation.log`), 패키지 1P(`Phase05_Unit5_Package.log`)와 리슨 2P host/guest(`Phase05_Unit5_2P_*.log`) 모두 baseline·face index·source key·SurfaceData probe PASS. production Meadow는 mesh compile 동기화 후 9,990 candidates·359,652 B로 재베이크했다(`Phase05_Unit5_Meadow_Rebake.log`).

2026-09-28 Phase 5 구현 단위 3·4: Mass spawn planning 자동화 1/1, runtime 자동화 3/3, 에디터 `-game` 스모크 통과. production 네 세트 120 Pod, shortfall 0이며 정상 초기화 로그에 legacy SurfaceCache bake가 없다.

2026-09-28 Phase 5 구현 단위 2: `LootNPopEditor Win64 Development` 전체 빌드 성공. Spawn codec/authoring validation 2/2, 결정론 베이크 1/1, runtime loader 2/2 통과(`Saved/Logs/Phase05_SpawnTests.log`, `Phase05_DeterministicTest.log`, `Phase05_RuntimeTests.log`). 세 SurfaceData를 `DataVersion=4`·`BakerSchemaVersion=3`으로 재베이크했다.

2026-09-28 Phase 5 구현 단위 1: `LootNPopEditor Win64 Development` 전체 빌드 성공. runtime 자동화 2/2와 WorldCollision 회귀 8/8 통과(`Saved/Logs/Phase05_LoaderUnit1_RuntimeTests.log`, `Phase05_LoaderUnit1_WorldCollisionTests.log`). 에디터 `-game` 1P와 리슨 2P host/guest 모두 snapshot generation 1, query 8/8, binding 88/88, registry SurfaceData generation 1로 PASS(`Phase05_LoaderUnit1_EditorGame_Final.log`, `Phase05_LoaderUnit1_2P_*.log`).

2026-09-28 Phase 5 구현 단위 0: production `OctantPoolData`에 `DA_OctantSurface_Meadow_00`과 `DysonSphere_R300m_V1`을 연결했다. `LootNPopEditor Win64 Development` 전체 빌드 성공, loader 합성·production 자동화 2/2 통과(`Saved/Logs/Phase05_LoaderUnit0_Final.log`).

2026-09-27 Phase 4b 구현 단위 4(Phase 4b 종료): 에디터 빌드 성공, 자동화 `LootNPop.SurfaceNavigation` 54/54(로그 `Saved/Logs/Auto4b_U4.log`), `-game` 리슨 2P 스모크 통과(`Saved/Logs/Smoke4b_*.log`, ensure·크래시·`LogLootNPop` 오류 0).

2026-09-27 Phase 4b 구현 단위 0: Development 패키지 리슨 2P(`-LNPLoadBaseline=50`, 로그 `Saved/Logs/FaceIndex4b_*`)에서 호스트·게스트 모두 `ProbeFaceIndex` 2,000/2,000 PASS, `ProbePanels` PASS.
