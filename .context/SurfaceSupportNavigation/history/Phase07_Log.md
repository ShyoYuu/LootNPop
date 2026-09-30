# Phase 7 작업 로그

> 상태: 진행 중
> 실행 계획: `../phases/Phase07a_NavDataFoundation.md`

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
