# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 7 완료(7a 2026-10-01, 7b 2026-10-02) — Phase 7c 착수 확인 대기
> 마지막 갱신: 2026-10-02

## 현재 목표

Phase 7b의 마지막 성능 Gate를 2026-10-02에 통과했다. 목표 투영 공유 변경본 Development package에서 trace 없는 자연·합성 추격 700마리 리슨 2P를 각각 두 번 실행해 모두 프레임 P95 16.67ms·경로 tick P95 1.5ms 안에 들었다. 아래 단계별 기록은 인계 이력이며 현재 미완료 작업은 아니다. 다음 목표는 사용자 승인 후 Phase 7c 실행 계획을 구체화하는 것이다. 7c 구현은 아직 시작하지 않았다.

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
- 자동화 `LootNPop.SurfaceNavigation` 77개(`Nav.PathScheduler`가 회귀 8-slot에서 scheduler 예산 분할 결정론·Stale·Cancelled·NoPath·우선순위·cache fingerprint를, `Nav.ProductionScheduler`가 Meadow 300요청 동시 투입의 직접 탐색 일치와 tick 시간을 검사. `Nav.GraphView`가 조밀 graph와 7a 조회 일치를, `Nav.RegressionPath`가 회귀 8-slot A*·스냅·접근점을, `Nav.ProductionPath`가 D-062 재선택과 Meadow A* 벤치마크를 검사. `Runtime.NavAssembly`가 8-slot Nav 조립·D-060 막힘·group·Stale을, `Nav.RegressionReachability`가 회귀 8-slot 도달성 oracle을, `Nav.ProductionSpawnProjection`이 Meadow Spawn 후보 투영을, `Nav.StepAndSlope`가 경사로·절벽 edge 규칙을 검사). `WorldCollision.LayerIdentity`가 저장된 SurfaceData로 exact face→Layer와 `QueryLayers` 일치를 보고, `Runtime.MassSpawnPlanning`이 Spawn 할당 규약을 검사한다. 자동화가 `SurfaceNavigationTests/MeshTerrain`의 `SM_BOptionExtracted`·`SM_COptionSphereSculpt`와 `Schema/DA_MinimalOctantSurfaceData`를 다시 저장하므로 커밋 전에 git으로 되돌린다
- 헤드리스 `-ExecCmds`는 쉼표로 명령을 나누고, 에디터 바이너리에서는 `Quit`로 종료되지 않는다(`Automation RunTests`는 종료함)
- 카메라 리그 `CR_ThirdPerson`에 `CollisionPush` 노드(`../TechDesign_CharacterMovement.md` §2.4)

## 바로 다음 작업

Phase 7b 계획은 `phases/Phase07b_PathExecution.md`(결정 D-061~063)다. 구현 단위 0(조밀 graph·A*·스냅), 단위 1(`FLNPNavPathScheduler`·경로 cache·revision view), 단위 2(Pod blocker overlay·Pod 소멸 시 재계획), 단위 3(Enemy 경로 연결·플레이어 접지 Nav 정보·배회 group 필터·진단)을 2026-10-01에 끝냈다. 단위 3은 자동화 78/78·전체 빌드·`-game` 추종 계측을 통과했고, 사용자 PIE 수동 플레이로 배경 프랍 우회와 동굴 안쪽 추격을 확인했다. 이음매 경로 12개는 회귀 자동화에서 검증했다. 단위 4(슬롯 도달성·접근점 이동·착지 Pod 재귀속)도 완료했다. 자동화 79/79·전체 빌드·크래시 수정 후 PIE 로딩과 사용자 양방향 접근점 플레이를 확인했다. 테스트용 거리·인내 설정은 원래 값으로 복구했다.

1. 단위 5의 계측·기능 검증과 성능 개선 단위 1을 완료했다. scheduler의 요청 시작·확장 대기·종료 처리와 Enemy/슬롯 소비자 비용을 분해하고, 검색 창에서 빈 Tile을 셀마다 조회하는 비용을 줄였다. 기존 창 범위·거리·후보 선택·방문 순서·확장 예산은 유지했다. 전체 자동화 79/79(회귀·production 창 비교 720입력 포함), 전체 빌드·BuildCookRun·패키지 리슨 2P 측정 5회의 정상 종료·probe·CSV 일치를 통과했다.
2. 700마리 합성 추격은 첫 실행에서 프레임 P95 14.97ms·경로 tick P95 0.640ms로 통과했지만, 반복 실행은 19.61ms·1.891ms로 실패했다. 자연 행동 700은 13.63ms·0.546ms로 통과, 합성 800은 21.17ms·1.794ms로 실패다. 실행별 경로 수요가 달라 안정적인 Gate 통과·최대 수용량으로 확정하지 않는다. Phase 7b는 미완료이며 Gate 16.67ms·1.5ms는 유지한다. 상세는 `history/Phase07_Log.md` 성능 개선 단위 1을 따른다.
3. 검색 창 정확성 수정 단위에서 꼭짓점 근처 3,000cm 반경 누락을 해결했다. 반경 구를 포함하는 로컬 좌표 상자의 격자 투영 범위를 조밀 검색과 `ProjectToNode`에 공통 적용했다. 수정 전 회귀·production 각각 8입력 실패를 재현했고, 수정 후 창 제한 없는 전 node 거리 oracle 720입력과 전체 자동화 79/79가 통과했다. 상세 재측정은 `history/Phase07_Log.md` 검색 창 정확성 수정 기록을 따른다.
4. 동일 CSV 비용 비교·scheduler 예산 검토 단위를 완료했다(2026-10-02). `Nav.RequestCostReplay`로 저장된 700마리 CSV 8,610/9,298건을 같은 시각 묶음별로 재생했다. 캐시 없는 네 설정의 상태·waypoint·확장·cost가 모두 같다. 기본 병렬 tick P95는 1.285/1.302ms, 예산 2,000은 0.732/0.831ms지만 완료 tick 수가 약 10% 늘었다. 시작 비용 64는 1.032/1.164ms이고, 직렬은 1.681/1.834ms다. 실제 차감량이 예산을 최대 15(시작 비용 64는 63) 넘는 경우를 확인했다. 시작·종료·병렬 대기를 포함한 시간 상한은 현재 없다. 기본값은 유지했다. Pod overlay·원래 owner 취소·실제 프레임 경계를 복원하지 않는 통제 비교이며 플레이 성능 Gate를 대체하지 않는다. 실행법·전체 표·한계는 `history/Phase07_Log.md`의 동일 CSV 비용 비교 기록을 따른다.
5. scheduler 예산 차감 초과 수정 단위를 완료했다(2026-10-02). 시작 비용이 모자라면 큐를 유지하고, 병렬 확장 배정 총량을 잔여 예산 안으로 제한한다. 프레임 예산이 시작 비용보다 작으면 새 요청은 설정을 올릴 때까지 대기하며 예산은 누적하지 않는다. 기본값·시간 예산·Gate는 유지했다. 전체 자동화와 동일 CSV 회귀 결과는 아래 마지막 검증 및 `history/Phase07_Log.md`를 따른다.
6. 수정본 Development package의 700마리 리슨 2P 합성 추격 두 번과 요청 지연 검증을 완료했다(2026-10-02). 프레임 P95 16.91/17.93ms는 실패, 경로 tick P95 1.129/0.903ms는 통과다. 전체 요청 지연 P95 156.8/159.4ms의 대부분이 큐 대기 155.7/159.0ms이며, 시작 뒤 종료 P95는 0.422/0.408ms다. 같은 프레임의 요청 묶음이 최대 572/576건이었다. 캡처 종료 미완료 0, 취소 30/46건을 별도로 집계했다. 기본값과 Gate를 유지하고 시간 예산은 추가하지 않았다.
7. 전체 프레임 CPU·동시 수요 프로파일 단위를 완료했다(2026-10-02). 기존 패키지의 CPU trace에서 게임 스레드 작업 대기 평균 8.02ms, Enemy 경로 소비 평균/P95 1.78/2.12ms, scheduler 0.217/1.043ms를 확인했다. worker 투사체 판정 범위는 평균/P95 5.74/6.48ms다(포함 벽시계 시간이며 게임 스레드 비용과 합산하지 않는다). 투사체 0 비교 실행의 프레임 P95는 12.24ms, 기본 500발 trace 실행은 18.15ms였다. trace 부담·실행별 수요 차이 때문에 순수 절감량이나 Gate 증거로 쓰지 않는다. 목표 전환의 가장 큰 여섯 묶음(540~561건, 요청 35.64%)은 큐 대기 P95 190.6ms, 나머지는 27.5ms였다. 상세와 분석 한계는 `history/Phase07_Log.md` 프레임 CPU 프로파일 기록을 따른다.
8. Enemy 경로 소비 내부 계측·목표 투영 공유 단위를 완료했다(2026-10-02). 목표 투영이 반복 조회되는 것을 trace로 확인하고 한 processor 실행의 고정 snapshot·overlay 안에서 동일 좌표·SurfaceHandle 결과만 공유한다. `-game` 700마리 합성 추격 최적화 두 번에서 목표 투영 재사용률은 99.40%, 소비 P95는 1.689/1.650ms였다. 전체 자동화 80/80·최종 전체 빌드·리슨 2P 네 실행 정상 종료와 probe/CSV 검증을 통과했다. 에디터·trace 실행이고 수요와 시스템 부하가 달라 순수 프레임 절감량이나 패키지 Gate 근거로 쓰지 않는다. 상세는 `history/Phase07_Log.md`의 Enemy 내부 계측 기록을 따른다.
9. 변경본 Development package 반복 측정·Phase 7b 종료 단위를 완료했다(2026-10-02). 자연 추격 700마리 두 번은 프레임/경로 tick P95 13.19/0.490ms, 14.07/0.557ms, 합성 추격은 12.74/0.440ms, 12.79/0.421ms로 모두 두 Gate를 통과했다. 새 패키지의 최종 1P도 정상 종료·probe·Gate를 통과했다. 소스·예산·기본값·Gate·테스트 에셋을 바꾸지 않았다. 요청 지연과 비교 한계는 `history/Phase07_Log.md`의 변경본 패키지 반복 측정 기록을 따른다. 현재 패키지 800마리는 측정하지 않아 최대 수용량은 미확정이다.
10. 다음 단위로 Phase 7c 실행 계획 구체화와 첫 구현 범위를 정리할지 사용자에게 확인한다. 승인 후 `Roadmap.md`의 Phase 7c와 D-064·065·070~073에 해당하는 기준 설계를 읽고, 접힌 sheet 분할·여러 portal·source별 해상도·비행 headroom·입체/지하 회귀 사례를 검증 가능한 단위로 나눈다. 승인 전에는 실행 계획 생성이나 7c 구현을 시작하지 않는다. 투사체 판정과 요청 지연의 제품 허용 상한은 별도 후속 후보다.

## 범위 확장 결정(2026-10-01, 검토 세션)

- 입체 지형(건물·탑·계단·벽, 그래플 동선), 벽 타기 NPC, 비행 NPC 대량화를 검토했다. 기존 Phase는 버리지 않는다.
- 결정은 D-064~D-070이다: 접힌 sheet 자동 분할, 여러 portal, `SurfaceCrawl` 도메인, Pod 단위 활동 대역(경계는 복제 컬·Actor 스폰 거리에서 유도), 구간 배회 복제, 비행 진형 기각, 입체 지형 레벨 디자인 규칙(계단 경사로 충돌·문 폭 200cm 이상).
- 새 단계는 Roadmap Phase 7c·13·14이며, 권장 순서는 7b → 7c → 13 → 14다. 7b 진행 순서는 바뀌지 않는다.
- D-054(비행 총수 200)의 근거는 대역폭이 아니라 서버 CPU다. Phase 13 뒤 재측정해 대체한다.
- 같은 날 지하 공간 검토로 D-071~D-073을 더했다. D-035를 대체해 옥탄트 중심 근방 대형 공동·복층 구조물·분기·여러 입구를 허용하고, 입구는 긴 경사로(지상 적 출입)와 수직 통로(Nav 미연결)를 옥탄트마다 조합한다. 넓은 공동 바닥은 `LNP.Surface.CoarseSupport`로 100cm 해상도를 쓰고, 비행 편성 Pod는 headroom이 충분한 후보에만 둔다. 구현은 Phase 7c 범위다.

## 이관된 후속 작업

- production Terrain Contract Component Tag 마이그레이션은 `Meadow_00`만 끝났다(4a 입력). 다른 production 옥탄트를 pool에 넣을 때 같은 방식으로 한다.
- `Meadow_00` 지각 이음매 경계 정점에 `|d| < 5e-7cm` 부동소수점 잡음이 있다. 베이커가 스냅하므로 Atlas에는 영향이 없다. 출처(mesh 생성기·빌드)는 추적하지 않았다.
- `LNPOctantSourceCollector`의 tag/profile/channel 검증과 marker authoring hash를 Phase 4·8 스키마에 맞춰 보강한다. owned external package를 모두 hash해 decoration 저장도 stale이 되는 현재 보수 정책은 보고서에 명시하고, false stale이 실제 문제가 될 때만 필터링한다.
- C-option 실험 에셋과 테스트의 구형 `LNP.Terrain.*` Component Tag는 Phase 4 입력으로 재사용하기 전에 현재 `LNP.Surface.*` 계약으로 마이그레이션한다.
- 현재 slot 순서 greedy definition 선택은 여러 slot mask가 있는 production pool을 도입하기 전에 최대 고유 제약 할당으로 교체한다(D-043).
- int16 복제 캡은 좌표 성분마다 걸리므로 30,000cm 옥탄트의 꼭짓점(좌표축) 부근 여유가 약 2,767cm다. 지하 공간은 옥탄트 경계·꼭짓점 근처를 피하고 옥탄트 중심 방향에 둔다(D-071, 허용 깊이 표는 `design/TerrainContract.md` §7).
- **Pod 등 동적 스폰 오브젝트의 Nav 차단(사용자 요구 2026-10-01):** Pod는 정적 Nav 베이크에 없으므로 지금은 이동 단계의 충돌 미끄러짐으로만 피한다. 7b 구현 단위 2의 최소 runtime overlay로 넣는다(D-061).
- **PCG 제외 구역(옥탄트 양산 전 필수, 사용자 결정 2026-09-27):** 2026-10-01부터 이음매 근처 제외 띠도 포함한다(콘텐츠 규칙, D-060). Meadow는 현재 이음매 clearance 탈락 49개(베이크 경고)이며 runtime에서 막힘으로 처리된다. PCG 프랍은 지각에만 광선을 쏘므로 동굴 입구 구멍에는 생기지 않지만 지붕 덮인 입구 옆·경사로 위에는 생길 수 있다. `Meadow_00`은 입구 주변 4개가 통행을 막지 않아 문제없지만 양산 옥탄트에서는 충분히 생길 수 있으므로 제외 구역을 만든다(`design/TerrainContract.md` §5 경사로와 같은 과제).
- **7b 목표 스냅 정책:** `ProjectToNode`는 edge 없는 고립 node(Meadow 8-slot 432개)도 반환한다. 7b는 가장 가까운 node를 우선하고 group이 어긋날 때만 반경 안 공통 group 짝을 다시 고른다(D-062).
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

Phase 7b 블로커는 해소됐다. 변경본 패키지의 자연·합성 추격 700마리 두 반복이 모두 프레임·경로 Gate를 통과했다. Phase 7c는 구현 블로커가 아니라 사용자 착수 확인 대기다.

## 마지막 검증

2026-10-02 변경본 패키지 반복 측정·7b 종료: Development BuildCookRun 104.69초·exit 0(`Phase07b_EnemyConsumer_Package.log`). trace 없는 자연·합성 추격 700마리 리슨 2P 네 실행과 최종 1P 100마리의 아홉 게임 프로세스 exit 0, probe 36개 PASS, assert·ensure·crash·게시 순서 위반·Unknown hit·Envelope escape·Layer jump 0이다. 네 요청/지연 CSV 6,404/9,485/7,310/9,295행의 중복·누락·시각 순서 오류 0, 캡처 종료 미완료 0이다. 최종 1P는 프레임 P95 4.52ms·경로 tick P95 0.007ms(`Phase07b_EnemyConsumer_Package1P_Final.log`)다. 전체 자동화는 소스 변경이 없어 직전 80/80 증거를 유지했다. 시작 시점 소스 6개·테스트 에셋 3개의 해시가 끝에도 일치했다. 자료는 `Saved/Profiling/Phase07b/N700_{natural,chase}_EnemyConsumer{1,2}_*`, 요약은 `EnemyConsumer_PackageSummary.json`이다. Phase 7b와 전체 Phase 7 완료.

2026-10-02 Enemy 내부 계측·목표 투영 공유: 최종 `LootNPopEditor Win64 Development` 전체 빌드(9.46초), 전체 자동화 80/80·실패/오류 0(`Phase07b_EnemyConsumer_AllTests.log`) 통과. 테스트 에셋 세 개는 실행 전 백업으로 복구하고 해시 일치를 확인했다. `-game` 리슨 2P 기준 두 번·최적화 두 번의 여덟 프로세스 exit 0, probe 32개 PASS, assert·ensure·crash·Unknown hit·Envelope escape·Layer jump 0, 요청/지연 CSV 9,533/9,182/9,131/9,308행의 키·시각 검증을 통과했다. 최적화 두 번의 목표 투영은 862,814/1,009,228회 중 실제 조회 5,188/6,096회(99.40% 재사용)다. 자료는 `Saved/Profiling/Phase07b/N700_chase_Enemy{Baseline,Optimized}{1,2}_*`, 요약은 `EnemyConsumer_Summary.json`이다. 패키지는 다시 만들지 않았고 Phase 7b는 미완료다.

2026-10-02 패키지 반복 측정·요청 지연 검증: 캡처 중에만 scheduler 제출·시작·종료 벽시계를 기록하고 취소·미완료를 구분했다. 에디터 전체 빌드(56.02초), 전체 자동화 80/80·오류 0(`Phase07b_BudgetFix_Latency_AllTests.log`), Development BuildCookRun(160.71초, `Phase07b_BudgetFix_Package.log`) 통과. 패키지 리슨 2P 두 실행의 호스트·게스트 네 프로세스 exit 0, probe 16개 PASS, assert·ensure·crash·Unknown hit·Envelope escape·Layer jump 0이다. `N700_chase_BudgetFix{1,2}_{Host.log,Guest.log,Requests.csv,RequestTimings.csv}`는 `Saved/Profiling/Phase07b`에 있다. 요청·지연 CSV 9,383/8,775행의 키 누락·시각 순서 오류 0이며 요약은 `BudgetFix_PackageSummary.json`이다. 테스트 에셋은 실행 전 내용으로 복구했다. 프레임 Gate 미통과이므로 Phase 7b는 미완료다.

2026-10-02 scheduler 예산 차감 초과 수정: 수정 전 `Nav.PathScheduler`에서 시작 비용 16·예산 15의 시작과 잔여 확장 예산 1·running 4의 확장 4를 재현했다(`Phase07b_BudgetFix_Repro.log`). 수정 후 전체 빌드(52.12초), 전체 자동화 80/80·오류 0(`Phase07b_BudgetFix_AllTests.log`, CSV 항목은 인자 없이 건너뜀), 별도 `Nav.RequestCostReplay` 두 번 PASS·exit 0(`Phase07b_BudgetFix_Replay{1,2}.log`)다. CSV 8,610/9,298건의 다섯 설정 모두 최대 차감이 예산 4,000/2,000 이하이며 캐시 없는 네 설정의 상태·waypoint·확장·cost 불일치 0이다. 직렬·병렬에서 예산 0·15·16·17, 잔여 예산 1, 즉시 종료 큐 보존을 검증했다. 테스트 에셋 세 개는 실행 전 내용으로 복구했고 `git diff --check` 통과다. tick 비용 CSV는 `Saved/Profiling/Phase07b/BudgetFix{1,2}_RequestCosts.csv`다. BuildCookRun·패키지 플레이 재측정은 이번 단위에 포함하지 않았다.

2026-10-02 동일 CSV 비용 비교·예산 검토: 전체 빌드(19.81초), 전체 SurfaceNavigation 자동화 80/80(신규 CSV 항목은 인자 없이 건너뜀, 기존 79개 통과), 별도 `Nav.RequestCostReplay` CSV 두 번 PASS·exit 0. 8,610/9,298건의 캐시 없는 네 설정은 상태·waypoint·확장·cost가 일치했다. 로그는 `Saved/Logs/Phase07b_RequestCostReplay{1,2,_AllTests}.log`, tick 비용 CSV는 `Saved/Profiling/Phase07b/WindowFix{1,2}_RequestCosts.csv`다. 테스트 에셋 세 개는 실행 전 내용으로 복구했고 `git diff --check`가 통과했다. runtime 변경·패키지 재측정은 없으며 700마리 Gate 미통과 상태를 유지한다.

2026-10-01 검색 창 정확성 수정: 전체 빌드, 전체 자동화 79/79(`Saved/Logs/Phase07b_WindowFix_AllTests.log`, 전 node 거리 oracle 720입력 포함), Development BuildCookRun(`Phase07b_WindowFix_Package.log`) 통과. 패키지 700마리 합성 추격 리슨 2P 두 번에서 host/guest 네 프로세스 exit 0·probe 16개 PASS·assert/ensure/crash/Unknown hit/Envelope escape/Layer jump/게시 순서 위반 0, CSV 8,610/9,298행이 제출 수와 일치했다. 파일 접두사는 `Saved/Profiling/Phase07b/N700_chase_WindowFix{1,2}`다. 프레임 Gate는 두 번 모두 실패했으며 7b는 미완료다.

2026-10-01 Phase 7b 단위 5 계측·기능 검증: 전체 자동화 79/79(`Saved/Logs/Phase07b_Unit5_OptimizedTests.log`), 에디터 전체 빌드, Win64 Development BuildCookRun(`Phase07b_Unit5_OptimizedPackage.log`) 통과. 최종 패키지 1P 100마리(`Phase07b_Unit5_FinalPackage1P.log`)는 프레임 P95 5.00ms·경로 tick P95 0.024ms, 요청 1,186건·waypoint 진행 4,211회다. 최종 리슨 2P 자연 추격 700 및 합성 추격 500·600·700·800의 host/guest 10개 로그에서 assert·ensure·crash·Layer jump·Unknown hit·Envelope escape·패널 게시 순서 위반 0, probe PASS를 확인했다. CSV 행 수는 각 실행의 제출 요청 수와 같다. 합성 추격 700·800은 성능 실패, 500은 통과, 600은 경로 Gate 경계 실패다. Phase 7b 완료 조건은 남아 있다.

2026-10-01 Phase 7b 구현 단위 3 완료: `LootNPopEditor Win64 Development` 전체 빌드, 전체 SurfaceNavigation 자동화 78/78(`Saved/Logs/Phase07b_Unit3_AllSurfaceNavTests.log`) 통과. `-game` 1P 100마리 스모크(`Phase07b_Unit3_EditorGame_WanderFinal.log`)에서 프레임·exact·lock PASS, 경로 추종 프레임 109,883회·waypoint 진행 1,099회, ensure·assert·crash 0. MCP 에디터 자동화로 `Nav.RegressionPath` 1/1을 재확인했고 이음매 12개 경로는 양쪽 슬롯을 지났다. 사용자 PIE 수동 플레이에서 배경 프랍 우회와 동굴 안쪽 추격을 확인했다. 동굴은 같은 슬롯의 Layer portal 두 개를 지나므로 슬롯 이음매와는 별개이며, 이음매 추격의 별도 수동 플레이는 수행하지 않았다.

2026-10-01 Phase 7b 구현 단위 2: `LootNPopEditor Win64 Development` 전체 빌드 통과. 신규 `Nav.PodOverlay`는 Pod 우회·Popped 뒤 직접 경로 복귀·같은 serial 재계획·이음매 양쪽 차단을 통과했다(`Saved/Logs/Phase07b_Unit2_PodOverlayTest_Final.log`). 전체 SurfaceNavigation 78/78도 통과했다(`Phase07b_Unit2_AllSurfaceNavTests.log`). 에디터 `-game` 1P에서 Pod 120개가 overlay revision 1·차단 node 623개로 게시됐고 ensure·crash 없이 종료했다(`Phase07b_Unit2_EditorGame.log`). 이 스모크의 부하 harness 프레임 P95 16.67ms는 단위 5 패키지 Gate가 아니다.

2026-10-01 Phase 7b 구현 단위 1: `LootNPopEditor Win64 Development` 전체 빌드(경고 0), Nav 12/12(`Saved/Logs/Phase07b_Unit1_NavTests.log`)와 전체 SurfaceNavigation 자동화 77/77(`Phase07b_Unit1_AllSurfaceNavTests.log`)가 통과했다. Meadow 300요청 동시 투입(예산 4,000·scratch 4)은 36 tick, 병렬 tick P50 1.09ms·P95 1.38ms, 직렬 P50 1.72ms·P95 2.00ms, 결과는 직접 탐색과 모두 같다. `-game` 스모크는 소비자가 없어 하지 않았다(구현 단위 3·5).

2026-10-01 Phase 7b 구현 단위 0: `LootNPopEditor Win64 Development` 전체 빌드, 전체 SurfaceNavigation 자동화 75/75(`Saved/Logs/Phase07b_Unit0_AllSurfaceNavTests.log`, 신규 `Nav.GraphView`·`Nav.RegressionPath`·`Nav.ProductionPath`)가 통과했다. Meadow A* 300쌍(20~80m) 확장 P50 308·P95 1,439, 시간 P50 118us·P95 512us, 확장당 0.38us, 전부 Found. 조밀 graph resident 4.10MiB. `-game` 스모크와 cooked 측정은 하지 않았다(구현 단위 5 Gate).

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
