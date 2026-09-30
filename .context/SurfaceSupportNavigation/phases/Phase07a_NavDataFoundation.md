# Phase 7a — Nav 데이터 기반

> 상태: 완료(2026-10-01) — 구현 단위 0~4와 cooked Gate 통과
> 예상 범위: 2~3세션
> 선행 조건: Phase 6 Enemy 접지·공중·넉백 전환(완료)

## 1. 목표

옥탄트별 `NavigationPayload`와 `TraversalPayload`를 에디터에서 결정론적으로 굽고, cooked 실행에서 8-slot 정적 연결성과 도달성을 immutable snapshot으로 게시한다.

- Support Layer별 coarse triangular Nav Grid를 만든다.
- stable local node 주소, 16×16 Tile, 6방향 이웃과 tile 경계 연결을 고정한다.
- 정적 obstacle dilation과 step·slope 규약으로 Walk edge를 만든다.
- 옥탄트 내부 StaticNavComponent와 동굴 입구·정적 보행면 portal을 굽는다.
- 8-slot 게시 때 지각 seam node를 연결하고 StaticNavComponent를 병합한다.
- 현재 활성 Walk link를 반영한 ReachabilityGroup과 단조 증가 `ConnectivityGraphVersion`을 게시한다.
- 셀·component·seam·portal·group을 에디터와 `-game`에서 진단할 수 있게 한다.

Phase 7b 소유인 일반 A*, 다중 프레임 request scheduler, path cache, waypoint following, Enemy 소비자 전환, Pod 재귀속과 근접 슬롯 도달성은 구현하지 않는다. 7a cooked load와 8-slot 연결성 검증이 끝난 뒤 실제 graph view를 기준으로 형식을 정한다.

## 2. 현재 코드 기준선

- `ULNPOctantSurfaceData`는 `NavigationPayload`와 `TraversalPayload` 경계를 이미 갖지만 베이커가 두 stream을 비운다. 현재 `DataVersion`은 4다.
- `FLNPOctantSurfaceBaker`는 Support Layer raster와 Spawn stream을 만들며 source mesh, face→Layer 표, 지각 seam hash를 이미 보유한다.
- `FLNPSurfaceDataSnapshot`은 Support·Spawn만 게시한다. 같은 SurfaceData asset을 쓰는 slot은 decoded payload를 공유하고 slot transform만 따로 가진다.
- `LNPCrustAtlas::ComputeSeamPairs`와 고정 8-slot 회전표가 12개 world seam의 slot·edge·역순 관계를 제공한다.
- PureEntity는 `FLNPSurfaceHandle`을 유지하지만 현재 Nav node·StaticNavComponent를 기록하지 않는다.
- 회귀 fixture는 지각, 겹친 섬, 동굴 입구, 정적 나무·바위, 세 이음매와 꼭짓점을 이미 제공한다. Phase 7에서 상징 이름이던 `NC_*`를 실제 component oracle로 바꾼다.

## 3. 확정 구현 규약

### 3.1 Grid와 agent profile

- Nav Layer는 Support Layer와 1:1이다. asset의 `LocalNavLayerId`는 같은 `LocalLayerId`를 사용한다.
- Support와 같은 옥탄트 삼각 격자 좌표 `(i, j, k=N-i-j)`를 쓰되 Nav 분할 수는 독립적으로 저장한다.
- 지각 목표 간격은 200cm, 비지각 Layer 목표 간격은 100cm로 시작한다. 큰 지각의 node 수를 억제하면서 폭 400cm 동굴 통로에는 dilation 뒤 중심 경로가 남아야 한다.
- 이웃은 삼각 격자의 6방향 `(±1,0)`, `(0,±1)`, `(+1,-1)`, `(-1,+1)`이다. 사각 격자의 4/8방향 규약을 사용하지 않는다.
- 7a는 지상 agent profile 하나만 굽는다. 기본값은 반지름 50cm, 반높이 88cm, 최대 step-up 45cm, 최대 step-down 60cm, walkable normal dot 0.71이다. production 지상 적이 이 profile보다 크면 cook validation 오류로 막고 다중 profile은 실제 콘텐츠 요구가 생길 때 추가한다.
- node는 대표 방향에서 같은 Support Layer를 가져야 한다. capsule clearance, slope와 step을 통과한 node만 walkable이고 두 endpoint와 그 사이가 모두 통과한 경우에만 edge를 연다. step과 경사는 구분한다. 인접 두 점의 지역 Up 높이 차는 `max(step 한도, 수평 거리 × tan(walkable 최대 경사))` 안이어야 하며, node 끝점(사전 필터)과 50cm exact Support 점 사이(본 판정)에 모두 적용한다(`design/GroundNavigation.md`).
- 나무·바위·벽·천장 같은 정적 Blocker는 agent radius로 dilation한다. Decoration, Dynamic, StatefulTraversal, Destructible은 base 정적 Grid에 합치지 않는다.

### 3.2 Node와 Tile 주소

- runtime `FLNPNavNodeRef`는 `RuntimeNavLayerId`, `TileId`, `LocalCellIndex`, `SnapshotGeneration`으로 식별한다. generation은 Surface snapshot의 `uint64` 값을 그대로 써 truncation하지 않는다.
- codec 안에서는 `(LocalNavLayerId, TileId, LocalCellIndex)`만 저장한다. slot과 runtime global layer ID, generation은 게시 때 붙인다.
- Tile은 `(i / 16, j / 16)`의 16×16 블록이다. 삼각형 바깥 좌표와 비보행 셀은 존재하지 않는다.
- `LocalCellIndex = (j % 16) * 16 + (i % 16)`이며 0~255다. tile 목록은 `(TileY, TileX)` 순, 셀은 `LocalCellIndex` 순으로 canonicalize한다.
- Tile은 runtime streaming 단위가 아니다. 지역 revision·무효화와 Phase 10 Cluster 구성의 최소 단위다.
- tile 경계 이웃은 별도 예외 경로가 아니라 6방향 좌표를 다시 node ref로 resolve한다. 같은 규칙으로 옥탄트 내부 tile 경계와 삼각 격자 꼭짓점 경계를 검사한다.

### 3.3 Navigation codec v1

`NavigationPayload`는 little-endian canonical stream이다.

- header: codec version, tile side(16), Layer 수, Tile 수, walkable cell 수, agent profile과 bake 파라미터
- Layer 표: `LocalNavLayerId`, 대응 `LocalLayerId`, subdivisions, Tile 범위, local StaticNavComponent 범위
- Tile 표: canonical `TileId`, `TileX`, `TileY`, cell 범위, 초기 local revision 0
- Cell: `LocalCellIndex`, 6-bit edge mask, clearance class, flags, asset-local StaticNavComponent ID

cell 위치·법선은 중복 저장하지 않는다. `(Layer subdivisions, Tile 좌표, LocalCellIndex)`로 방향을 복원하고 대응 Support Layer에서 지면점·법선을 얻는다. codec은 범위, 정렬, 중복, edge 대칭, 존재하지 않는 endpoint, component ID와 descriptor count를 decode 시 검증한다.

Nav stream 추가로 `FLNPSurfaceBakeHeader::CurrentDataVersion`을 5로, `BakerSchemaVersion`을 4로 올리고 세 production SurfaceData를 모두 다시 굽는다. 구현 단위 3의 edge step 규칙 수정으로 `BakerSchemaVersion`은 5가 됐다(codec은 그대로). `Header.Navigation.ElementCount`는 walkable cell 수다.

### 3.4 Traversal codec v1과 정적 연결성

`TraversalPayload`는 다음 정적 그래프 입력을 저장한다.

- asset-local StaticNavComponent descriptor와 포함 node 수
- 서로 다른 Nav Layer 사이의 정적 Walk portal endpoint 쌍
- 지각 세 변의 ordered seam endpoint(`edge`, seam step, local node)
- portal·seam endpoint의 최소 clearance와 양방향 여부

같은 Nav Layer의 6방향 edge로 먼저 flood fill한다. 동굴 입구나 서로 다른 Support source의 연속 보행면처럼 Layer가 달라도 capsule이 연속 이동할 수 있는 endpoint 쌍은 정적 portal로 연결하고 component를 병합한다. 끊긴 부유섬 사이에는 거리만으로 portal을 만들지 않는다.

게시 때 고정 seam pair 표로 양쪽 endpoint를 맞춘다. 이음매 node는 두 slot에 같은 월드 위치의 사본으로 존재하므로 cross-slot 연결은 비용 0의 seam link이며, link가 생긴 사본의 asset-local component를 runtime component로 union한다.

- 지면 반지름(1cm 초과), 법선(25° 초과), 방향, clearance class, crust Nav 해상도, agent profile이 어긋나면 게시를 실패시킨다. 데이터 자체가 호환되지 않는 경우다.
- 이음매 줄 node의 capsule clearance와 이음매 방향 edge의 sweep은 사본마다 다를 수 있다. 각 slot 베이크 월드에는 자기 옥탄트 지오메트리만 있어 이음매 너머로 걸친 캡슐 절반이 이웃 쪽 나무·바위를 보지 못하기 때문이다. 실제 월드의 충돌은 두 지오메트리의 합집합이므로 양쪽 사본이 모두 통과해야 유효하다(D-060). 한쪽에만 있는 node는 남은 사본을 `BlockedSeamNodes`에 넣고 link하지 않는다. 한쪽만 열린 이음매 방향 edge는 열린 사본을 `BlockedSeamEdges`에 넣는다. 7b 경로 탐색은 두 목록을 제외한다.
- 알려진 근사: 막힌 사본이 bake 시 local component의 유일한 다리였다면 runtime component가 실제보다 크게 합쳐질 수 있다. 도달성 과대 추정이므로 A*가 실패로 확인한다. 이음매로 비스듬히 들어가는 cross edge의 끝 캡슐은 이웃 지오메트리를 부분만 반영한다. 이음매 너머로 약 1.2m보다 깊이 걸친 정적 픽스쳐는 이웃 slot의 안쪽 node를 막지 못하므로, 이음매 근처 정적 배치 금지를 콘텐츠 규칙으로 둔다(`design/TerrainContract.md` §7).
- 베이커 report는 walkable 이음매 좌표 중 clearance로 탈락한 지각 node 수를 콘텐츠 경고로 출력한다.

`Header.Traversal.ElementCount`는 portal과 seam endpoint record 수의 합이다. stream은 component 범위, endpoint 존재, 중복 edge, 정렬, 동일 node self-link와 clearance를 decode 시 검증한다.

### 3.5 ReachabilityGroup과 version

- `StaticNavComponent`는 base Grid edge, 정적 portal, 8-slot seam 연결까지 합친 뒤 확정되며 매치 중 바뀌지 않는다.
- 7a에는 Phase 8 동적 link가 없으므로 초기 `ReachabilityGroup`은 StaticNavComponent와 1:1이다.
- snapshot 게시 때 active link 집합으로 union-find를 새로 만들고 `ConnectivityGraphVersion`을 1로 게시한다. 이후 link 상태가 실제로 바뀔 때만 단조 증가한다.
- group 숫자만 유효성 근거로 쓰지 않는다. 모든 lookup은 `SnapshotGeneration`과 `ConnectivityGraphVersion`을 함께 확인한다.
- node 조회 API는 world 위치와 선호 `FLNPSurfaceHandle`을 받아 같은 slot·Layer의 가장 가까운 walkable node를 제한 반경 안에서 찾는다. 다른 Layer나 다른 component로 임의 스냅하지 않는다.

### 3.6 고정 검증 입력

- codec 합성 fixture: 빈/단일/다중 Tile, 여섯 tile 경계 방향, partial triangle tile, 손상·중복·비대칭 edge
- local component fixture: blocker로 둘로 갈린 한 Layer, 분리 Layer, portal로 이어진 두 Layer, clearance 때문에 삭제된 좁은 통로
- 정적 회귀 LVI 8-slot: 지각은 seam 12개를 거쳐 runtime StaticNavComponent 하나, 모든 끊긴 섬은 지각·서로와 다른 component, 동굴 바닥은 입구 portal로 지각과 같은 component
- 정적 프랍: 나무·바위 cell dilation과 우회 연결성, Decoration 무시
- seam: 변 중점과 여섯 world 꼭짓점에서 양쪽 node·edge 대칭, 누락·모호 endpoint 0
- Spawn stream: 모든 authored Pod anchor와 ground enemy candidate가 같은 Layer의 walkable node로 제한 반경 안에 project됨
- runtime loader: 같은 asset을 쓰는 8-slot decoded payload 공유, runtime layer ID 분리, cooked load 뒤 node/component/group 결과 일치

## 4. 구현 단위

### 구현 단위 0 — 순수 Nav 자료구조와 codec

- [x] `LNPNavData` runtime 순수 타입, 16×16 Tile 주소와 6방향 neighbor helper 구현
- [x] Navigation/Traversal codec v1 encode·decode와 canonical validation 구현
- [x] node ref generation, tile 경계, codec round trip·손상 입력 자동화 추가
- [x] `DataVersion=5`, `BakerSchemaVersion=4` 전환 경계를 마련하되 저장 에셋은 베이커가 두 stream을 생성할 때 함께 갱신

완료 조건: UObject·월드 없이 합성 Nav graph를 encode/decode하고 모든 tile 경계와 invalid payload를 자동화로 고정한다.

결과(2026-09-29): `LNPNavData`와 Nav 자동화 3개를 추가했다. codec은 canonical Layer/Tile/Cell 순서, component node 수, 6방향 reciprocal edge, portal endpoint, ordered seam 좌표, 손상·trailing payload를 검증한다. `LootNPopEditor Win64 Development` 전체 빌드와 `LootNPop.SurfaceNavigation.Nav` 3/3이 통과했다. 기존 SurfaceData를 중간 상태로 무효화하지 않도록 active version은 아직 4/3이고 다음 값 5/4를 명시했다. 구현 단위 1에서 두 stream을 실제 생성하는 변경과 함께 활성화한다.

### 구현 단위 1 — Nav bake와 옥탄트 내부 연결성

- [x] Support source와 정적 Blocker geometry에서 Layer별 coarse Grid와 dilation을 생성
- [x] 6방향 Walk edge, local StaticNavComponent와 Layer 간 정적 portal 생성
- [x] 지각 세 변의 ordered seam endpoint 생성
- [x] 베이크 report에 Layer/Tile/cell/component/portal/seam 수와 stream 크기 추가
- [x] 결정론 베이크와 회귀 fixture local oracle 자동화 추가

완료 조건: 세 SurfaceData의 Navigation/Traversal stream이 비어 있지 않고 반복 베이크 byte가 동일하며 동굴 통로와 프랍 우회 공간이 보존된다.

결과(2026-09-30):

- 순수 `LNPNavBaking`이 Support Atlas와 clearance callback에서 coarse Grid, 6방향 reciprocal edge, local component, Layer portal과 seam endpoint를 만든다. editor 베이커는 exact collision preview world를 Nav와 Spawn이 공유하며 Navigation/Traversal codec v1 payload와 header descriptor를 저장한다.
- 정적 blocker는 반지름 50cm·반높이 88cm gravity-up capsule로 dilation한다. 경사면에서는 floor normal과 gravity Up의 dot으로 캡슐 중심 높이를 보정한다. 같은 Layer edge는 양방향 50cm segment sweep을 통과해야 한다.
- Layer portal 탐색 거리는 800cm다. 적어도 한 endpoint가 grid 경계여야 하고, 거리·양방향 step 뒤 50cm 간격 exact `LNPSurfaceSupport` hit가 연속이며 walkable해야 한다. capsule sweep은 endpoint 직선이 아니라 이 exact Support hit point·normal polyline을 따라간다. 따라서 떨어진 평면을 거리만으로 잇지 않으면서 20° 동굴 통로와 구면 지각 전이를 보존한다.
- 회귀 fixture는 69,378 cells, 7 local components, portal 2개(지각 0↔통로 3, 공동 2↔통로 3), seam endpoint 1,107이다. 나무·바위 중심 node는 dilation으로 제거되고 Decoration node는 남으며 지각 component는 갈라지지 않는다.
- 세 SurfaceData를 `DataVersion=5`, `BakerSchemaVersion=4`와 최신 settings hash로 다시 구웠다. Crust는 68,500 cells/5 components/0 portals/1,107 seams, Regression은 위 수치, Meadow는 67,729 cells/5,151 components/4 portals/1,058 seams다(구현 단위 3의 step 규칙 수정 뒤 91 components).
- `LootNPopEditor Win64 Development` 전체 빌드, `LootNPop.SurfaceNavigation.Nav` 4/4(`Saved/Logs/Phase07a_Unit1_NavTests_Final.log`), 저장본과 두 번 베이크를 비교하는 `Bake.OctantBakeDeterministic` 1/1(`Phase07a_Unit1_Deterministic_Final.log`)이 통과했다. 최종 재베이크 로그는 `Phase07a_Unit1_Rebake_Final.log`다.

### 구현 단위 2 — runtime load와 8-slot 조립

- [x] loader가 두 stream을 필수 검증·decode하고 같은 asset의 decoded Nav를 공유
- [x] slot별 runtime Nav Layer ID와 node ref 생성
- [x] seam endpoint를 연결하고 runtime StaticNavComponent를 병합
- [x] immutable Nav/connectivity view를 Support snapshot과 같은 release publication에 포함
- [x] load 시간, serialized bytes와 decoded resident bytes에 Nav를 포함

완료 조건: 에디터와 cooked 실행에서 같은 8-slot node·edge·component 결과가 게시되며 실패 시 Support만 부분 게시하지 않는다.

결과(2026-10-01):

- 순수 `LNPNavRuntime`이 8-slot 입력에서 `FLNPNavSnapshot`을 만든다. runtime Nav Layer ID는 `SlotLayerBase[Slot] + LocalNavLayerId`, runtime component는 portal·seam link union 뒤 (slot, local id) 첫 등장 순으로 번호를 매긴다. `MakeRuntimeNodeRef`·`ResolveRuntimeNodeRef`·`GetRuntimeStaticComponent`가 generation을 검사한다.
- `ValidateAndBuildSnapshot`은 Navigation/Traversal을 필수 payload로 hash·decode·descriptor count 검증하고 asset별로 한 번만 decode한다. Nav view는 같은 `FLNPSurfaceDataSnapshot`에 들어가며, 모든 실패 경로가 부분 조립 결과를 비운다.
- 이음매 비대칭은 D-060 규칙으로 처리한다. production Meadow 8-slot은 seam link 4,036, 막힌 이음매 node 392·edge 16이다. 원인은 모두 clearance이며 세 변에 1~3 step씩 흩어져 있다(asset당 49개, 베이크 경고와 일치).
- 회귀 fixture 8-slot: runtime Layer 56, local component 56 → runtime 33(지각·동굴 1개 + slot당 끊긴 섬 4개), seam link 4,428, 막힘 0, seam 반지름 차 0cm.
- Meadow 8-slot: runtime Layer 88, local component 41,208 → runtime 41,169, Nav resident 0.74MiB, 전체 decoded resident 3.68MiB, serialized 3.43MiB. 에디터 `-game` validateBuild 20.6ms(Support·Spawn decode 포함), 게시까지 597ms(에디터 async load 지배, 이전 641ms와 같은 수준).
- `LootNPopEditor Win64 Development` 전체 빌드, 전체 SurfaceNavigation 자동화 69/69, 에디터 `-game` 1P와 리슨 2P 스모크를 통과했다. 호스트·게스트가 같은 Nav 결과를 게시했고 `ProbeSurfaceData`(Nav generation 포함)·`ProbePanels` PASS, ensure·crash 0이다. cooked 검증은 구현 단위 4에서 한다.

### 구현 단위 3 — 도달성 API와 debug visualization

- [x] ReachabilityGroup과 `ConnectivityGraphVersion` 게시·검증 API 구현
- [x] world position/Surface handle→node projection과 node→world support 조회 구현
- [x] cell, Tile, StaticNavComponent, portal, seam, ReachabilityGroup 시각화·보고 명령 추가
- [x] 지각·섬·동굴·프랍·Spawn anchor의 8-slot oracle 자동화 추가

완료 조건: 끊긴 섬은 즉시 도달 불가, 동굴과 이웃 slot 지각은 도달 가능으로 판정되고 진단 그림과 보고서가 같은 결과를 보인다.

결과(2026-10-01):

- `FLNPNavSnapshot`에 `ReachabilityGroupByStaticComponent`(7a는 1:1), `ReachabilityGroupCount`, `ConnectivityGraphVersion=1`, 정렬된 막힘 node key를 추가했다. 새 `LNPNavQuery`가 `ProjectToNode`(handle의 slot·Layer 안, 기본 반경 300cm, 막힌 이음매 node 제외), `GetNodeSupport`, `GetStaticComponent`, `GetReachabilityGroup`, `TestReachability`(Reachable/Unreachable/Stale)를 제공한다.
- 진단 명령 `LNP.SurfaceNav.NavReport [Top]`(slot별 cell·Tile·edge·고립 cell·component·portal·seam, 막힘, group·version, component 크기 분포, 폰 node)와 `LNP.SurfaceNav.DrawNav [component|tile|group] [Radius] [Seconds]`(node·edge를 ID 색으로, 막힘 빨강, seam link 청록, portal 자홍)를 추가했다. 두 명령은 같은 ID 해시 색과 같은 폰 node 줄을 찍는다. 부하 harness가 종료 전에 둘을 실행한다.
- oracle 자동화: `Nav.RegressionReachability`는 회귀 8-slot에서 group 33개, 지각 8 slot 도달, 섬 32개 서로 다른 group·지각 도달 불가, 섬 윗면의 지각 Layer 스냅 거부, 나무·바위 둘레 500cm 원 12점 지각 도달, Decoration 인접 node, anchor 24개(동굴 도달·섬 불가), 12 seam 중점 양쪽 사본과 6 꼭짓점 네 사본, random 후보 10,616개 전부 투영을 검사한다. `Nav.ProductionSpawnProjection`은 Meadow 후보 9,990개 전부 투영(P50 55.8·P99 124.3·최대 290.5cm)을 검사한다. `Runtime.NavAssembly`에 group·version·stale·막힌 이음매 투영 제외를, `Nav.StepAndSlope`에 경사로 연결·절벽 분리를 추가했다.
- **단위 1 베이커 결함 수정:** 스모크의 `DrawNav`에서 폰 주변 node 395개에 edge 227개, 폰이 node 3개짜리 component에 서는 현상을 발견했다. 같은 Layer edge의 `CanStep`이 200cm 끝점 높이 차를 step 한도 45cm와 비교해 약 12.7° 이상 경사를 모두 끊고 있었다. Meadow slot 0 지각에서 두 walkable node 사이 끊긴 edge 32,090개 중 24,490개가 이 판정이었고 전부 45° walkable 경사 안이었다. §3.1의 step·경사 구분 규칙으로 고치고 같은 Layer edge도 portal과 같은 exact Support polyline 검사·sweep을 쓰게 했다. `BakerSchemaVersion=5`로 세 asset을 재베이크했다. Meadow local component 5,151→91, runtime 41,169→689, slot당 고립 cell 4,479→54, 끊긴 인접 edge 32,090→3,044다. 회귀·Crust fixture 수치는 변하지 않았다.
- 전체 SurfaceNavigation 72/72, 에디터 `-game` 1P와 리슨 2P host/guest에서 같은 Nav(component·group 689, version 1)를 게시했고 `ProbeSurfaceData`·`ProbePanels` PASS, ensure·crash 0이다.

### 구현 단위 4 — cooked Gate와 7b 인계

- [x] 전체 SurfaceNavigation 자동화와 `LootNPopEditor Win64 Development` 전체 빌드 통과
- [x] Development cook/package에서 DataVersion 5 SurfaceData load·게시 검증
- [x] `-game` 1P와 리슨 서버 2P에서 asset 조합, generation, component/group 결과 일치 검증
- [x] Nav serialized/resident memory와 publish 시간 기록
- [x] 실제 node 수·분기 수·component 크기 분포를 7b scheduler/cache 입력으로 기록

완료 조건: 7a cooked load와 8-slot 연결성 검증이 끝나고, 7b가 추정이 아니라 실제 graph 통계로 시작할 수 있다.

결과(2026-10-01):

- 전체 SurfaceNavigation 72/72와 에디터 전체 빌드, Win64 Development BuildCookRun(972 packages, 오류 0)이 통과했다.
- 패키지 1P와 리슨 2P host/guest가 DataVersion 5·`BakerSchemaVersion=5` Meadow 8-slot을 generation 1로 게시했고 runtime component·group 689, version 1, seam link 4,036, 막힘 392/16이 모두 같다. `ProbeSurfaceData`·`ProbePanels`·`ProbeFaceIndex`·`ProbeSourceKeys`와 LoadBaseline이 PASS이고 ensure·assert는 0이다.
- cooked load: elapsed 34~35ms, validateBuild 21~24ms, serialized 3.39MiB, decoded resident 3.56MiB, Nav resident 0.59MiB.
- 7b 입력: node 541,832, grid edge 1,504,960, seam link 4,036, portal 32. grid 차수 평균 약 5.55(73.1%가 6), 가장 큰 지각 component 510,352 node(94.2%). Spawn 후보 투영 P50 55.8·P90 82.9·P99 124.3·최대 290.5cm, 고립 node 적중 5·소형 component(≤10 node) 적중 25/9,990. `NavReport`에 `gridDegree` 줄을 추가했다.
- 7b 설계 질문으로 남긴 것: 가장 가까운 node 규칙이 고립 node를 고른다. 패키지 2P 호스트 폰도 node 1개짜리 component에 투영됐다. 상세 수치는 `history/Phase07_Log.md` 2026-10-01 "구현 단위 4"에 있다.

## 5. 전체 완료 조건

- 옥탄트별 Navigation/Traversal stream이 결정론적으로 베이크되고 stale 검출에 포함된다.
- 16×16 Tile과 삼각 격자 6방향 이웃이 모든 내부·경계 좌표에서 대칭이다.
- 정적 blocker dilation 뒤 프랍 우회 공간과 동굴 통로가 기대대로 남는다.
- local StaticNavComponent, Layer 간 portal과 8-slot seam 연결이 회귀 oracle과 일치한다.
- 끊긴 부유섬은 별도 ReachabilityGroup이고 지각·동굴·이웃 slot은 실제 Walk 연결만큼 병합된다.
- 모든 authored Pod anchor와 지상 Spawn candidate가 유효한 Nav node에 대응한다.
- immutable snapshot의 generation과 connectivity version으로 stale 조회가 차단된다.
- debug visualization과 수치 보고가 cell·Tile·component·portal·group을 식별한다.
- 전체 자동화, 전체 에디터 빌드, Development package, `-game` 리슨 서버 2P 스모크가 통과한다.

위 조건을 모두 충족했다(2026-10-01). 증거는 §4 각 구현 단위 결과와 `history/Phase07_Log.md`에 있다.
