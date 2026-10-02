# Phase 7b — 경로 실행

> 상태: 완료(2026-10-02) — 구현 단위 0~5와 자연·합성 추격 700마리 반복 Gate 통과
> 예상 범위: 3~4세션
> 선행 조건: Phase 7a Nav 데이터 기반(완료, 2026-10-01)

## 1. 목표

7a가 게시한 8-slot Nav snapshot 위에서 지상 NPC가 실제 경로를 계산하고 따라간다.

- A* 확장 루프가 쓰는 조밀 graph view를 게시한다.
- chord heuristic 일반 A*를 frame 예산 안에서 여러 프레임에 나눠 실행한다.
- 요청은 snapshot generation, `ConnectivityGraphVersion`, 지역 revision으로 검증한다.
- 결과 경로를 단순화해 cache에 넣고 여러 개체가 나눠 쓴다.
- 스폰 뒤 움직이지 않는 Pod가 덮는 Nav node를 runtime overlay로 막는다.
- PureEntity와 ActorPromoted가 같은 경로 추종 결과를 받는다.
- 근접 슬롯 배정에 도달성을 넣고, 넉백으로 다른 group에 떨어진 적을 Pod에 재귀속한다.

Phase 8의 Conditional Patch, 파괴 overlay, 동적 link 때문에 바뀌는 ReachabilityGroup 재계산은 이 Phase에서 하지 않는다. flow field와 계층형 탐색은 Phase 10이다(D-044). 7b에서는 Phase 10 비교에 쓸 요청 캡처만 남긴다.

## 2. 현재 코드 기준선

- `FLNPNavSnapshot`(`SurfaceNavigation/LNPNavRuntime.h`)은 slot별 Layer base, runtime component, seam link 4,036개, 막힌 이음매 node·edge, component별 ReachabilityGroup과 version 1을 게시한다. Surface snapshot과 같은 immutable 객체다.
- `LNPNavQuery`는 `ProjectToNode`, `GetNodeSupport`, `GetStaticComponent`, `GetReachabilityGroup`, `TestReachability`를 제공한다. node를 해석할 때마다 Tile 표를 선형 탐색하고 Support Layer를 보간한다. 검증·진단에는 충분하지만 A* 확장 루프에는 너무 느리다.
- 추격: `ULNPEnemyTargetFollowProcessor`(Behavior 그룹)와 `FLNPEnemySteeringTask`가 매 프레임 `FMassMoveTargetFragment::Center`를 타겟 앞 정지 거리 지점으로 쓴다. `ULNPEnemyMovementProcessor`는 그 점을 향해 접평면에서 직진한다. Actor 승격 개체도 같은 방향으로 `SetAIMoveInput`을 받는다.
- 배회: `FLNPEnemyIdleTask`가 Parent Pod 주변 임의 방향을 `ProjectWanderTarget`으로 현재 Layer에 투영한다. 도달하지 못하면 `bWanderTargetTimedOut`으로 다시 뽑는다.
- 적은 `FLNPEnemyFragment::SurfaceHandle`, Pod는 `FLNPLootPodFragment::SurfaceHandle`을 갖는다. **플레이어 Mass 엔티티에는 Surface handle이 없다.** ActorPromoted 적은 Mover floor hit을 `ULNPHitIdentitySubsystem::ResolveHit`으로 handle에 연결한다(Phase 6 구현 단위 3). 플레이어 handle도 같은 경로로 만들 수 있다.
- Pod는 스폰 뒤 움직이지 않고, 루팅이 끝나 Popped가 되면 엔티티가 사라진다. collision proxy(`LNPStaticBlocker` 캡슐, D-047)는 엔티티 수명에 묶여 있다.
- 슬롯: `ULNPTargetingSubsystem`이 플레이어당 근접 10·원거리 20 슬롯을 관리하고 도달성은 보지 않는다.

### 7a 인계 통계(production Meadow 8-slot)

- node 541,832(slot당 67,729), grid edge 1,504,960, seam link 4,036, portal 32
- grid 차수 평균 약 5.55이고 73.1%가 6방향 전부 열려 있다. 분기 수는 사실상 6이다.
- runtime component 689개 중 지각 주 component가 510,352 node(94.2%)다. 같은 group 안의 탐색 1회가 최악의 경우 약 51만 node를 확장할 수 있다.
- 1 node component 432개, 2~10 node 200개. 가장 가까운 node 규칙은 이런 고립 node도 고른다. 에디터와 패키지의 2P 호스트 폰이 모두 1 node component에 투영됐다.

## 3. 확정 구현 규약

### 3.1 조밀 graph view

A*는 `FLNPNavNodeRef`를 직접 해석하지 않고 게시 때 만든 조밀 index를 쓴다. `design/GroundNavigation.md`의 `FLNPNavGraphView` 역할이다.

- **asset 단위 공유 데이터**: 같은 SurfaceData를 쓰는 slot은 한 벌을 공유한다(7a의 decode 공유와 같다).
  - asset-local 조밀 node index. 순서는 codec canonical 순서(Layer → Tile → cell)다.
  - node별 slot local 지면점(`FVector3f`). load 때 Support Layer 보간으로 한 번 복원한다.
  - node별 같은 Layer 6방향 이웃의 조밀 index(없으면 `INDEX_NONE`)와 edge cost
  - node별 asset-local component ID
- **runtime 전역 index**: `SlotNodeBase[Slot] + AssetLocalIndex`. slot 사이 이동은 seam link와 portal의 추가 인접 표(CSR)로만 한다.
- 막힌 이음매 node와 edge(D-060)는 graph view를 만들 때 인접에서 뺀다.
- 실측 resident는 Meadow 8-slot 전체 4.10MiB다(asset 하나 공유, node 48B와 Tile·cell 조회표 포함, 2026-10-01). load 보고의 Nav resident에 포함된다.
- `FLNPNavNodeRef`와 조밀 index는 양방향으로 변환할 수 있어야 한다. 경로 결과, cache key, overlay는 조밀 index를 쓰고 외부 API와 진단은 node ref를 쓴다.

### 3.2 A* 비용과 휴리스틱

- edge cost는 두 node 지면점의 3D chord 길이에 비용 배율을 곱한 값이다. 배율은 항상 1 이상이다. 7b의 배율은 경사 가중 하나뿐이다(평지 1.0에서 walkable 한계 경사 1.5까지 선형, 초기값).
- seam link cost는 0이다. 같은 월드 위치에 있는 사본이기 때문이다. portal cost는 두 endpoint 지면점의 chord 길이다.
- 휴리스틱은 현재 node 지면점과 목표 node 지면점 사이의 3D chord다(D-040). 모든 edge cost가 chord 이상이므로 하한이 성립한다.
- 확장 순서는 결정론적이어야 한다. 같은 f면 g가 큰 쪽을, 그다음 조밀 index가 작은 쪽을 먼저 연다. 같은 입력은 같은 경로를 낸다.
- 시작과 목표의 ReachabilityGroup이 §3.3 재선택 뒤에도 다르면 탐색하지 않고 바로 `Unreachable`을 반환한다.

### 3.3 시작·목표 스냅 정책

기본은 7a처럼 가장 가까운 walkable node다. 가장 가까운 node끼리 group이 어긋날 때만 주변에서 group이 맞는 짝을 다시 고른다(D-062, 사용자 제안 2026-10-01).

1. 시작은 개체의 현재 handle Layer에서, 목표는 목표의 handle Layer에서 각각 가장 가까운 node를 고른다. 두 node의 group이 같으면 그대로 쓴다.
2. group이 다르면 시작 주변과 목표 주변의 스냅 반경(기본 300cm) 안에 있는 node들의 group을 모은다. 양쪽에 공통으로 있는 group 중에서 두 스냅 거리의 합이 가장 작은 (시작 node, 목표 node) 짝을 고른다. 합이 같으면 조밀 index가 작은 쪽을 고른다.
3. 공통 group이 없으면 `Unreachable`이다. A*는 돌리지 않는다.

- 이 재선택은 A*를 다시 돌리는 것이 아니라 두 투영 창 안의 group 비교다. A*는 짝이 정해진 뒤 한 번만 돈다.
- 1 node 고립 component(Meadow 432개)나 프랍 사이 작은 틈(2~10 node, 200개)에 먼저 스냅돼도 반경 안에 요청자와 같은 group이 있으면 그쪽으로 옮겨 간다. 끊긴 작은 자리에 실제로 선 목표에 요청자도 같은 자리에 있으면 1단계에서 그대로 성립한다. 따라서 임의의 component 크기 기준값을 두지 않는다.
- 끊긴 부유섬 위 목표는 보통 반경 안에 지각 node가 없으므로 `Unreachable`이다.
- **접근점**(D-063): `Unreachable` 결과는 요청자 group의 node 중 목표에 가장 가까운 node를 `ApproachRadius`(초기 3,000cm) 안에서 함께 돌려준다. 후보는 같은 Layer 6방향이 모두 열린 내부 node로 제한해 가장자리 바로 위를 피한다. 반경 안에 없으면 접근점도 없다. 접근점은 도달 가능 판정이 아니므로 슬롯·재귀속에는 쓰지 않는다.
- 스냅 거리만큼은 이동 단계의 충돌 처리로 걷는다.
- 근접 슬롯 도달성(§3.8)과 Pod 재귀속도 같은 함수를 쓴다. 판정 경로가 둘이면 "슬롯은 받았는데 길은 없음"이 생긴다.
- 어느 한쪽 반경 안에 node가 아예 없으면 `NoNode`다. 소비자는 `Unreachable`과 같이 처리하되 통계를 따로 센다.
- 조밀 view에서는 투영이 격자 좌표 창 안의 index 조회가 되므로, Tile 선형 탐색을 하지 않는 빠른 경로로 다시 구현한다. 7a `ProjectToNode`는 진단과 oracle 비교용으로 남긴다.

### 3.4 Request lifecycle과 scheduler

- 경로 요청은 서버 전용이다. 게스트는 경로를 계산하지 않고 복제된 위치와 행동 상태만 받는다.
- `ULNPNavPathSubsystem`(world subsystem)이 요청 대기열, scratch pool, 경로 pool, cache를 소유한다. 실제 로직은 UObject가 없는 순수 코어 `FLNPNavPathScheduler`(`SurfaceNavigation/LNPNavPathScheduler.h`)에 두고, subsystem은 서버에서 매 프레임 게시된 snapshot으로 코어를 tick하고 CVar를 반영하는 얇은 창구다. 자동화는 코어를 회귀·production snapshot으로 직접 검사한다.
- 호출은 모두 게임 스레드다. owner(`FMassEntityHandle`)마다 최신 요청 하나만 추적한다. 새 요청을 내면 이전 요청은 `Cancelled`가 되고, 소비자는 `GetResult(Owner, Serial)`로 자기 serial의 상태를 조회한다. 사라진 owner는 tick 첫 단계에서 정리한다.
- 요청 상태: `Queued → Running → Succeeded | NoPath | Unreachable | NoNode | Stale | Cancelled`
  - `Running`은 여러 프레임에 걸칠 수 있다. 시작할 때의 `SnapshotGeneration`, `ConnectivityGraphVersion`, overlay revision을 보존한다.
  - 프레임을 넘겨 재개할 때 셋 중 하나라도 바뀌었으면 결과를 섞지 않고 `Stale`로 끝낸다. 요청자는 다음 프레임에 다시 요청한다.
  - 요청은 owner entity와 serial을 갖는다. owner가 사라졌거나 더 새 요청을 냈으면 결과를 버린다(`Cancelled`).
- 예산(구현 단위 1 확정 초기값, 부하 조건 판정은 구현 단위 5):
  - 프레임 전체 확장 예산 `LNP.SurfaceNav.NavExpansionsPerFrame` 4,000. 구현 단위 0 측정(확장당 0.38us)으로 계획 초안 8,000(약 3ms)을 절반으로 줄였다.
  - 시작·목표 스냅(`ResolveEndpoints`, P95 약 8us)은 요청마다 확장 16개로 환산해 같은 예산에서 뺀다. cache hit·도달 불가처럼 탐색이 없는 요청이 몰려도 프레임 비용이 예산에 묶인다.
  - 시작 차감과 실제 확장 수의 합은 프레임 예산을 넘지 않는다. 잔여 예산이 시작 비용보다 작으면 큐를 유지하고, 실행 중 요청이 잔여 예산보다 많으면 앞의 요청에만 확장을 배정한다. 프레임 예산 자체가 시작 비용보다 작으면 새 요청은 설정이 올라갈 때까지 대기하며 예산은 누적하지 않는다. 시작·종료·병렬 대기 시간의 상한은 별도 과제다.
  - 한 프레임 안에서 "대기열 시작 → 실행 중 요청을 남은 예산의 균등 몫으로 병렬 확장 → 끝난 요청 정리"를 예산이 남는 동안 반복한다. 끝난 scratch를 같은 프레임에 다시 쓰므로 짧은 요청은 프레임당 scratch 수보다 많이 끝난다. 대기열은 빈 scratch가 있을 때만 꺼낸다.
  - 요청당 누적 확장 상한 `LNP.SurfaceNav.NavMaxExpansionsPerRequest` 30,000. 넘으면 `NoPath`로 끝낸다. 7a가 알려 둔 group 과대 추정(막힌 이음매가 유일한 다리였던 경우)의 최악 탐색을 여기서 끊는다.
  - 동시 실행 요청 수는 scratch 수(`LNP.SurfaceNav.NavScratchCount`, 초기 4)로 제한한다. 각 scratch는 요청 하나를 맡아 worker에서 병렬로 돈다(`LNP.SurfaceNav.NavParallelSearch`). 결과는 병렬 여부·예산 분할과 무관하다.
- 도달 불가(`Unreachable`·`NoNode`)여도 요청에 `ApproachRadius`가 있고 접근점이 있으면 접근점까지 A*를 돌려 경로를 붙인다(D-063). 상태는 도달 불가 그대로이며, 접근점 경로는 cache에 넣지 않는다.
- scratch는 전역 node 수 크기의 g·parent·방문 stamp 배열이다(541,832 × 12B, 약 6.2MiB). stamp를 쓰므로 요청마다 배열을 비우지 않는다. 4개면 약 25MiB이며, 측정에서 문제가 되면 희소 해시 scratch와 비교한다.
- 요청 우선순위: 새 추격 요청 > 경로 무효화에 따른 재계획 > 배회·재귀속·접근점 이동. 같은 우선순위는 먼저 온 순서다.

### 3.5 경로 결과·단순화·cache

- A* 결과 node 열을 **Nav 직선 보행 검사**로 단순화해 waypoint 열을 만든다. 검사는 두 지면점 사이 대원호를 격자 간격의 절반으로 샘플링하고, 연속 샘플이 edge로 이어진 walkable node에 떨어지는지 본다. 같은 검사를 "직선 경로 우선"(`design/GroundNavigation.md`)에도 쓴다.
- waypoint는 지면점 world 위치와 조밀 index를 가진다. 경로는 immutable `FLNPNavPath`이고 handle은 thread-safe 공유 포인터(`FLNPNavPathPtr`)다. 별도 index pool 대신 참조 수가 수명을 정한다. 개체는 handle과 현재 waypoint index만 가진다. 경로 하나를 여러 개체가 공유할 수 있다.
- 경로는 통과한 Tile과 그 Tile의 overlay revision 목록을 함께 저장한다(`TraversedRevisionFingerprint`, `design/GroundNavigation.md`). Tile은 A* node가 아니라 단순화된 waypoint 구간의 직선 보행 cell에서 모은다. 개체가 실제로 걷는 곳이기 때문이다. Tile 주소는 `(RuntimeNavLayerId << 16) | TileId`다(`LNPNavGraph::GetTileKey`).
- overlay revision view(`FLNPNavOverlay`: 전역 revision과 Tile별 revision, `SurfaceNavigation/LNPNavOverlay.h`)는 구현 단위 1에서 먼저 정의했다. overlay가 없으면 모든 revision이 0이다. 막힘 내용과 생산자는 구현 단위 2에서 더한다.
- cache key: `(StartTile, GoalTile, SnapshotGeneration, ConnectivityGraphVersion)`. agent profile은 snapshot당 하나라 generation에 포함된다. hit이 되려면 저장된 Tile revision이 현재와 모두 같아야 한다. hit한 개체는 자기 시작 node에서 경로 첫 waypoint까지, 경로 마지막 waypoint에서 자기 목표 node까지 직선 보행 검사를 통과해야 그 경로를 쓴다. Tile이 16×16 cell(지각 약 32m)이라 목표 쪽 확인도 필요하다. 실패하면 개별 요청으로 돌아간다.
- cache 용량과 수명(초기값): 항목 256개, LRU. 목표 Tile이 바뀌는 추격 수요에서 실제 hit율은 구현 단위 5에서 잰다. hit율이 낮으면 cache를 지우지 않고 Phase 10 비교 입력으로 기록한다.

### 3.6 Pod runtime blocker overlay

사용자 요구(2026-10-01)를 7b에 넣는다(D-061, 2026-10-01 확정). revision 검증이 실제 생산자를 가져야 7b의 무효화 경로를 끝까지 검증할 수 있고, Phase 8은 같은 overlay를 Conditional Patch로 넓힌다.

- overlay는 조밀 전역 index의 막힘 bitset과 Tile별 revision이다. Surface snapshot과 따로 게시하는 작은 immutable 객체이며, 바뀔 때마다 새 객체로 교체하고 전역 overlay revision을 올린다.
- Pod 하나가 막는 node는 지면점이 Pod 중심에서 `proxy 반지름 + agent 반지름` 안에 드는 같은 slot·Layer node다. Pod의 `SurfaceHandle` Layer를 쓴다.
- 막힌 node가 이음매 사본이면 seam link로 이어진 반대 slot 사본도 함께 막는다(D-060 규칙과 같은 방향).
- 생산자: Mass spawn이 Pod 배치를 끝낸 뒤 한 번에 추가하고, Pod가 Popped로 사라질 때 제거한다. 추가·제거는 게임 스레드에서 새 overlay를 만들어 게시한다.
- **overlay는 ReachabilityGroup을 바꾸지 않는다.** Pod가 좁은 통로를 완전히 막는 경우는 A*가 `NoPath`로 확인한다. group 재계산은 Phase 8 동적 link에서 한다.
- A*는 막힌 node를 열지 않는다. 경로 추종 중인 개체는 전역 overlay revision이 바뀐 프레임에만 자기 경로의 Tile revision 목록을 대조하고, 다르면 재계획을 요청한다.
- 게스트는 overlay를 만들지 않는다. 경로 소비자가 서버뿐이다.

### 3.7 경로 추종과 소비자

- 새 `FLNPEnemyPathFragment`: 경로 handle, waypoint index, 요청 serial·상태, 마지막 요청 시각, 목표 Tile, 재계획 사유 통계
- `FMassMoveTargetFragment`는 계속 **의미상 목표**(추격 정지점·배회 목표)를 뜻한다. 도착 판정과 `DistanceToGoal`도 지금처럼 목표 기준이다. 경로 추종은 별도 **조향점**만 제공한다. 이동 프로세서는 경로가 유효하면 조향점 방향으로, 아니면 지금처럼 목표 방향으로 걷는다. StateTree Task와 TargetFollow는 고치지 않는다.
- 조향점: 현재 waypoint를 향하다가 접평면 거리 `WaypointAcceptRadius`(초기 100cm) 안에 들면 다음 waypoint로 넘어간다. 다음 waypoint가 직선 보행 검사로 보이면 앞당긴다.
- 요청 조건(직선 경로 우선):
  1. 목표까지 Nav 직선 보행 검사가 통과하면 경로 없이 기존처럼 직진한다.
  2. 막히면 경로를 요청한다. 추격은 목표 Tile이 바뀌었거나 목표가 경로 끝에서 `RepathGoalDrift`(초기 400cm) 넘게 벗어났을 때만 다시 요청한다. 최소 재요청 간격은 0.5초다.
  3. 진행 교착(기존 배회 timeout을 일반화)이 생기면 경로를 버리고 다시 요청한다.
- 배회: 투영한 배회 목표가 Parent Pod group 밖이거나 제외 규칙에 걸리면 버리고 다시 뽑는다. 직선 보행 검사를 통과하는 후보를 우선하고, 막히면 경로를 요청한다.
- PureEntity와 ActorPromoted는 같은 조향점을 쓴다. Actor는 기존 `SetAIMoveInput` 계산이 조향점 방향을 받는다. LOD 전환 때 경로 fragment는 엔티티에 남으므로 인계할 것이 없다.
- 공중 개체(넉백·낙하)는 경로를 따르지 않는다. 착지하면 경로를 버리고 §3.8을 먼저 처리한다.
- 비행 NPC(`FLNPEnemyFlyingTag`)는 이 경로의 대상이 아니다(D-019).

### 3.8 도달성 소비자

- **플레이어 Surface handle**: 게임 스레드가 플레이어 Mover floor hit을 `ResolveHit`으로 해석해 플레이어 Mass 엔티티의 새 fragment에 handle과 조밀 node·group을 기록한다. 공중이면 마지막 접지 값을 유지하되 공중임을 표시한다.
- **근접 슬롯 배정**: 근접 슬롯은 §3.3 스냅 규칙으로 적과 플레이어 사이에 공통 group 짝이 있을 때만 배정한다. 이미 슬롯을 가진 근접 적이 도달 불가가 되면 슬롯을 반납한다. 반납은 공중 순간이 아니라 플레이어의 접지 group이 바뀐 뒤 `SlotReachabilityGrace`(초기 1.5초)가 지났을 때 한다. 점프 한 번에 슬롯이 흔들리지 않게 하기 위해서다. 원거리 슬롯은 기존 LoS·사거리 기준을 유지한다(`design/MovementIntegration.md` "슬롯과 도달성").
- 슬롯을 받지 못한 근접 적은 Alert에 머문다. 대상이 도달 불가이고 접근점이 있으면 Alert 중 제자리 대신 접근점까지 경로로 이동해 선다(D-063). 대상이 도달 가능하지만 슬롯이 찬 적은 지금처럼 제자리에서 기다린다. Alert 인내 시간과 포기 규칙은 바꾸지 않는다.
- 도달 불가 대상의 원거리 적은 사거리 안에 들거나 접근점에 닿을 때까지 접근점으로 이동한다. 사격 조건은 기존 LoS·사거리다.
- 접근점은 목표가 움직여 목표 Tile이 바뀔 때만 다시 계산한다(추격 재요청 조건과 같다).
- **Pod 재귀속**(D-016): 착지 이벤트에서 D-062로 Parent Pod 도달성을 검사하고 도달 가능하면 유지한다. 다르면 도달 가능한 활성 Pod 중 chord 거리 최소 후보로 고른다. 후보 축소 거리와 최종 근사 비용이 같으므로 가까운 4개 임시 목록 없이 같은 최소 후보를 바로 고른다. 실제 A* cost 비교는 구현 단위 4 측정에서 필요할 때만 넣는다. 후보가 없으면 `Orphaned`로 두고 착지점 중심·현재 component 안에서만 배회하며 1초 간격으로 재귀속을 다시 확인한다. Pod의 node·generation/version을 포함한 group은 최초 경로 처리에서 한 번 계산하고 snapshot이 바뀐 경우만 다시 계산한다.
- 재귀속과 슬롯 판정은 `TestReachability`처럼 generation·version을 함께 비교한다. `Stale`이면 판정을 미루고 다음 프레임에 다시 조회한다.

### 3.9 고정 검증 입력

- graph view: 7a `ProjectToNode`·`GetNodeSupport`와 조밀 view의 node 지면점·이웃·component가 회귀 8-slot과 Meadow 전체 node에서 일치
- A* 정답성(회귀 8-slot): 나무·바위 우회, 동굴 입구 portal 경유 공동 도달, 12 seam과 꼭짓점 경유, 끊긴 섬은 탐색 없이 `Unreachable`, 막힌 이음매 node 비경유. 작은 fixture에서는 Dijkstra 전수 결과와 경로 cost가 같아야 한다(최적성).
- 결정론: 같은 요청을 예산 1프레임과 여러 프레임으로 나눠 실행해도 결과 node 열이 같음
- lifecycle: 실행 중 generation·version·overlay revision 변경 시 `Stale`, owner 소멸·재요청 시 `Cancelled`, 요청당 상한 초과 시 `NoPath`
- overlay: Pod 추가 후 경로가 Pod node를 피함, 제거 후 원래 경로로 복귀, 이음매 위 Pod가 양쪽 사본을 막음
- 스냅 정책: 같은 group이면 가장 가까운 node 유지, 고립 node에 먼저 스냅된 목표가 반경 안 요청자 group으로 옮겨 감(2P 호스트 폰 사례 재현 위치), 고립 틈에 떨어진 시작도 같은 규칙으로 옮겨 감, 다른 섬 목표 `Unreachable`과 내부 node 접근점(지각→섬 밑, 섬→섬 가장자리 안쪽)
- Meadow 벤치마크: 주 component 안 결정론적 무작위 쌍(직선 20~80m)의 확장 수·시간 P50/P95, 확장당 비용

## 4. 구현 단위

### 구현 단위 0 — 조밀 graph view와 A* 핵심

- [x] load 때 asset 단위 조밀 graph view(지면점·6방향 이웃·edge cost·component)와 runtime 전역 index·seam/portal CSR을 만들어 Nav snapshot에 게시
- [x] 조밀 view 기반 빠른 투영과 §3.3 스냅 정책(가장 가까운 node 우선, group이 어긋나면 반경 안 공통 group 짝 재선택)
- [x] 순수 함수 A*(재개 가능, 결정론적 순서, group 선판정)와 Nav 직선 보행 검사·waypoint 단순화
- [x] graph view 일치, A* 정답성·최적성, 스냅 정책 자동화
- [x] Meadow 벤치마크 자동화: 확장 수·시간 분포를 로그로 남김(판정은 기록 전용)

완료 조건: UObject 없이 8-slot snapshot에서 경로를 계산하고, 회귀 oracle과 Dijkstra 전수 비교를 통과하며, 확장당 비용과 요청당 확장 분포가 기록된다.

결과(2026-10-01):

- `LNPNavGraph`(조밀 graph·투영·D-062/D-063 스냅·직선 보행 검사)와 `LNPNavPathfinding`(재개 가능 A*·waypoint 단순화)을 추가했다. `LNPNavRuntime::BuildSnapshot`이 7a 조립 끝에서 asset graph를 asset당 한 번 만들고 전역 graph를 `FLNPNavSnapshot::Graph`로 게시한다. 실패하면 7a와 같이 전체 게시가 실패한다.
- 전역 graph는 seam link와 portal을 추가 인접(CSR 대신 From 정렬 배열 + node bit)으로, D-060 막힌 node·edge를 bit와 정렬 key로 둔다. 막힌 이음매 edge는 열린 사본 한 방향만 기록돼 있어 역방향도 함께 막는다.
- 직선 보행 검사: 옥탄트 면으로의 중심 투영이 대원을 직선으로 보내므로 두 node 사이 대원호는 격자 `(i, j)` 공간의 선분이다. 선분을 격자 간격 1/4로 샘플링해 삼각 격자 metric(`di² + dj² + di·dj`)의 최근접 격자점을 따라가며, 연속 cell이 열린 grid edge로 이어졌는지 본다.
- 접근점(D-063)은 목표 방향과 각거리로 겹칠 수 있는 slot만, 그중 시작 group component를 가진 Layer만 훑는다.
- 자동화 3개 추가(Nav 10개 중 신규 3개):
  - `Nav.GraphView`: 회귀·production 전 node의 node ref 왕복, 7a `GetNodeSupport` 지면점 일치(최대 0.0016cm), 7a component·group 일치, grid edge 대칭. Spawn 후보 위치의 창 조회 최근접이 7a `ProjectToNode`와 같다. 다른 node를 고른 32건(회귀 31, production 1)은 모두 등거리 동률(최대 거리 차 0.0006cm)이다. 7a는 (J, I) 순, 조밀 view는 index 순으로 동률을 깬다
  - `Nav.RegressionPath`: 나무 우회(직선 막힘·줄기에서 떨어짐), 트인 지각 직선 통과, 지각→통로→공동 portal 2회, 12 seam 모두 두 slot 경유, 섬 group 선판정(확장 0), 지각→섬 접근점(섬 아래 지각 내부 node)과 섬→지각 접근점(섬 내부 node), Dijkstra 전수 비교 cost 일치, 예산 7씩 분할 결정론, version 변경 시 Invalid, 확장 상한 NoPath, stale handle
  - `Nav.ProductionPath`: 주 group 옆 고립 node 위 목표 40건이 모두 주 group으로 재선택(D-062), Meadow 벤치마크
- Meadow 벤치마크(주 group 지각, 직선 20~80m 결정론적 무작위 300쌍, 에디터 Development 단일 스레드):

| 지표 | P50 | P95 | 최대 |
|:---|---:|---:|---:|
| 확장 수 | 308 | 1,439 | 2,308 |
| 요청 시간 | 118us | 512us | 825us |
| waypoint 수 | 5 | 9 | — |
| 스냅(ResolveEndpoints) | 5.9us | 8.2us | — |

  확장당 0.379us, 300쌍 모두 Found(상한 30,000 도달 0), 직선 보행 가능 37쌍(12%). graph resident 4.10MiB, scratch 1개 6.21MiB. production snapshot 전체 build는 에디터에서 30.1ms다.
- 구현 단위 1 예산 입력: 확장당 0.38us이므로 계획 초기값 `NavExpansionsPerFrame` 8,000은 CPU 약 3ms다. 경로 CPU P95 1.5ms 목표에는 프레임 약 4,000 확장이 맞고, P50 요청(308 확장) 기준 프레임당 약 13요청이다. 요청당 상한 30,000은 이번 분포 최대의 13배라 여유가 있다.

### 구현 단위 1 — request scheduler와 cache

- [x] `ULNPNavPathSubsystem`: 대기열, 우선순위, scratch pool, 프레임 예산, 다중 프레임 재개, 경로 pool
- [x] generation·version·overlay revision 검증과 `Stale`·`Cancelled`·`NoPath` 처리
- [x] 경로 cache와 hit 조건(§3.5)
- [x] 예산 분할 결정론, lifecycle 자동화
- [x] 구현 단위 0 측정으로 §3.4 예산 초기값 확정

완료 조건: 여러 요청이 예산 안에서 병렬·다중 프레임으로 끝나고 한 번에 실행한 결과와 같다.

결과(2026-10-01):

- `LNPNavPathScheduler`(순수 코어), `ULNPNavPathSubsystem`(서버 tick 창구·CVar 5개), `LNPNavOverlay.h`(revision view)를 추가했다. `LNPNavGraph`에 `GetTileKey`와 `CollectDirectWalkNodes`를 더하고, 직선 보행 검사 본체를 visitor 템플릿 하나로 묶어 두 함수가 같은 판정을 쓰게 했다.
- 구조 결정(§3.4·§3.5에 반영): owner별 최신 요청 하나, 경로 handle은 공유 포인터, 스냅 비용의 예산 환산, 프레임 안 반복 라운드, 도달 불가 요청의 접근점 경로, cache hit의 목표 쪽 직선 검사, Tile fingerprint는 waypoint 구간 cell 기준
- 자동화 2개 추가:
  - `Nav.PathScheduler`(회귀 8-slot): 나무·동굴·트인 지각 세 요청을 한 tick 전량(예산 100만·scratch 4), 병렬 분할(예산 40·scratch 2, 45 tick), 직렬 분할(예산 7·scratch 1, 250 tick)로 처리해 모두 직접 A*+단순화와 같은 waypoint. 도달 불가 섬은 접근점 반경 없이 확장 0·경로 없음, 반경 3,000cm면 `Unreachable`에 접근점 끝 경로(미캐시). 실행 중 `ConnectivityGraphVersion`·overlay revision 변경 → `Stale`과 scratch 반납, 옛 handle → `Stale`. 재요청 → 옛 serial `Cancelled`, `Cancel` → 기록 삭제, owner 소멸 → 정리·`Cancelled` 계수, 상한 2 → `NoPath`. scratch 1개에서 나중 추격 요청이 먼저 온 배회보다 먼저 시작. 같은 Tile 쌍 두 번째 요청은 같은 경로를 확장 0으로 공유하고, 지나는 Tile revision이 바뀌면 재계산, 무관한 Tile revision은 hit 유지
  - `Nav.ProductionScheduler`(Meadow): 주 group 20~80m 결정론적 무작위 300요청을 한 프레임에 넣고 기본 예산(4,000·scratch 4·cache 끔)으로 처리. 병렬·직렬 모두 300/300 성공하고 직접 탐색과 waypoint가 같다
- Meadow scheduler 측정(에디터 Development, 300요청 동시 투입):

| 모드 | tick 수 | tick P50 | tick P95 | tick 최대 | 요청 대기 tick P95 |
|:---|---:|---:|---:|---:|---:|
| 병렬 | 36 | 1.09ms | 1.38ms | 3.47ms | 1 |
| 직렬 | 36 | 1.72ms | 2.00ms | 3.49ms | 1 |

  프레임당 약 8.3요청을 끝낸다. 직렬 tick은 예산 확장 비용(4,000 × 0.38us ≈ 1.5ms)에 스냅·단순화·fingerprint가 약 15% 더해진 값이다. 최대값은 첫 tick의 scratch 4개(24.8MiB) 할당이다. 병렬은 게임 스레드 기준으로 1.6배 빠르며, 남은 요청 몇 개가 라운드 끝을 끄는 탓에 4배에 못 미친다.
- 예산 초기값은 4,000으로 확정한다. 구현 단위 5의 "경로 CPU P95 1.5ms" Gate는 게임 스레드 tick 시간(병렬 대기 포함)으로 재고, worker 합산 CPU는 참고값으로 함께 기록한다(사용자 확정 2026-10-01, 프레임에 실제로 드러나는 비용이 게임 스레드 대기이기 때문이다). 병렬 모드 tick P95 1.38ms가 현재 이 기준 안이다.

### 구현 단위 2 — Pod runtime blocker overlay

- [x] overlay 객체(막힘 bitset·Tile revision·전역 revision) 게시와 교체
- [x] Mass spawn Pod 배치 후 일괄 추가, Pod Popped 시 제거, 이음매 사본 동시 막힘
- [x] 경로의 Tile revision 대조와 재계획 요청
- [x] overlay 자동화와 `DrawNav`에 overlay 막힘 표시 추가

완료 조건: Meadow production Pod 120개가 overlay에 반영되고, 경로가 Pod를 미리 돌아가며, Pod 소멸 뒤 재계획이 일어난다.

결과(2026-10-01): 실제 스폰 120 Pod에서 overlay revision 1·차단 node 623개를 게시했다. 자동화는 Pod 우회 A*, 직접 보행 차단·복귀, Popped 후 같은 serial 재계획, 이음매 양쪽 사본 차단을 검증했다. 적이 재계획 경로를 조향에 쓰는 검증은 소비자 연결 단위 3에서 한다.

### 구현 단위 3 — 경로 추종과 Enemy 연결

- [x] `FLNPEnemyPathFragment`와 경로 요청·추종 프로세서(서버, Behavior 그룹, TargetFollow 뒤·Movement 앞)
- [x] 이동 프로세서의 조향점 사용(PureEntity·ActorPromoted 공통)
- [x] 추격 직선 경로 우선과 재요청 조건, 배회 후보의 group 필터와 경로 요청
- [x] 플레이어 Surface handle·node·group fragment
- [x] `DrawNav`·`NavReport`에 개체 경로, 요청 상태 분포, 예산 사용량 추가
- [x] PIE 수동 플레이: NPC가 배경 프랍을 우회하고 동굴 안쪽까지 추격(사용자 확인, 2026-10-01)
- [x] 회귀 fixture의 이음매 경로 12개가 시작·목표 슬롯을 모두 지나는지 자동화 확인(`Nav.RegressionPath`); 실제 경로 추종은 PIE 수동 플레이·`-game` 계측으로 확인

완료 조건: 에디터 `-game`에서 적이 나무·바위를 돌아 추격하고 동굴 입구를 거쳐 공동까지 따라간다. ensure·crash가 없다.

결과(2026-10-01): 사용자 PIE 수동 플레이에서 NPC의 배경 프랍 우회와 동굴 안쪽 추격을 확인했다. 동굴 경로는 같은 슬롯의 지각→통로→공동 portal 두 개를 지나며, 슬롯 경계 이음매와는 별개다. 이음매 12개는 `Nav.RegressionPath`에서 각 경로가 양쪽 슬롯을 실제로 지나는지 검증했다. `-game` 100마리 스모크에서 경로 추종 109,883프레임·waypoint 전진 1,099회, ensure·assert·crash 0을 기록했고 SurfaceNavigation 78/78 및 전체 빌드가 통과했다. 이음매 추격의 별도 수동 플레이는 수행하지 않았다.

### 구현 단위 4 — 슬롯 도달성과 Pod 재귀속

- [x] 근접 슬롯 group 조건, 도달 불가 grace 뒤 반납
- [x] 착지 이벤트의 group 비교, 재귀속 후보 선택, `Orphaned` 상태와 제한 배회
- [x] 끊긴 섬 위 플레이어에게 근접 적이 슬롯 없이 접근점까지 다가와 서고, 섬 위 적이 지각 플레이어 쪽 가장자리 안쪽에 서는 시나리오 검증(D-063)
- [x] 넉백으로 다른 group에 떨어진 적의 재귀속 자동화 또는 `-game` 재현

완료 조건: 섬 위 플레이어에게 근접 슬롯이 배정되지 않고 근접 적이 접근점까지 다가오며, 다른 group에 떨어진 적이 같은 group의 Pod로 재귀속하거나 `Orphaned`가 된다.

결과(2026-10-01): 전체 자동화 79/79와 전체 빌드를 통과했다. `Nav.EnemyReachability`가 실제 Mass 근접·승격 슬롯의 거부/유예/반납, 점프 보존, 양방향 접근점 경로, 착지 Pod 유지/재귀속/Orphaned를 검증한다. PIE에서 발견한 재귀속 신호 subsystem 접근 선언 오류를 수정하고 로딩·추격/공격 정상 실행을 재확인했다. 사용자 수동 플레이에서 섬 위로 피하면 지각 적이 섬 바로 아래에, 지각으로 피하면 섬 적이 가장자리에 모이는 동작을 확인했다. 기존 유지 25m·인내 8초로는 도착 전 포기하므로 검증 중에만 유지 80m·세력권 100m·인내 60초를 적용했으며, 검증 후 MCP 재조회로 원래 값 복구를 확인했다. 기존 Alert 포기 규칙과 맵은 유지한다. 상세 증거와 크래시 수정은 `../history/Phase07_Log.md`에 있다.

### 구현 단위 5 — 부하 측정과 Gate

- [x] 부하 harness에 추격 요청 합성 모드 추가(가상 목표 여러 개로 다대소 수요 재현)
- [x] Development package 리슨 2P `-nullrhi -corelimit=4`, Phase 6과 같은 조건 700마리: 경로 CPU P50/P95, 확장 수, 동시 요청, 결과 상태 분포, cache hit율
- [x] 요청 로그 캡처를 Phase 10 benchmark 입력으로 저장
- [x] 전체 자동화, 에디터 전체 빌드, BuildCookRun, 패키지 1P와 리슨 2P 스모크
- [x] 700마리 프레임 P95 16.67ms·경로 tick P95 1.5ms Gate 통과

완료 조건(2026-10-01 사용자 확정): 700마리 추격 조건에서 프레임 P95가 16.67ms 안이고, 경로 CPU P95가 1.5ms 안이다. 경로 CPU는 `ULNPNavPathSubsystem` 게임 스레드 tick 시간(병렬 확장 대기 포함)이다. worker 합산 CPU는 참고값으로만 기록한다. Phase 6 cache-first 한계 800마리 대비 한계 감소를 기록한다.

측정 규약:

- `-LNPLoadBaselineChase`는 기존 지상 적의 조향·속도 의도를 네 가상 목표로 바꾼다. 타겟팅 상태·슬롯·실제 공격·합성 넉백·투사체 부하는 유지한다. 링 중심에서 접평면 네 방향 2,500cm 지점을 준비 단계에 slot 4 지각의 Nav node로 투영하며, 캐시가 빈 방향은 링 중심 반지름으로 시작해 1,500cm 창 안에서 투영한다. 네 목표를 만들지 못하면 측정을 중단한다. 5초마다 목표 배정을 반대편으로 바꾼다.
- `RunNavChaseMatrix.ps1`은 같은 패키지·seed 1·투사체 500발·리슨 2P·`-nullrhi -corelimit=4`로 자연 추격 700, 합성 추격 700·800을 순차 실행한다. 특정 시나리오 이름을 인자로 주면 그 실행만 한다.
- 경로 tick을 하지 않은 프레임의 경로 시간·확장은 0이다. `searchSumMs`는 확장 작업별 벽시계 경과 시간의 합(게임 스레드에서 실행한 확장도 포함)이며, OS 스레드 CPU 시간은 아니다. Gate에는 게임 스레드 tick 시간을 쓴다. running·queued 표본은 tick 뒤에 남은 요청 수다.
- capture 시작 시 누적 요청·종료 상태·cache 통계를 기준값으로 잡고 30초 동안의 차분을 보고한다. warm-up에서 시작해 capture 중 끝난 요청도 종료 분포에 들어가므로 제출 수와 종료 수가 꼭 같지는 않다.
- `NavRequests_N<count>_Chase<0|1>_Seed<seed>.csv`는 capture 동안 메모리에 모아 보고 시점에 저장한다. 시각·owner/serial·우선순위·시작/목표 좌표와 Surface handle·snap/접근점 반경·graph/overlay version을 담는다. 매트릭스 스크립트는 패키지의 CSV를 `Saved/Profiling/Phase07b/<scenario>_Requests.csv`로 복사한다. 재생 입력의 지형·베이크 버전과 초기 Pod overlay는 같은 실행의 host 로그·seed 및 당시 production 에셋을 따른다.

결과(2026-10-01): 계측과 기능 검증은 끝났지만 단위 5의 성능 완료 조건은 충족하지 못했다. 최종 합성 추격 700마리의 프레임 P95 22.89ms·경로 tick P95 4.781ms가 모두 Gate를 넘는다. 500마리는 13.12ms·1.047ms로 통과, 600마리는 프레임 16.18ms 통과·경로 1.501ms로 경계 실패다. 800마리도 실패했다. 전체 자동화 79/79, 최종 에디터·게임 빌드와 BuildCookRun, 패키지 1P 및 리슨 2P 종료 검증은 통과했다. 요청 CSV 5개와 상태·cache·확장 계측을 저장했다. 종료 때 파괴된 플레이어 엔티티 조회 크래시를 수정했고 최종 2P 5회에서 assert·ensure·crash 0이다. 측정 표·로그·다음 성능 과제는 `../history/Phase07_Log.md`의 단위 5 기록을 따른다. Phase 7b는 완료하지 않는다.

## 5. 전체 완료 조건

2026-10-02 변경본 패키지 반복 측정·Phase 7b 종료: 목표 투영 공유 최종 소스로 Development BuildCookRun을 통과했다(104.69초). 같은 패키지·seed 1·투사체 500발·trace 없는 리슨 2P 700마리에서 자연 추격 두 번의 프레임/경로 tick P95는 13.19/0.490ms, 14.07/0.557ms, 합성 추격은 12.74/0.440ms, 12.79/0.421ms다. 네 실행 모두 두 Gate를 통과했다. 여덟 프로세스 exit 0·probe 32개 PASS·CSV 무결성을 확인했고 같은 패키지의 최종 1P 100마리도 exit 0·probe 4개 PASS·4.52/0.007ms로 통과했다. 소스·기본값·Gate·에셋을 유지했으며 직전 전체 자동화 80/80과 이전 기능 회귀 증거를 합쳐 구현 단위 5와 Phase 7b를 완료한다. 현재 패키지 800마리는 측정하지 않아 최대 수용량은 확정하지 않는다. 측정 전체 표·요청 지연·한계는 `../history/Phase07_Log.md`의 변경본 패키지 반복 측정 기록을 따른다. Phase 7c는 사용자 진행 확인 전까지 미착수다.

2026-10-02 Enemy 내부 계측·목표 투영 공유 단위: 시작/목표 투영·타겟 도달성·직선 보행·결과 소비·제출·waypoint 추종을 trace로 분해했다. 합성 추격의 공유 목표 반복 투영을 확인해 한 processor 실행의 고정 snapshot·overlay 안에서 동일 좌표·SurfaceHandle의 결과만 공유한다. 최적화 두 번에서 목표 투영 재사용률 99.40%, Enemy 소비 P95 1.689/1.650ms를 기록했다. 최종 전체 빌드·전체 자동화 80/80·`-game` 리슨 2P 네 실행 정상 종료·probe 32개 PASS·CSV 검증을 통과했고 테스트 에셋을 복구했다. 에디터·trace 결과이므로 패키지 Gate를 대체하지 않는다. 이번 단위는 완료, Phase 7b는 미완료이며 변경본 패키지의 자연/합성 추격 반복 측정을 다음 후보로 남긴다. 상세는 `../history/Phase07_Log.md`의 Enemy 내부 계측 기록을 따른다.

2026-10-02 프레임 CPU·동시 수요 프로파일 단위: 기존 Development package를 CPU trace로 실행했다. 기본 500발에서 게임 스레드 작업 대기 평균 8.02ms, Enemy 경로 소비 평균/P95 1.78/2.12ms, scheduler 0.217/1.043ms였다. worker 투사체 판정 포함 시간은 평균/P95 5.74/6.48ms다. 투사체 0 비교 실행의 프레임 P95 12.24ms는 기본 500발의 18.15ms보다 낮지만 trace 부담·수요 차이·부하 조건 변경 때문에 정규 Gate 판정이나 순수 절감량으로 쓰지 않는다. 목표 전환의 가장 큰 여섯 제출 묶음은 큐 대기 P95 190.6ms, 나머지는 27.5ms였다. probe 16개 PASS·네 프로세스 정상 종료·요청 키/시각 검증을 통과했다. CPU 범위 계층과 프레임 비용 합계를 검증했고 분석기 메모리 채널 경고와 캡처 경계 미완료를 구분해 기록했다. 소스·기본값·Gate는 유지했으며 추가 빌드·자동화는 실행하지 않았다. 단위는 완료, Phase 7b는 미완료다. 상세와 후속 후보는 `../history/Phase07_Log.md`의 프레임 CPU 프로파일 기록을 따른다.

2026-10-02 패키지 반복 측정·요청 지연 검증 단위: 캡처 전용 제출·시작·종료 시각과 취소·미완료 구분을 추가하고 전체 자동화 80/80·전체 빌드·Development BuildCookRun을 통과했다. 수정본 700마리 리슨 2P 두 번은 프레임 P95 16.91/17.93ms로 실패, 경로 tick P95 1.129/0.903ms로 통과다. 요청 전체 지연 P95는 156.8/159.4ms이며 대부분 큐 대기다. 종료 미완료 0, 취소 30/46건, 네 프로세스 정상 종료·probe 16개 PASS를 확인했다. 단위는 완료, Phase 7b는 미완료다. 기본값·Gate를 유지하고 시간 예산을 추가하지 않았다. 상세 근거·후속 프로파일 후보는 `../history/Phase07_Log.md`의 패키지 반복 측정 기록을 따른다.

2026-10-02 scheduler 예산 차감 초과 수정 단위: 시작 비용이 모자라면 큐를 보존하고 병렬 확장 배정 합계를 잔여 예산 안으로 제한했다. 수정 전 초과 재현 뒤 전체 빌드·전체 자동화 80/80·동일 CSV 두 번을 통과했다. 직렬·병렬의 낮은 예산과 큐 보존을 검사했고, CSV 다섯 설정 모두 최대 차감이 예산 이하이며 캐시 없는 네 설정은 상태·waypoint·확장·cost가 일치했다. 기본값·시간 예산·Gate는 유지했고 패키지 재측정은 하지 않았다. 단위는 완료, Phase 7b는 미완료다. 상세는 `../history/Phase07_Log.md`의 예산 차감 초과 수정 기록을 따른다.

2026-10-02 동일 CSV 비용 비교·scheduler 예산 검토 단위: `Nav.RequestCostReplay`에서 700마리 CSV 8,610/9,298건을 예산 4,000·2,000, 시작 비용 16·64, 병렬·직렬, cache 256으로 통제 비교했다. 캐시 없는 네 설정의 상태·waypoint·확장·cost가 일치했다. 예산 감소는 tick P95를 줄이지만 처리 tick과 큐 지연을 늘린다. 시작 차감·작은 남은 예산의 균등 분배에서 예산 초과가 있고 종료·병렬 대기에는 시간 상한이 없다. 이번 단위는 비교 도구와 검토까지이며 runtime 기본값은 유지했다. Pod overlay와 실제 owner 수명·프레임 경계를 복원하지 않아 플레이 Gate를 대체하지 않는다. 전체 결과와 다음 후보는 `../history/Phase07_Log.md` 동일 CSV 비용 비교 기록을 따른다.

2026-10-01 검색 창 정확성 수정 단위: 꼭짓점 부근 반경 누락을 로컬 좌표 상자의 격자 투영 범위로 수정했다. 창 제한 없는 전체 node 거리 oracle 720입력·전체 자동화 79/79·전체 빌드·Development BuildCookRun이 통과했다. 패키지 700마리 합성 추격 반복 두 번의 프레임 P95는 18.60/17.19ms(실패), 경로 tick P95는 1.042/1.287ms(통과)다. 2P 정상 종료·probe·CSV 일치를 확인했다. 검색 정확성 단위는 완료이며 Phase 7b 성능 Gate는 미충족이다. 상세는 `../history/Phase07_Log.md` 검색 창 정확성 수정 기록을 따른다.

2026-10-01 성능 개선 단위 1: 검색 창의 빈 Tile 조회를 줄이고 게임 스레드 단계별 시간을 추가했다. 기존 창·거리·후보 선택·방문 순서·확장 예산은 유지했다. 전체 자동화 79/79, 창 조회와 독립 전 node 필터의 비교 720입력, 전체 빌드·BuildCookRun을 통과했다. 합성 700은 첫 실행 14.97ms·0.640ms로 통과했지만 반복 19.61ms·1.891ms로 실패했으므로 성능 완료 조건은 미충족이다. 자연 700은 13.63ms·0.546ms 통과, 합성 800은 21.17ms·1.794ms 실패다. 기존 검색 창의 꼭짓점 부근 3,000cm 반경 누락도 별도 정확성 과제로 남았다. 상세 측정·로그·후속 과제는 `../history/Phase07_Log.md` 성능 개선 단위 1을 따른다.

- A*가 회귀 fixture에서 나무·바위·절벽을 우회하고 동굴·이음매를 경유하며 최적 cost를 낸다.
- 끊긴 섬 목표는 탐색 없이 도달 불가로 끝나고 적은 슬롯 없이 접근점까지 다가오며, 고립 node에 먼저 스냅된 시작·목표는 반경 안 공통 group으로 옮겨 간다.
- 다중 프레임 요청이 generation·version·overlay revision 변경을 감지하고 서로 다른 snapshot 결과를 섞지 않는다.
- Pod가 경로 계획에서 미리 회피되고 Pod 소멸 뒤 재계획된다.
- PureEntity와 ActorPromoted가 같은 경로를 따라 추격·배회한다.
- 근접 슬롯이 도달성을 따르고, 다른 group에 떨어진 적이 재귀속하거나 `Orphaned`가 된다.
- 경로 CPU·확장 수·cache hit율 기록과 Phase 10 요청 캡처가 있다.
- 전체 자동화, 에디터 전체 빌드, Development package, `-game` 리슨 서버 2P 스모크가 통과한다.

## 6. 결정 기록

- 목표 스냅 정책(§3.3): 2026-10-01 D-062로 확정했다. 처음 권장안은 node 16개 미만 component 제외였으나, 가장 가까운 node를 우선하고 group이 어긋날 때만 주변 공통 group 짝을 고르는 사용자 제안으로 바꿨다. 기준값이 필요 없고 실제로 끊긴 작은 자리도 존중한다.
- Pod Nav 차단 위치(§3.6): 2026-10-01 D-061로 확정했다.
- 도달 불가 대상 접근(§3.3·§3.8): 2026-10-01 D-063으로 확정했다. 처음 계획은 제자리 Alert였다. 상태·슬롯 규칙을 그대로 두고 이동 목표만 더하는 작은 변경이라 사용자 요청대로 접근하게 했다.
