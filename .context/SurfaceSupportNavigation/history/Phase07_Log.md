# Phase 7 작업 로그

> 상태: 진행 중
> 실행 계획: `../phases/Phase07a_NavDataFoundation.md`

## 2026-10-01 — Phase 7b 구현 단위 3 완료

- `FLNPEnemyPathFragment`와 서버 Behavior 단계의 `ULNPEnemyPathProcessor`를 TargetFollow 뒤·Movement 앞에 연결했다. `FMassMoveTargetFragment`는 의미상 목표를 유지하고, PureEntity·ActorPromoted 이동이 공통 waypoint 조향점을 소비한다. 공중·비행 개체는 경로를 따르지 않는다.
- `ULNPPlayerNavProcessor`가 플레이어 Mover floor hit을 hit identity registry로 해석해 Surface handle·node·group fragment에 기록한다. 공중일 때는 마지막 접지 정보를 보존한다.
- 추격은 Nav 직선 보행을 먼저 검사하고, 막히면 scheduler에 요청한다. 목표 Tile·이동량·경로 revision 변경과 Pod overlay 변경 뒤 재요청한다. 배회는 Pod ReachabilityGroup 밖 후보를 버리고 직선 보행 후보를 우선한다.
- `NavReport`에 요청 상태·예산·cache·추종 프레임·waypoint 진행 수를, `DrawNav`에 개체 경로 선을 추가했다.
- 전체 빌드와 SurfaceNavigation 자동화 78/78이 통과했다(`Saved/Logs/Phase07b_Unit3_AllSurfaceNavTests.log`). 에디터 `-game` 1P 100마리 스모크는 프레임·exact·lock 기준을 통과했고, 추종 프레임 109,883회·waypoint 진행 1,099회를 기록했다(`Phase07b_Unit3_EditorGame_WanderFinal.log`). ensure·assert·crash는 없었다. 기존 Mass의 CharacterMovementComponent 추출 오류는 단위 2 스모크에도 있었다.
- Unreal MCP 에디터 자동화에서 `LootNPop.SurfaceNavigation.Nav.RegressionPath` 1/1을 다시 통과했다(오류·경고 0). 이 테스트는 프랍 우회, 지각→동굴 통로→공동 portal, 12개 seam 경유를 검증한다. 라이브 PIE의 동굴 공동에 플레이어를 스폰해 group 0 투영과 portal 1개를 `DrawNav`로 확인했고, 별도 PIE에서 `NavReport`가 성공 경로 222개·경로 추종 1,977,984프레임·waypoint 전진 34,613회를 기록했다. 다만 공동 PIE에서는 적이 시야 범위 바깥에 있어 실제 동굴 진입 추격이 발생하지 않았다.
- 사용자 PIE 수동 플레이에서 NPC의 배경 프랍 우회와 동굴 안쪽 추격을 확인했다. 이 경로는 같은 슬롯의 Layer portal 두 개를 지나며 슬롯 이음매를 지나지는 않는다. 이음매 12개는 `Nav.RegressionPath`가 양쪽 슬롯을 실제로 지나는 경로를 검증했고, 경로 추종은 PIE와 `-game` 계측으로 확인했다. 별도 수동 이음매 추격은 요구하지 않고 단위 3을 완료했다.

## 2026-10-01 — Phase 7b 구현 단위 2

- `FLNPNavOverlay`에 조밀 node 차단 bitset을 추가했다. Pod footprint는 collision proxy의 공통 반지름 128cm와 Nav agent 반지름을 합산한다. 같은 slot·Support Layer node만 차단하고 seam link의 양쪽 사본을 함께 막는다.
- Mass spawn은 Pod ID·배치 지면·Surface handle을 모아 모든 스폰이 끝난 뒤 한 번 게시한다. Popped 후 서버의 지연 상태 전환 커맨드가 해당 Pod를 제거하며, 바뀐 Tile revision과 전역 overlay revision만 올린 새 객체를 게시한다.
- A* 확장·시작/목표 스냅·Nav 직선 보행·경로 단순화·cache 검증에 overlay 차단을 적용했다. 기존 완료 경로는 통과 Tile의 revision이 달라지면 같은 serial로 재계획 대기열에 넣는다. `DrawNav`는 Pod 차단 node를 주황색으로 표시한다.
- `LootNPopEditor Win64 Development` 전체 빌드 통과. 신규 `Nav.PodOverlay` 자동화는 Pod 우회, 제거 뒤 직접 경로 복귀, 같은 serial 재계획, seam 양쪽 차단을 통과했다(`Saved/Logs/Phase07b_Unit2_PodOverlayTest_Final.log`). 전체 SurfaceNavigation은 78/78 통과(`Phase07b_Unit2_AllSurfaceNavTests.log`). 자동화가 재저장한 fixture 에셋 3개는 작업 시작 상태로 되돌렸다.
- 에디터 `-game` 1P 스모크에서 production Pod 120개를 overlay revision 1·차단 node 623개로 게시하고 정상 종료했다(`Saved/Logs/Phase07b_Unit2_EditorGame.log`). 부하 harness의 프레임 P95 16.67ms는 에디터 1P 수치이며 단위 5 패키지 Gate로 사용하지 않는다. 이번 단위에는 Enemy 경로 소비자가 없으므로 실제 조향·Popped 뒤 이동 전환은 단위 3에서 검증한다.

## 2026-09-29 — Phase 7a 실행 계약

- Phase 7을 7a Nav 데이터 기반과 7b 경로 실행으로 분리한 Roadmap 내부 게이트에 따라 7a를 시작했다.
- `Phase07a_NavDataFoundation.md`를 작성했다.
- Support Layer별 coarse triangular Grid, 지각 200cm·비지각 100cm 목표 간격, 16×16 Tile, 6방향 이웃을 고정했다.
- Navigation/Traversal codec v1, local StaticNavComponent, Layer 간 정적 portal, ordered seam endpoint와 8-slot 병합 규약을 고정했다.
- ReachabilityGroup은 active link 집합에서 재구축하고 `SnapshotGeneration`과 `ConnectivityGraphVersion`을 함께 검증하도록 했다.
- codec 합성 입력과 정적 회귀 LVI의 지각·섬·동굴·프랍·seam·Spawn projection oracle을 7a 고정 검증 입력으로 정했다.
- 아직 소스 코드와 에셋은 변경하지 않았다. 다음은 구현 단위 0의 순수 자료구조·codec·자동화다.

## 2026-09-29 — Phase 7a 구현 단위 0

- runtime 순수 `LNPNavData` 타입과 16×16 Tile 주소, 삼각 격자 6방향 neighbor/opposite helper를 구현했다.
- runtime node ref는 Surface snapshot의 `uint64` generation을 그대로 보존하고, codec local ref는 `(LocalNavLayerId, TileId, LocalCellIndex)`를 쓴다.
- Navigation codec v1은 canonical Layer/Tile/Cell 순서, agent profile, walkable cell, 6방향 edge와 local component ID를 encode/decode한다.
- Traversal codec v1은 component descriptor, Layer 간 정적 portal, ordered seam endpoint를 encode/decode한다.
- validation이 범위·중복·component node 수·reciprocal edge·누락 portal endpoint·seam 좌표 불일치·truncated/trailing payload를 거부한다.
- 기존 SurfaceData를 중간 상태로 무효화하지 않도록 active `DataVersion`/`BakerSchemaVersion`은 4/3으로 유지하고 다음 전환 값 5/4를 코드에 명시했다. 구현 단위 1에서 stream 생성과 함께 활성화한다.
- `LootNPopEditor Win64 Development` 전체 빌드 성공.
- `LootNPop.SurfaceNavigation.Nav` 자동화 3/3 통과. 최종 로그는 `Saved/Logs/Phase07a_Unit0_NavTests.log`다.
- 첫 자동화 실행은 duplicate 입력 fixture가 같은 `TArray` 원소를 직접 `Add`해 UE self-add assertion으로 중단됐다. 임시 값에 복사한 뒤 추가하도록 테스트만 수정했고 재빌드·최종 실행은 통과했다.

## 2026-09-30 — Phase 7a 구현 단위 1 중단점

- `LNPNavBaking` 순수 베이커와 합성 자동화 `Nav.Baking`을 추가했다. Support Layer와 Nav Layer는 1:1이고 지각 200cm·비지각 100cm 격자, 6방향 edge, 16×16 Tile, local component, Layer portal 후보와 ordered seam endpoint를 생성한다.
- editor 베이커가 기존 exact collision preview world를 Nav와 Spawn에 공유한다. 정적 blocker는 반지름 50cm·반높이 88cm 캡슐로 dilation하고 edge는 양방향 segment sweep한다. 캡슐 축은 바닥 법선이 아니라 지역 중력 Up이며 경사 접촉 높이를 보정한다.
- Navigation/Traversal codec 버전과 모든 agent/Nav 설정을 `BakeSettingsHash`에 포함하고 `DataVersion=5`, `BakerSchemaVersion=4`를 활성화했다. header descriptor, payload 저장, report와 결정론 테스트도 두 stream을 포함한다.
- `LootNPop.SurfaceNavigation.Nav` 4/4 통과(`Saved/Logs/Phase07a_Unit1_NavTests.log`). 여러 차례의 증분을 포함해 마지막 `LootNPopEditor Win64 Development` 전체 빌드도 성공했다.
- 초기 250cm 설정의 세 asset 재베이크와 결정론 테스트는 성공했다. 당시 회귀 fixture는 69,377 cells, 7 components, portal 1, seams 1,107이었고 나무·바위 dilation 및 Decoration 무시 oracle이 통과했다.
- 회귀 portal을 조사해 그 1개가 공동 Layer 2↔통로 Layer 3임을 확인했다. 통로 Layer 3↔지각 Layer 0은 거리와 양방향 step을 통과하며 50cm 간격 exact Support도 연속이지만 capsule sweep에서만 탈락한다. 탐색 거리 800cm와 한쪽 경계 후보까지 적용한 현재 결과도 portal 1개다(`Saved/Logs/Phase07a_Unit1_PortalOneBoundary.log`).
- 현재 코드는 컴파일되지만 구현 단위 1은 미완료다. 회귀 asset만 현재 실험 설정으로 저장됐고 Crust·Meadow asset은 최신 settings hash에 stale하다. portal 문제를 해결하기 전에는 최종 asset 재베이크나 결정론 green 기준으로 간주하지 않는다.
- 다음 시작점은 Layer 0↔3 실패 sweep의 hit component와 50cm segment를 기록하는 것이다. 해결 후 임시 Display 진단을 제거하고 회귀 portal 2개 oracle, 세 asset 재베이크, Nav/결정론 자동화, 전체 빌드를 다시 통과시킨다.

## 2026-09-30 — Phase 7a 구현 단위 1 완료

- `SweepSingle` 계측으로 가장 가까운 지각 Layer 0↔통로 Layer 3 후보가 첫 segment에서 통로 바닥 `StaticMeshActor_16.StaticMeshComponent0`을 맞히는 것을 확인했다. 후보 거리·step·Support가 아니라 endpoint 사이 직선 capsule 경로가 경사 바닥 안으로 파고드는 것이 원인이었다(`Saved/Logs/Phase07a_Unit1_PortalHit.log`).
- portal의 50cm exact Support trace가 얻은 실제 hit point·normal을 capsule polyline에 재사용하도록 수정했다. 그 결과 지각 0↔통로 3과 공동 2↔통로 3이 모두 생겼다. 임시 hit/component Display 진단과 공개 report 진단 배열은 제거했다.
- portal은 최대 800cm 안에서 적어도 한쪽이 boundary node인 후보를 찾고, 양방향 step, 중간 exact Support 연속성, gravity-up capsule 양방향 sweep을 모두 통과해야 한다. 이 규칙은 빈 간격 shortcut을 막으면서 coarse grid 사이의 실제 접합을 보존한다.
- 최종 재베이크 수치:
  - Crust: 68,500 cells, 5 components, 0 portals, 1,107 seams, Navigation 553,104 B, Traversal 14,447 B
  - Regression: 69,378 cells, 7 components, 2 portals, 1,107 seams, Navigation 560,328 B, Traversal 14,503 B
  - Meadow: 67,729 cells, 5,151 components, 4 portals, 1,058 seams, Navigation 547,584 B, Traversal 55,058 B
- 세 asset의 `DataVersion=5`·`BakerSchemaVersion=4` 저장본을 최신 settings hash로 정렬했다(`Saved/Logs/Phase07a_Unit1_Rebake_Final.log`).
- 최종 `LootNPopEditor Win64 Development` 전체 빌드 성공. Nav 자동화 4/4(`Phase07a_Unit1_NavTests_Final.log`)와 결정론/저장본/회귀 oracle 1/1(`Phase07a_Unit1_Deterministic_Final.log`) 통과. 구현 단위 1 완료.

## 2026-10-01 — Phase 7a 구현 단위 2 완료

- 순수 runtime 모듈 `LNPNavRuntime`을 추가했다. 8-slot의 decoded Support·Navigation·Traversal과 slot 회전 표로 `FLNPNavSnapshot`을 만든다. runtime Nav Layer ID는 slot별 base + local ID이고, runtime component는 portal과 seam link를 union한 뒤 결정론적으로 번호를 매긴다.
- loader는 Navigation/Traversal을 필수 payload로 바꿨다. hash·codec·descriptor count를 검증하고 asset별로 한 번만 decode해 slot이 공유한다. Nav view는 같은 `FLNPSurfaceDataSnapshot`에 들어가 Support와 원자적으로 게시된다. `ValidateAndBuildSnapshot`은 모든 실패 경로에서 부분 결과를 비운다.
- 첫 production 조립이 Meadow 8-slot에서 실패했다. 이음매 한쪽 사본에만 있는 node가 392개, 비대칭 이음매 방향 edge가 16개였다. 누락 측 법선은 모두 walkable이었으므로 전부 capsule clearance 탈락이다. asset 변별로 보면 X0 17·Y0 18·Z0 14개가 1~3 step(200~600cm)씩 흩어져 있었다. 이음매 근처 정적 blocker가 한쪽 slot 베이크에만 보이는 구조적 비대칭으로 판단했다. 회귀 fixture는 막힘 0이다.
- 사용자와 D-060을 확정했다. 이음매 줄 node·이음매 방향 edge는 양쪽 사본이 모두 통과할 때만 유효하고, 차이는 `BlockedSeamNodes`·`BlockedSeamEdges`로 기록한다. 지면·해상도·profile 불일치는 계속 게시 실패다. 이음매 근처 정적 배치 금지는 콘텐츠 규칙(`design/TerrainContract.md` §7)이고, 동적 스폰 오브젝트는 이음매 위에 둘 수 있다. 베이커 report에 이음매 clearance 탈락 경고를 추가했다(Meadow 49개).
- 새 파일이 unity build 묶음을 바꾸면서 `LNPNavBaking.cpp`와 `LNPNavData.cpp`의 익명 네임스페이스 helper 이름(`NodeLess`·`PortalLess`·`CoordKey`)이 충돌했다. 베이커 쪽 helper에 `Bake` 접두사를 붙였다.
- 결과 수치:
  - 회귀 fixture 8-slot: runtime Layer 56, local component 56, runtime component 33, seam link 4,428, 막힘 0/0, seam 반지름 차 0cm, 최소 법선 dot 0.99999
  - Meadow 8-slot: runtime Layer 88, local component 41,208, runtime component 41,169, seam link 4,036, 막힘 392/16, 최소 법선 dot 0.95296
  - Nav resident 0.74MiB, 전체 decoded resident 3.68MiB, serialized 3.43MiB
  - 에디터 `-game` validateBuild 20.6ms, 게시 597ms(이전 641ms, 에디터 async load가 지배)
- 검증:
  - `LootNPopEditor Win64 Development` 전체 빌드 성공
  - 전체 `LootNPop.SurfaceNavigation` 69/69(`Saved/Logs/Phase07a_Unit2_AllSurfaceNavTests.log`). 새 `Runtime.NavAssembly`는 공유 decode, Layer base, seam link 수, node ref 왕복과 stale 거부, Nav 손상·누락 시 부분 게시 거부, 한쪽 node와 비대칭 edge의 막힘 기록, 반지름 불일치 실패, 회귀 8-slot의 지각·portal component 병합을 검사한다.
  - 에디터 `-game` 1P(`Phase07a_Unit2_EditorGame.log`)와 리슨 2P(`Phase07a_Unit2_2P_Host.log`·`_Guest.log`): 호스트·게스트가 같은 Nav 결과를 게시했다. `ProbeSurfaceData`(Nav generation 포함)와 `ProbePanels`가 PASS이고 ensure·crash는 0이다. 1P 두 번째 실행의 frame FAIL은 에디터 `-game` 측정 오염이며 성능 Gate가 아니다(첫 실행 PASS).
- Meadow의 slot당 local component 5,151개 중 seam으로 합쳐지는 것은 극소수다. 대부분 작은 고립 조각으로 보이며, 7b 입력 통계(구현 단위 4)에서 크기 분포를 확인한다.

## 2026-10-01 — Phase 7a 구현 단위 3 완료

- `FLNPNavSnapshot`에 ReachabilityGroup(초기 StaticNavComponent 1:1)·`ConnectivityGraphVersion=1`·정렬된 막힘 이음매 node key를 추가했다. 새 `LNPNavQuery`는 Surface handle의 slot·Layer 안에서만 가장 가까운 walkable node를 찾고(기본 300cm, 막힌 이음매 node 제외), node의 Support 지면점 복원, component·group 조회, generation·version을 함께 보는 `TestReachability`를 제공한다.
- 진단 명령 `LNP.SurfaceNav.NavReport`·`LNP.SurfaceNav.DrawNav`를 추가하고 부하 harness 종료 probe에 넣었다. 두 명령은 같은 ID 해시 색과 폰 node 줄을 찍어 그림과 보고서를 대조할 수 있다.
- 첫 1P 스모크에서 Meadow runtime component 41,169개 중 35,832개가 node 1개였고, 폰 주변 node 395개에 edge가 227개뿐이었으며 폰이 node 3개 component에 서 있었다. 임시 분석으로 slot 0 지각의 끊긴 인접 edge 32,090개 중 24,490개가 `CanStep`(200cm 끝점 높이 차 ≤ 45cm) 탈락이고 전부 45° walkable 경사 안임을 확인했다. 존재하는 edge의 최대 높이 차가 정확히 45.0cm였다. 약 12.7°를 넘는 경사가 모두 끊기던 단위 1 결함이다.
- step과 경사를 구분하는 규칙(`max(step, 수평 × tan 최대 경사)`)으로 끝점 사전 필터를 바꾸고, 같은 Layer edge도 portal처럼 50cm exact Support polyline(구간별 같은 규칙)과 그 polyline sweep을 쓰게 했다. 사용하지 않게 된 endpoint 직선 sweep helper는 지웠다. `BakerSchemaVersion` 4→5로 세 asset을 재베이크했다(`Saved/Logs/Phase07a_Unit3_Rebake.log`).
  - Meadow: local component 5,151→91, Traversal payload 55,058→14,578 B, 8-slot runtime component 41,169→689, slot당 고립 cell 4,479→54, edge 188,120, 끊긴 인접 지각 edge 32,090→3,044, portal 4개 유지
  - 크기 분포(8-slot runtime): 1 node 432, 2~10 200, 11~100 40, 101~1,000 8, 1,000 초과 9. 가장 큰 지각 component 510,352 node
  - 회귀·Crust fixture는 cells·components·portals·seams와 payload 크기가 변하지 않았다
- `WorldCollision.Api`의 "Some raycasts ran off the game thread" 간헐 실패가 두 번째로 재발해, `ParallelFor`를 스레드 풀 작업 안에서 호출하도록 테스트를 고쳤다(호출 스레드 몫도 워커 실행이 된다).
- 검증:
  - `LootNPopEditor Win64 Development` 전체 빌드 성공
  - 전체 `LootNPop.SurfaceNavigation` 72/72(`Saved/Logs/Phase07a_Unit3_AllSurfaceNavTests.log`). 새 테스트는 `Nav.RegressionReachability`, `Nav.ProductionSpawnProjection`, `Nav.StepAndSlope`이고 `Runtime.NavAssembly`를 보강했다
  - 회귀 oracle: group 33, anchor 24, random 후보 10,616개 투영 P50 55.3·P99 106.9·최대 115.3cm. Meadow 후보 9,990개 투영 P50 55.8·P99 124.3·최대 290.5cm
  - 에디터 `-game` 1P(`Phase07a_Unit3_EditorGame.log`)와 리슨 2P(`Phase07a_Unit3_2P_Host.log`·`_Guest.log`): 양쪽이 같은 Nav(component·group 689, version 1, seam link 4,036, 막힘 392/16, Nav resident 0.56MiB)를 게시했고 `ProbeSurfaceData`·`ProbePanels` PASS, ensure·crash 0. 1P 폰은 지각 주 component, 폰 주변 node 363개·edge 721개. 1P frame FAIL은 에디터 `-game` 측정 오염이다
- 7b 입력 메모: 가장 가까운 node 규칙은 edge 없는 고립 node(8-slot 432개)도 고른다. 2P 호스트 폰이 그런 node에 투영됐다. 게스트 폰은 Support NeedsExact 구역이라 투영하지 않았다. 7b 목표 스냅은 group 크기나 edge 유무를 볼지 정해야 한다.

## 2026-10-01 — Phase 7a 구현 단위 4 완료(Phase 7a 종료)

- 7b 입력 통계용 진단 두 가지를 추가했다. 둘 다 기록 전용이며 판정에는 쓰지 않는다.
  - `NavReport`에 같은 Layer grid edge 기준 node 차수 분포 한 줄(`gridDegree`, seam link·portal 제외)
  - `Nav.ProductionSpawnProjection` 보고에 투영 P90, 고립 node 적중 수, node 10개 이하 component 적중 수
- 검증:
  - `LootNPopEditor Win64 Development` 전체 빌드 성공, 전체 `LootNPop.SurfaceNavigation` 72/72(`Saved/Logs/Phase07a_Unit4_AllSurfaceNavTests.log`)
  - Win64 Development BuildCookRun: build·cook 972 packages·stage·pak·archive 성공, 오류 0. 기존 Lyra Mannequin Material 경고만 재현(`Phase07a_Unit4_BuildCookRun.log`, archive `Saved/SurfaceNavigationPhase3Package`)
  - 패키지 1P(`Phase07a_Unit4_Package1P.log`)와 리슨 2P(`Phase07a_Unit4_Package2P_Host.log`·`_Guest.log`), `-nullrhi -corelimit=4`, 적 100: 세 프로세스 모두 DataVersion 5·`BakerSchemaVersion=5` Meadow 8-slot을 generation 1로 게시했다. runtime Layer 88, local component 728, runtime component·group 689, version 1, seam link 4,036, 막힘 392/16, seam 반지름 차 0cm로 같다. `ProbeSurfaceData`(Nav 포함)·`ProbePanels`·`ProbeFaceIndex`·`ProbeSourceKeys`와 LoadBaseline frame/exact/lock 모두 PASS, ensure·assert 0
- 패키지 load 지표:

| 실행 | elapsed | validateBuild | serialized | decoded resident | Nav resident |
|:---|---:|---:|---:|---:|---:|
| 1P | 35.35ms | 22.29ms | 3.39MiB | 3.56MiB | 0.59MiB |
| 2P host | 33.99ms | 21.38ms | 3.39MiB | 3.56MiB | 0.59MiB |
| 2P guest | 34.90ms | 23.77ms | 3.39MiB | 3.56MiB | 0.59MiB |

  Meadow 한 asset의 Nav/Traversal payload는 약 535KiB/14KiB이고 8 slot이 decode 결과를 공유한다. 에디터 `-game`의 게시 597ms는 async load 지배였고 cooked에서는 35ms 안팎이다.
- 7b 입력 통계(production Meadow 8-slot, cooked 실행과 자동화 수치 일치):
  - node 541,832(slot당 67,729, Tile 349개, Tile당 평균 약 194 node), 같은 Layer grid edge 1,504,960, seam link 4,036, portal 32
  - grid 차수: 0:432, 1:1,416, 2:4,288, 3:13,800, 4:46,896, 5:79,056, 6:395,944. 평균 약 5.55이고 73.1%가 6방향 전부 열린 내부 node다. A* 분기 수는 사실상 6이다
  - runtime component 689: 1 node 432, 2~10 200, 11~100 40, 101~1,000 8, 1,000 초과 9. 가장 큰 지각 component가 510,352 node(94.2%), 다음 8개는 slot별 큰 섬 3,222 node다. 탐색 1회의 최악 탐색 공간은 지각 전체 약 51만 node다
  - Spawn 후보 9,990개 투영: P50 55.8·P90 82.9·P99 124.3·최대 290.5cm. 고립 node 적중 5, node 10개 이하 component 적중 25(0.25%)
- 7b 설계 질문(목표 스냅): 패키지 2P 호스트 폰도 106.5cm 거리의 node 1개짜리 component에 투영됐다(단위 3 에디터 2P와 같은 현상). 가장 가까운 node 규칙은 고립 node를 고르므로 7b는 목표 스냅이 edge 유무·group 크기를 볼지, 반경 안 대체 node를 찾을지 정해야 한다. 1P 폰과 2P 게스트 폰은 발 반지름 31,020·31,131cm에서 Support 결과가 없어(status 2) 투영하지 않았다. 종료 probe 시점의 폰 위치 문제로 보이며 Nav 게시와 무관하다.

## 2026-10-01 — Phase 7b 실행 계획 작성

- `phases/Phase07b_PathExecution.md`(초안)를 작성했다. 입력은 7a 인계 통계와 현재 추격·배회·이동 코드다.
- 코드 확인에서 나온 계획 전제:
  - `LNPNavQuery`의 node 해석은 Tile 표 선형 탐색과 Support 보간이라 A* 확장 루프에 쓸 수 없다. 구현 단위 0에서 asset 단위 조밀 graph view(지면점·이웃·edge cost·component, asset당 약 2.4MiB)를 먼저 만든다.
  - 추격은 `MoveTarget.Center`를 매 프레임 타겟 쪽으로 쓰고 이동 프로세서가 직진한다. 경로는 MoveTarget을 바꾸지 않고 별도 조향점만 준다. 그래서 StateTree Task와 TargetFollow를 고치지 않고 PureEntity·ActorPromoted가 같은 결과를 받는다.
  - 플레이어 Mass 엔티티에는 Surface handle이 없다. 적 ActorPromoted의 Mover floor → `ResolveHit` 경로를 재사용해 만든다.
  - Pod는 Popped 때 엔티티가 사라지므로, Nav 차단은 추가뿐 아니라 해제와 revision 무효화가 필요하다.
- 결정 두 건을 확정했다.
  - D-061: Pod Nav 차단을 7b 최소 overlay로 넣는다. overlay는 A*만 막고 ReachabilityGroup은 바꾸지 않는다.
  - D-062: 스냅 정책. 처음 권장안(node 16개 미만 component 제외)을 사용자 제안으로 바꿨다. 가장 가까운 node를 우선하고, group이 어긋날 때만 양쪽 300cm 반경 안에서 공통 group 짝을 다시 고른다. 재선택은 A* 재시도가 아니라 투영 창의 group 비교다. 기준값이 필요 없고 실제로 끊긴 작은 자리도 존중한다. 끊긴 섬 위 목표는 반경 안에 지각 node가 없어 여전히 도달 불가이며 근접 적은 Alert에 머문다.
  - D-063: 도달 불가 대상에게 제자리 Alert 대신 접근점까지 이동한다. 상태 머신·슬롯·Alert 인내 규칙을 그대로 두고 이동 목표만 더하는 작은 변경이라 채택했다. 접근점은 자기 group의 6방향 내부 node 중 목표에 가장 가까운 node(초기 3,000cm)이다. 플레이어가 섬 위이고 적이 지각이면 지각 쪽 최근접점은 섬 바로 아래라서 적이 섬 밑에 모인다. 이 점은 사용자에게 알렸다.

## 2026-10-01 — Phase 7b 구현 단위 0 완료

- `LNPNavGraph`·`LNPNavPathfinding`을 추가하고 `LNPNavRuntime::BuildSnapshot` 끝에서 조밀 graph를 게시한다. 상세 구조는 `phases/Phase07b_PathExecution.md` 구현 단위 0 결과.
- 첫 실행에서 `Nav.GraphView`가 창 조회 최근접 node를 7a `ProjectToNode`와 비교해 회귀 31건·production 1건 불일치를 냈다. 거리 차를 찍어 보니 모두 등거리 동률(최대 0.0006cm)이었다. 7a는 (J, I) 순, 조밀 view는 index 순으로 동률을 깨기 때문이다. 테스트를 "0.01cm 넘는 거리 차만 불일치"로 고쳤다. 동률 규칙 자체는 각자 결정론적이라 맞출 필요가 없다.
- 검증:
  - `LootNPopEditor Win64 Development` 전체 빌드 성공
  - 전체 `LootNPop.SurfaceNavigation` 75/75(`Saved/Logs/Phase07b_Unit0_AllSurfaceNavTests.log`)
  - 조밀 graph: 회귀 555,024 node·extra link 8,888, production 541,832 node·extra link 8,136·막힌 edge 32(16쌍 양방향), 7a 지면점과 최대 0.0016cm 차, resident 4.05/4.10MiB
  - Meadow 벤치마크(주 group 지각, 직선 20~80m, 300쌍, 에디터 Development 단일 스레드): 확장 P50 308·P95 1,439·최대 2,308, 시간 P50 118us·P95 512us·최대 825us, 확장당 0.379us, 전부 Found, 직선 보행 가능 37쌍, waypoint P50 5·P95 9, 스냅 P50 5.9us
  - D-062: 주 group 옆 고립 node 위 목표 40건 모두 주 group으로 재선택
- 구현 단위 1 입력: 계획 초기 예산 8,000 확장/프레임은 CPU 약 3ms라 목표(경로 CPU P95 1.5ms)의 두 배다. 약 4,000에서 시작한다.
- `-game` 스모크는 하지 않았다. 게시 경로는 자동화가 부르는 `ValidateAndBuildSnapshot`과 같지만, 실제 게시·게스트 확인은 구현 단위 5 Gate에서 한다.

## 2026-10-01 — Phase 7b 구현 단위 1 완료

- `LNPNavPathScheduler`(순수 코어)·`ULNPNavPathSubsystem`(서버 tick 창구)·`LNPNavOverlay.h`(revision view)를 추가했다. 규약은 `phases/Phase07b_PathExecution.md` §3.4·§3.5와 구현 단위 1 결과에 반영했다.
- 설계 선택:
  - 코어를 UObject 없이 두어 자동화가 회귀·production snapshot으로 직접 tick한다. subsystem은 snapshot·CVar·owner 유효성(`IsEntityValid`)만 넘긴다.
  - overlay의 revision 부분만 먼저 정의했다. 다중 프레임 Stale 판정과 cache fingerprint 대조에 실제 revision 생산자 모양이 필요했고, 막힘 내용·생산자는 구현 단위 2의 몫으로 남겼다.
  - fingerprint Tile은 A* node가 아니라 단순화된 waypoint 구간의 직선 보행 cell에서 모은다. 이를 위해 `IsDirectWalkable` 본체를 visitor 템플릿으로 묶고 `CollectDirectWalkNodes`를 더했다(판정 로직은 그대로).
  - cache hit에 목표 쪽 직선 검사(마지막 waypoint → 요청자 목표 node)를 더했다. Tile이 지각 약 32m라 시작 쪽만 보면 경로 끝이 목표에서 멀 수 있다.
  - 스냅 비용을 확장 16개로 환산해 예산에서 뺀다. cache hit·도달 불가 요청이 몰려도 프레임 비용이 예산 안이다.
- 검증:
  - `LootNPopEditor Win64 Development` 전체 빌드 성공(경고 0)
  - Nav 12/12(`Saved/Logs/Phase07b_Unit1_NavTests.log`), 전체 SurfaceNavigation 77/77(`Phase07b_Unit1_AllSurfaceNavTests.log`)
  - `Nav.PathScheduler`: 세 요청(나무·동굴·트인 지각)이 한 tick·병렬 분할(45 tick)·직렬 분할(250 tick)에서 모두 직접 탐색과 같은 waypoint, 병렬 분할 최대 동시 2. lifecycle·우선순위·cache 사례 전부 통과
  - `Nav.ProductionScheduler`(Meadow 300요청 동시 투입, 예산 4,000·scratch 4·cache 끔): 36 tick, 병렬 tick P50 1.09·P95 1.38·최대 3.47ms, 직렬 P50 1.72·P95 2.00ms, 요청 대기 tick P95 1, 확장 합계 138,062, scratch 24.82MiB. 결과 300/300이 직접 탐색과 같다
- 해석: 프레임당 약 8.3요청을 끝낸다. 직렬 tick은 예산 확장 비용(약 1.5ms)에 스냅·단순화·fingerprint가 약 15% 더해진 값이다. 병렬 이득이 1.6배에 그치는 이유는 라운드 끝의 긴 요청 몇 개다. 필요하면 구현 단위 5에서 라운드 몫 배분을 조정한다.
- 예산 4,000 확정. 구현 단위 5의 경로 CPU Gate는 게임 스레드 tick 시간(병렬 대기 포함)으로 재고 worker 합산 CPU를 참고값으로 기록한다. 사용자가 이 기준을 확정했다. worker 합산은 지나치게 빡빡하고, 프레임에 실제로 드러나는 비용은 게임 스레드 대기라는 이유다.
- `-game` 스모크는 하지 않았다. 아직 요청을 내는 소비자가 없다(구현 단위 3).
