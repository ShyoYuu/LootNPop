# Phase 7 작업 로그

> 상태: 진행 중
> 실행 계획: `../phases/Phase07a_NavDataFoundation.md`

## 2026-10-01 — Phase 7b 구현 단위 4 검증 중

- 근접 슬롯의 D-062 도달성 필터·1.5초 grace·점프 중 기존 슬롯 보존, Unreachable Alert의 자기 group 접근점 이동·정지, 정적 착지 후 활성 Pod 유지/최근접 재귀속과 Orphaned 재시도를 구현했다. 원거리·비행 슬롯 규칙은 유지한다.
- 전체 빌드와 초기 SurfaceNavigation 자동화 79/79는 통과했다. 실제 Mass 슬롯 배정 회귀를 추가한 최종 실행(`Phase07b_Unit4_AllSurfaceNavTests_Final.log`)은 테스트용 stack EntityManager의 공유 소유권 assertion으로 중단됐다. 테스트를 `MakeShared`로 수정했으며 재컴파일·재검증은 남아 있다.
- `Phase07b_Unit4_EditorGame.log`의 100마리 스모크는 frame·exact·lock PASS, UnknownHits/EnvelopeEscapes 0, 정상 종료했다. 경로 추종 159,108프레임·waypoint 전진 1,212회를 기록했다. 기존 Mass CharacterMovementComponent 추출 및 game 모드의 editor Python toolset 등록 오류가 있다.
- 별도 자동화 프로세스가 MCP 8000 포트를 먼저 사용한 사이 사용자가 에디터를 열어 MCP 서버 바인딩에 실패했다. 사용자에게 `ModelContextProtocol.StopServer`와 `ModelContextProtocol.StartServer 8000`을 요청했다. 이후 별도 자동화는 `-ModelContextProtocolPort=8001` 등으로 포트를 분리한다.
- 단위 4는 미완료이며 최종 자동화와 PIE 접근점 검증을 마친 뒤에만 다음 단위 진행 여부를 확인한다.
- 후속: 사용자가 풀빌드하고 에디터를 재시작한 뒤 MCP 연결을 확인했다. `Nav.EnemyReachability` 1/1과 전체 SurfaceNavigation 79/79가 오류·경고 0으로 통과했다(`Saved/Logs/Phase07b_Unit4_AllSurfaceNavTests_MCP.log`, 170.84초). 끊긴 섬 양방향 접근·정지는 사용자 PIE 확인을 요청했다. 테스트가 재저장한 에셋 3개의 git 원복은 열린 에디터 상태에서 파일 교체 오류로 실패했으며, 정리가 남아 있다.
- PIE 크래시 후속: `LNPEnemyPathProcessor.cpp:440`의 `Context.GetMutableSubsystemChecked<UMassSignalSubsystem>()`이 query에만 선언된 접근 권한을 사용할 수 없어 `Undeclared read/write access`와 `InstancePtr` assertion을 발생시켰다(`Phase07b_Unit4_PIECrash.log`). 요구사항을 `ProcessorRequirements`로 옮겨 전체 빌드를 통과했다. PIE 재검증은 남아 있다. 에디터 종료 후 테스트 에셋 3개의 원복은 성공했다. Autosaves 161개·50,385,604B를 `Saved/RecoveryBackups/Phase07b_Unit4_PIECrash_Autosaves`에 보존했다. 복구 JSON의 Packages는 비어 있고 최근 에셋 자동 저장은 9월 27일이라 이번 Unsaved 변경 복구를 보장할 수 없다.
- 수정 후 PIE 재검증: 사용자가 에디터를 다시 연 뒤 MCP로 PIE를 시작하고 20초 이상 실행했다. 로딩을 통과했고 subsystem 접근 오류·ensure·assertion은 재발하지 않았으며 적 추격·공격 상태 전환이 기록됐다(`Phase07b_Unit4_PIELoadFixed.log`). PIE를 실행 상태로 두고 사용자에게 끊긴 섬 양방향 접근·정지를 요청했다.
- 사용자 양방향 플레이 관찰: 지각↔섬 이동 시 NPC가 추격을 멈추고 Pod 배회로 복귀했다. 접근점 도착·정지를 확인한 결과로 간주하지 않는다. MCP로 두 근접 Config의 실제 값(추적 유지 2,500cm·Pod 세력권 5,000cm·Alert 인내 8초)을 확인했다. 거리 판정은 높이 차를 포함하고 기존 포기 규칙은 유지하므로, 테스트용 유지 8,000cm·세력권 10,000cm·인내 60초 임시 적용 후 원복하는 방법을 사용자에게 제안했다. 아직 설정은 변경하지 않았다.
- 사용자 승인 후 두 근접 Config에 유지 8,000cm·세력권 10,000cm·인내 60초를 MCP로 임시 적용하고 재조회로 확인했다. 에셋은 저장하지 않았다. 원본은 `Saved/RecoveryBackups/Phase07b_Unit4_TargetingOriginals.json`에 보존했다.
- 단위 4 완료: 사용자가 섬 위로 피할 때 지각 적이 섬 바로 아래에 모이고, 지각으로 피할 때 섬 적이 가장자리에 모이는 양방향 동작을 확인했다. 두 Config의 모든 targeting 값을 원본으로 복구하고 MCP 재조회로 유지 2,500cm·세력권 5,000cm·인내 8초를 확인했다. Content의 git 변경은 없다. 기존 거리·포기 규칙과 맵은 유지한다. 단위 5는 사용자 진행 확인 전까지 착수하지 않는다.

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

## 2026-10-01 — Phase 7b 단위 5 계측·기능 검증, 성능 Gate 미통과

사용자가 단위 5 진행을 승인했고 전체 빌드 전에 에디터를 종료했다. 계측·캡처·기능 검증을 수행했지만 700마리 성능 완료 조건은 충족하지 못했다. Phase 7b는 미완료로 유지하고 후속 성능 개선 진행 확인을 기다린다.

### 구현

- `LNPLoadBaseline`에 `-LNPLoadBaselineChase`를 추가했다. 준비 단계에 링 중심에서 네 방향 25m 지점을 slot 4 지각 Nav node로 투영하고, 실제 지상 적이 공유하는 목표를 5초마다 반대편으로 바꾼다. 타겟팅·슬롯·공격·넉백·투사체 부하는 유지한다. 처음 캐시만 조회했을 때 네 방향 중 두 방향이 빈 구간이라 자연 추격으로 폴백했으므로, Nav 투영으로 네 목표를 보장하도록 고쳤다. 최종 CSV의 추격 목표 좌표가 정확히 네 종류임을 확인했다.
- `LNPNavPathSubsystem`이 capture 구간에 제출된 요청을 메모리에 모으고 종료 후 CSV로 저장한다. 형식과 재생 입력 규약은 Phase 7b 문서의 단위 5가 소유한다. 제출 수와 CSV 행 수가 다섯 실행 모두 정확히 같다.
- 프레임별 게임 스레드 scheduler 시간, 확장 작업 경과 시간 합, 확장 수, tick 뒤 남은 실행·대기 요청 수를 표본화한다. tick하지 않은 프레임은 이전 값 대신 0을 넣는다. 제출·종료 상태·cache hit/miss는 warm-up 끝 누계의 차분이다. `searchSumMs`는 OS 스레드 CPU 시간이 아니라 작업별 벽시계 경과 시간 합이며 참고값이다.
- `Scripts/Profiling/RunNavChaseMatrix.ps1`은 자연 추격 700, 합성 추격 700·800을 기본으로 실행한다. 500·600 등의 하향 탐색은 시나리오 이름을 지정한다. 프로세스 종료 코드도 검사해 종료 크래시를 스모크 성공으로 오인하지 않는다.

### 작은 최적화와 종료 크래시

- `LNPNavGraph::ScanLayer`가 최근접 하나만 필요한 소비자에서는 최소 후보 하나를 유지한다. 접근점은 전체 후보 저장·정렬을 제거하고 기존 거리·node 동률 규칙으로 같은 점을 고른다. Enemy·Player 단일 node 투영도 같은 경로를 쓴다. `Nav.GraphView`에 회귀·production 후보의 전체 정렬 결과와 최근접 전용 결과의 node·거리 일치 검사를 추가했다. 기존 D-062 공통 group 재선택은 전체 후보를 계속 쓴다.
- Pod 재귀속에서 먼 후보를 생략하는 변경을 잠시 넣었으나, baseline 적은 Pod에 귀속되지 않아 이번 Gate에 영향을 주지 않는 것으로 확인하고 제거했다.
- 첫 합성 700 실행은 capture·보고 뒤 게스트가 종료될 때 호스트에서 `CurrentArchetype` assert가 났다. callstack은 `ULNPEnemyPathProcessor`의 `GetFragmentDataPtr<FLNPPlayerNavFragment>(Target.TargetPlayer)`였다. 단순 `IsValid()`는 파괴된 엔티티를 걸러내지 못하므로 `IsEntityActive()`로 타겟 생존을 먼저 확인하고, 사라진 타겟은 경로 취소 분기로 보낸다. 최종 리슨 2P 다섯 실행에서 assert·ensure·crash가 없다.
- 작은 정렬 절감만으로 Gate는 해결되지 않았다. 초기 자연/합성 700은 각각 프레임 P95 21.03/21.70ms, 경로 tick P95 2.142/3.161ms였다. 최종 실행은 아래 표처럼 더 높았다. frame delta에 따른 넉백·플레이어 접지·요청 상태 구성이 달라지므로 이 차이를 최적화 효과나 회귀의 확정 수치로 해석하지 않는다. 초기 로그·CSV는 `*_BeforeOptimization_*`로 보존했다.

### 최종 검증

- 에디터 전체 빌드 성공: `Saved/Logs/Phase07b_Unit5_OptimizedBuild.log`.
- 전체 자동화 79/79: `Phase07b_Unit5_OptimizedTests.log`. 재저장된 테스트 전용 에셋 세 개는 실행 전 Git 상태로 복구했다.
- Win64 Development BuildCookRun 성공: `Phase07b_Unit5_OptimizedPackage.log`. 기존 Lyra Material Function 누락 등 콘텐츠 경고는 재현됐고 코드 컴파일 오류는 없다.
- 최종 패키지 1P 100마리 합성 추격: `Phase07b_Unit5_FinalPackage1P.log`. 프레임 P50/P95 3.99/5.00ms, 경로 tick P95 0.024ms, 요청 1,186건, cache hit 55.66%, waypoint 진행 4,211회. 네 목표 생성·CSV 저장·probe·종료 정상.
- 최종 리슨 2P 다섯 실행의 host/guest 10개 로그: assert·ensure·crash 0. host Layer jump·Unknown hit·Envelope escape 0, 패널 게시 순서 위반 0, 각 probe PASS.

### Development 패키지 리슨 2P 측정

공통 조건은 seed 1, 지상 적만, 투사체 500, `-nullrhi -nosound -corelimit=4`, cache-first·lateral sweep·parallel movement 기본 유지, warm-up 10초·capture 30초다. scheduler는 예산 4,000·scratch 4·cache 256·병렬을 유지했다. 로그와 CSV는 `Saved/Profiling/Phase07b/<scenario>_{Host.log,Guest.log,Requests.csv}`다.

| 시나리오 | 프레임 P50 / P95(ms) | 경로 tick P50 / P95(ms) | 확장 작업 합 P95(ms) | 확장 P50 / P95 | 남은 running / queued P95 | 제출·CSV 행 수 | cache hit율 | 두 Gate |
|:---|---:|---:|---:|---:|---:|---:|---:|:---|
| N700_natural | 19.07 / 28.38 | 0.714 / 8.697 | 0.899 | 46 / 2,132 | 0 / 0 | 8,013 | 27.60% | 실패 |
| N500_chase | 10.90 / 13.12 | 0.050 / 1.047 | 0.169 | 18 / 464 | 0 / 0 | 7,389 | 62.51% | 통과 |
| N600_chase | 13.48 / 16.18 | 0.096 / 1.501 | 0.375 | 53 / 1,039 | 0 / 0 | 8,024 | 58.61% | 경로 경계 실패 |
| N700_chase | 18.47 / 22.89 | 0.568 / 4.781 | 1.287 | 114 / 3,011 | 3 / 27 | 9,525 | 61.35% | 실패 |
| N800_chase | 20.50 / 26.21 | 0.433 / 5.174 | 1.458 | 153 / 3,424 | 4 / 162 | 10,719 | 62.02% | 실패 |

완료 기준은 프레임 P95 16.67ms 이하·경로 tick P95 1.5ms 이하다. 600의 1.501ms는 근접하더라도 이번 실행에서 실패로 기록하며 기준을 바꾸지 않는다. running·queued는 tick 뒤 남은 요청 수로, 같은 tick 안에서 시작·완료된 요청은 이 수에 남지 않는다.

| 시나리오 | Succeeded | Unreachable | NoNode | NoPath | Stale | Cancelled |
|:---|---:|---:|---:|---:|---:|---:|
| N700_natural | 1,547 | 1,866 | 4,589 | 0 | 0 | 11 |
| N500_chase | 5,572 | 1,798 | 0 | 0 | 0 | 19 |
| N600_chase | 6,096 | 1,888 | 0 | 0 | 0 | 40 |
| N700_chase | 7,262 | 2,286 | 0 | 0 | 0 | 64 |
| N800_chase | 8,425 | 2,133 | 0 | 3 | 0 | 98 |

warm-up에서 시작한 요청이 capture에서 끝날 수 있으므로 종료 합계와 capture 제출 수는 다를 수 있다. `Unreachable`도 접근점 경로를 포함할 수 있다. 합성 모드의 네 목표 자체는 유효한 Nav node이고, 섬의 별도 group은 기존 D-063 접근점 경로를 따른다.

Phase 6 cache-first의 최대 확인 통과점은 800마리였다. 이번 합성 추격에서는 두 Gate 모두 통과한 최대 확인점이 500이고, 600은 경로 Gate 경계다. 프레임 기준만 보면 600 통과·700 실패다. 정확한 최대값은 추가 탐색·반복으로 확정하지 않았다. 합성 모드는 실제 슬롯을 받지 못한 적도 가상 목표로 이동시키므로 Phase 6의 자연 추격과 수요가 같지 않다. 자연 추격 700도 이번 실행에서 실패했으므로 수용량 감소 자체는 후속 과제다.

### 다음 작업

사용자에게 성능 개선을 계속할지 확인한다. 승인 후 스냅·접근점·cache·경로 단순화/종료 처리와 Enemy·슬롯 소비자의 시간을 먼저 분해한다. scheduler의 요청 시작 비용은 현재 고정 16 확장이지만 접근점 처리 비용까지 똑같이 환산하는 점이 재검토 대상이다. sparse Layer 접근점 스캔과 매 프레임 중복 조회도 후보이며, 측정 없이 원인으로 확정하지 않는다. 700마리 두 Gate 통과 전에는 Phase 7b 완료나 7c 전환을 하지 않는다.

## 2026-10-01 — 성능 개선 단위 1: 단계별 계측·빈 Tile 건너뛰기

사용자 승인 후 요청 시작(스냅·접근점·cache), 확장(병렬 대기 포함), 종료 처리, Enemy 경로 소비, 슬롯 재배분의 프레임별 P50/P95를 추가했다. 확장 작업별 시간 합은 계속 참고값으로만 둔다. 슬롯 시간의 쓰기·읽기는 기존 DataLock으로 보호한다.

### 구현과 검증

- `ScanLayer`는 같은 (J, I) 창에서 한 행의 TileId를 먼저 확인하고 빈 Tile의 셀 16개를 건너뛴다. 존재하는 Tile도 셀마다 주소 변환·Tile 조회를 반복하지 않는다. 창 범위·world 거리 계산·후보 동률 규칙·방문 순서·overlay 차단 검사는 유지했다.
- 확장 예산 4,000·scratch 4·cache 256·요청 시작 비용 16은 그대로다. 이번 단위에서 예산이나 Gate를 바꾸지 않았다.
- `Nav.GraphView`에 주소 조회를 사용하지 않는 전 node 필터 oracle을 추가했다. 기존 창의 좌표·거리 조건을 적용해 회귀 336입력·production 384입력에서 후보 집합을 비교한다. Tile 경계·삼각 격자 끝·희소 Layer·300/3,000cm 반경을 포함한다.
- 전체 빌드 `Saved/Logs/Phase07b_Perf1_FinalBuild.log`, 전체 SurfaceNavigation 79/79 `Phase07b_Perf1_FinalAllTests.log`, BuildCookRun `Phase07b_Perf1_FinalPackage.log` 통과. 테스트가 다시 저장한 에셋 세 개는 실행 전 상태로 복구했다.
- 수정 전 계측 패키지 1회와 수정 후 4회, 리슨 2P host/guest 모두 정상 종료했다. 각 실행의 네 probe는 양쪽 PASS(8개), assert·ensure·crash·Unknown hit·Envelope escape·Layer jump·순서 위반 0이다. CSV 행 수가 capture 제출 수와 모두 일치한다.

### 패키지 측정

기존 단위 5와 같은 seed 1·지상 적·투사체 500·리슨 2P·`-nullrhi -corelimit=4`·warm-up 10초·capture 30초다. 시간은 ms다. 로그/CSV 경로의 공통 접두사는 `Saved/Profiling/Phase07b`이며 아래 이름에 `_{Host.log,Guest.log,Requests.csv}`가 붙는다.

| 실행 / 파일 접두사 | 프레임 P50 / P95 | 경로 tick P50 / P95 | 확장 P50 / P95 | 잔여 running / queued P95 | 제출·CSV | cache hit율 | Gate |
|:---|---:|---:|---:|---:|---:|---:|:---|
| 수정 전 700 추격 / N700_chase_Perf1_Before | 14.52 / 17.80 | 0.197 / 2.980 | 96 / 3,443 | 3 / 42 | 10,269 | 63.67% | 실패 |
| 수정 후 700 자연 / N700_natural | 12.36 / 13.63 | 0.042 / 0.546 | 8 / 907 | 0 / 0 | 5,081 | 24.50% | 통과 |
| 수정 후 700 추격 1 / N700_chase_Perf1_After1 | 13.08 / 14.97 | 0.090 / 0.640 | 53 / 1,392 | 0 / 0 | 9,346 | 62.47% | 통과 |
| 수정 후 700 추격 2 / N700_chase | 17.11 / 19.61 | 0.188 / 1.891 | 118 / 3,410 | 3 / 7 | 9,730 | 60.83% | 실패 |
| 수정 후 800 추격 / N800_chase | 18.38 / 21.17 | 0.209 / 1.794 | 128 / 2,883 | 3 / 38 | 10,524 | 61.32% | 실패 |

| 실행 | 요청 시작 P50 / P95 | 확장 대기 P50 / P95 | 종료 P50 / P95 | Enemy 소비 P50 / P95 | 슬롯 P50 / P95 |
|:---|---:|---:|---:|---:|---:|
| 수정 전 700 추격 | 0.038 / 2.162 | 0.038 / 0.743 | 0.013 / 0.114 | 3.006 / 3.455 | 1.047 / 1.332 |
| 수정 후 700 자연 | 0.012 / 0.242 | 0.006 / 0.300 | 0.006 / 0.042 | 1.924 / 2.121 | 0.155 / 0.292 |
| 수정 후 700 추격 1 | 0.020 / 0.228 | 0.024 / 0.425 | 0.010 / 0.054 | 2.077 / 2.416 | 0.883 / 1.145 |
| 수정 후 700 추격 2 | 0.104 / 0.528 | 0.052 / 0.828 | 0.019 / 0.148 | 2.725 / 3.193 | 0.924 / 1.197 |
| 수정 후 800 추격 | 0.113 / 0.807 | 0.056 / 0.706 | 0.022 / 0.229 | 3.104 / 3.625 | 1.080 / 1.421 |

단계별 P95를 더한 값은 전체 tick의 P95가 아니다. 실행별 실제 이동·요청 구성이 달라 확장 수도 크게 변했다. 요청 시작 비용은 줄었지만 700 추격의 반복 결과가 Gate를 넘으므로 안정적인 통과·새 최대 수용량으로 확정하지 않는다. 이전 단위 5의 700/800 결과는 `N700_chase_Unit5_*`, `N800_chase_Unit5_*`로 보존했다. Phase 7b는 미완료다.

### 발견한 기존 정확성 문제와 다음 단위

최초 독립 검사는 창 좌표 제한 없이 전체 node를 거리만으로 거른 결과와 비교했다. 회귀·production 각각 8개 입력(각 slot의 Layer 0 끝 node 근처 3,000cm)에서 기존 창 밖의 후보를 놓쳤다. 예를 들어 회귀 slot 0은 창 안 810개·반경 안 전체 924개, production slot 0은 763개·869개다. 진단은 `Saved/Logs/Phase07b_Perf1_WindowDiagnostic.log`에 남았다. 최적화 이전부터 쓰던 각도→(I, J) 창의 범위 문제이며 이번 변경은 범위를 유지했다. 최종 oracle은 이번 최적화의 보존 조건인 기존 창 안 후보 일치를 검증한다. 반경 내 전체 후보를 보장하는 검증과 수정은 아직 끝나지 않았다.

이 구현 단위를 끝낸 뒤 사용자에게 다시 진행 여부를 확인한다. 다음 단위는 기존 검색 창의 경계 누락을 먼저 수정·회귀 검증하고, 동일 CSV 입력 비용 비교와 요청 시작·확장 대기·종료 예산 검토로 이어갈 수 있다. Gate 16.67ms·1.5ms는 유지한다. 승인 전에는 추가 구현이나 7c로 넘어가지 않는다.

## 2026-10-01 — 검색 창 정확성 수정 단위

사용자의 Phase 7 재개 요청으로 이전 기록의 꼭짓점 검색 누락을 먼저 해결했다. 구현 단위 완료 뒤 다음 단위를 다시 확인하는 규칙을 유지한다.

### 구현과 재현

- `Nav.GraphView`의 독립 oracle에서 기존 창 좌표 제한을 제거했다. 같은 720입력(회귀 336·production 384, 300/3,000cm 반경)을 전 node의 world 거리·막힘 조건만으로 비교한다. 수정 전 회귀·production 각각 8입력이 실패했다(`Saved/Logs/Phase07b_WindowFix_Repro.log`). 실행 프로세스는 exit 0이었지만 테스트의 `Result={Fail}`을 확인했다.
- `LNPNavData::GetGridSearchBounds`는 slot local 반경 구를 포함하는 양의 좌표 상자에서 `x/(x+y+z)`, `y/(x+y+z)`의 단조성으로 좌표 범위를 계산한다. float node 반올림을 덮는 한 cell 여유와 최종 world 거리 필터를 쓴다. Layer 기준 반지름·실제 지면 높이에 의존하지 않는다.
- 조밀 `ScanLayer`와 진단 `ProjectToNode`의 각도 기반 창을 이 공통 범위로 교체했다. Tile 건너뛰기·(J,I) 방문 순서·동률·overlay·요청 예산은 유지했다. 스키마·베이크 입력은 바뀌지 않았다.
- `LootNPopEditor Win64 Development` 전체 빌드 성공(49.04초), 전체 SurfaceNavigation 79/79 통과(`Saved/Logs/Phase07b_WindowFix_AllTests.log`). 창 제한 없는 oracle 720입력이 모두 일치했다. 자동화가 다시 저장한 테스트 에셋 세 개는 실행 전 상태로 복구했다.
- Development BuildCookRun의 build·cook·stage·pak·archive 성공(99.03초, `Saved/Logs/Phase07b_WindowFix_Package.log`).

### 수정 후 패키지 700마리 반복 측정

기존과 같은 seed 1·투사체 500·리슨 2P·합성 추격·`-nullrhi -corelimit=4`, warm-up 10초·capture 30초 조건이다. 아래 파일은 `Saved/Profiling/Phase07b`에 있고 접두사 뒤 `_{Host.log,Guest.log,Requests.csv}`가 붙는다.

| 실행 / 파일 접두사 | 프레임 P50 / P95(ms) | 경로 tick P50 / P95(ms) | 확장 P50 / P95 | running / queued P95 | 제출·CSV | cache hit율 | Gate |
|:---|---:|---:|---:|---:|---:|---:|:---|
| 1 / N700_chase_WindowFix1 | 16.16 / 18.60 | 0.102 / 1.042 | 74 / 2,200 | 0 / 0 | 8,610 | 61.90% | 프레임 실패·경로 통과 |
| 2 / N700_chase_WindowFix2 | 15.00 / 17.19 | 0.122 / 1.287 | 74 / 2,913 | 1 / 0 | 9,298 | 60.29% | 프레임 실패·경로 통과 |

| 실행 | 시작 P50 / P95(ms) | 확장 대기 P50 / P95(ms) | 종료 P50 / P95(ms) | Enemy 소비 P50 / P95(ms) | 슬롯 P50 / P95(ms) |
|:---|---:|---:|---:|---:|---:|
| 1 | 0.020 / 0.188 | 0.033 / 0.623 | 0.014 / 0.105 | 1.737 / 2.142 | 0.660 / 0.903 |
| 2 | 0.045 / 0.270 | 0.036 / 0.678 | 0.015 / 0.105 | 1.794 / 2.114 | 0.613 / 0.817 |

host/guest 네 프로세스 모두 exit 0, 네 probe가 각 로그마다 PASS(총 16개), assert·ensure·crash·Unknown hit·Envelope escape·Layer jump·게시 순서 위반 0이다. CSV 행 수는 제출 수와 일치한다. 기존 Mover 초기화 경고와 Mass의 CharacterMovementComponent 추출 오류 로그는 남아 있으며 이번 단위에서 변경하지 않았다.

검색 정확성 단위는 완료했지만 두 실행 모두 프레임 Gate를 넘으므로 Phase 7b는 미완료다. 실행별 실제 경로 수요가 다르므로 이전 패키지와의 수치 차이를 이번 수정의 성능 개선으로 단정하지 않는다. 1P·800마리 재측정은 이번 단위에서 하지 않았다. 다음 후보는 저장된 동일 CSV 입력으로 요청 비용을 비교하고 시작·확장 대기·종료 예산을 검토하는 것이다. 추가 구현이나 7c로 넘어가기 전에 사용자에게 진행 여부를 확인한다.

## 2026-10-02 — 동일 CSV 요청 비용 비교·scheduler 예산 검토 단위

사용자가 다음 단위로 승인한 동일 CSV 비교와 예산 검토를 수행했다. 이번 변경은 에디터 자동화 `Nav.RequestCostReplay`와 문서다. 기존 검색 창 정확성 수정은 유지했으며 runtime scheduler·CVar 기본값·Gate는 바꾸지 않았다.

### 비교 규약과 재실행

- 입력은 `Saved/Profiling/Phase07b/N700_chase_WindowFix{1,2}_Requests.csv`의 8,610/9,298건 전부다. production 8-slot snapshot generation 1·graph version 1과 CSV schema·숫자·시각 순서·generation/version 호환성을 검사한다.
- 같은 CSV 시각의 요청을 한 묶음으로 제출하고 모두 완료한 뒤 다음 묶음을 넣는다. 시간 간격은 재생하지 않는다. 원래 owner·serial 대신 행별 독립 owner를 부여해 취소·소비자 타이밍의 차이가 비용 비교를 바꾸지 않게 한다. 완료 owner는 묶음 뒤 제거하며 cache는 유지한다.
- CSV는 Pod 차단 mask·Pod 위치·취소/owner 소멸·프레임 경계·warm-up cache를 담지 않는다. overlay 없이 재생하며 캡처의 overlay revision을 실제 차단 상태로 간주하지 않는다. 이 도구는 입력 좌표가 같은 통제 비교이며 원래 플레이의 요청 상태·cache hit율·프레임 Gate를 재현하는 도구가 아니다.
- 같은 입력마다 기본(4,000/16/병렬/cache 0), 예산 2,000, 시작 비용 64, 직렬, cache 256 순으로 실행한다. scratch 4·누적 확장 상한 30,000은 같다. 순서가 고정된 한 번씩의 실행이므로 작은 시간 차이는 반복 측정 없이 확정하지 않는다. 각 설정은 새 scheduler·빈 scratch/cache로 시작해 최초 할당 시간도 포함한다.
- 캐시 없는 네 설정은 요청별 상태·waypoint node 열·확장 수·cost를 기본과 비교한다. 두 CSV 모두 불일치 0이다. cache 설정은 공유 경로가 달라질 수 있어 이 동일성 비교에서 제외하며 비용과 hit만 기록한다.
- tick CSV는 `WindowFix{1,2}_RequestCosts.csv`, 자동화 로그는 `Saved/Logs/Phase07b_RequestCostReplay{1,2}.log`다. tick·시작·확장 대기·종료 시간과 worker 시간 합, 시작 수·확장 수·잔여 queue/running을 남긴다. 병렬 worker 시간 합은 게임 스레드 확장 대기 시간과 다르다.

PowerShell 재실행 예시(엔진·사용자 캐시에 기록하므로 권한 확장 실행):

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UnrealProjects\LootNPop\LootNPop.uproject' -unattended -nullrhi -nosound -corelimit=4 '-ExecCmds=Automation RunTests LootNPop.SurfaceNavigation.Nav.RequestCostReplay' '-TestExit=Automation Test Queue Empty' '-LNPNavReplayCsv=D:\UnrealProjects\LootNPop\Saved\Profiling\Phase07b\N700_chase_WindowFix1_Requests.csv' '-LNPNavReplayOutput=D:\UnrealProjects\LootNPop\Saved\Profiling\Phase07b\WindowFix1_RequestCosts.csv' '-abslog=D:\UnrealProjects\LootNPop\Saved\Logs\Phase07b_RequestCostReplay1.log'
```

CSV 인자가 없는 전체 자동화에서는 안내를 남기고 비교를 건너뛴다. 입력을 지정하면 출력 경로도 필수이며 잘못된 schema·숫자·generation/version이면 실패한다.

### 동일 입력 측정

시간은 ms다. P95 분모는 **실제로 실행한 scheduler tick**이며 idle tick·실제 플레이 프레임은 포함하지 않는다. 처리 tick 수·잔여 큐는 실제 게임의 요청 지연이 아니라 묶음별 통제 부하 지표다. 단계별 P95의 합은 tick P95가 아니다.

| CSV / 설정 | tick 수 | tick P50 / P95 / 최대 | 시작 / 확장 대기 / 종료 P95 | 확장 합 | cache hit | 잔여 큐 P95 |
|:---|---:|:---|:---|---:|---:|---:|
| 1 기본 | 1,900 | 0.119 / 1.285 / 2.635 | 0.193 / 0.859 / 0.224 | 1,150,364 | 0 | 163 |
| 1 예산 2,000 | 2,087 | 0.134 / 0.732 / 2.160 | 0.144 / 0.473 / 0.129 | 1,150,364 | 0 | 349 |
| 1 시작 비용 64 | 1,943 | 0.120 / 1.032 / 1.911 | 0.168 / 0.671 / 0.176 | 1,150,364 | 0 | 248 |
| 1 직렬 | 1,900 | 0.124 / 1.681 / 2.713 | 0.196 / 1.293 / 0.196 | 1,150,364 | 0 | 163 |
| 1 cache 256 | 1,806 | 0.072 / 0.782 / 2.677 | 0.123 / 0.553 / 0.078 | 564,829 | 4,143 | 0 |
| 2 기본 | 2,024 | 0.130 / 1.302 / 2.311 | 0.201 / 0.825 / 0.221 | 1,243,024 | 0 | 129 |
| 2 예산 2,000 | 2,230 | 0.154 / 0.831 / 2.472 | 0.172 / 0.523 / 0.149 | 1,243,024 | 0 | 313 |
| 2 시작 비용 64 | 2,068 | 0.147 / 1.164 / 2.932 | 0.207 / 0.746 / 0.204 | 1,243,024 | 0 | 208 |
| 2 직렬 | 2,024 | 0.135 / 1.834 / 2.699 | 0.211 / 1.366 / 0.218 | 1,243,024 | 0 | 129 |
| 2 cache 256 | 1,941 | 0.087 / 1.291 / 2.327 | 0.206 / 0.712 / 0.099 | 673,813 | 4,147 | 0 |

기본 설정의 요청당 시작 단계 평균은 9.84/10.90us다(시작 단계 합/시작 수, cache 조회·초기 scratch 할당·즉시 종료도 포함). 기본 tick의 시간 합은 488.3/547.0ms이며 시작 84.7/101.3ms, 확장 대기 320.7/355.5ms, 종료 81.4/88.6ms다. cache 256의 총 tick 시간은 287.7/384.6ms이고 확장량은 약 51%/46% 줄었다. 현재 cache를 유지할 근거이며 실제 플레이 hit율과 같은 수치는 아니다.

### scheduler 예산 검토 결론

1. **확장 수 예산은 엄격한 상한이 아니다.** `Budget > 0`이면 다음 요청을 시작해 16/64를 차감하므로 잔여 예산이 시작 비용보다 작아도 실행한다. 또 남은 예산이 running 수보다 작으면 `Share=max(1, Budget/RunningCount)`가 전체 잔여량을 넘는다. 기본 설정의 최대 차감(`started×16+expansions`)은 4,014/4,015, 시작 비용 64는 4,062/4,063이다. 기본 125/139 tick에서 4,000을 넘었다. 작은 수치 초과만으로 기존 프레임 Gate 실패 원인이라고 단정하지 않는다.
2. **시작 비용 16은 시간 상한이 아니다.** 초기 근거 0.38us/확장으로 환산하면 6.08us이고 이번 평균 시작 비용보다 작다. 고정 비용만 64로 올리면 P95가 낮아졌지만 큐 P95와 처리 tick이 늘며 시간 최대값을 보장하지 않는다. 전체 평균으로 접근점·cache hit·NoNode 각각의 비용을 확정할 수는 없다.
3. **종료 처리는 확장 예산에 별도 차감되지 않는다.** waypoint 단순화·통과 Tile 수집·cache 등록은 완료된 요청 수와 길이에 따라 실행된다. `StartRequest`의 즉시 종료는 현재 시작 단계 시간에 들어간다. 종료 P95 약 0.22ms도 실제 비용이므로 확장량만으로 전체 tick 시간을 추정하면 빠진다.
4. **병렬 대기는 시간 예산에 묶이지 않는다.** 동일 확장량에서 직렬은 두 CSV 모두 tick P95가 1.5ms보다 높았다. 병렬은 유지한다. worker 작업 합을 시간 상한에 쓰면 병렬 대기를 놓치므로 실제 게임 스레드 경과 시간으로 검토해야 한다. 이 재생에는 Enemy 이동·슬롯·physics·복제 등 실제 워커 경합이 없다.
5. **예산을 2,000으로 낮추면 비용을 줄이기보다 여러 tick으로 나눈다.** tick 수가 9.8%/10.2% 늘고 큐 P95도 증가한다. 각 설정의 1.5ms 초과 tick도 남는다. 기본 경로 Gate가 최근 실제 패키지 두 번에서 이미 통과했으므로 이 결과만으로 runtime 기본값을 낮추거나 시간 예산을 추가하지 않는다.

다음 권장 단위는 차감 초과를 먼저 막는 최소 변경과 그 결정론·낮은 예산 회귀 검증이다. 시작·확장·종료를 포함한 시간 예산은 실제 패키지의 요청 대기 지연과 함께 판단한다. 비교용 overlay/취소/프레임 경계 캡처를 확장하면 실제 scheduler trace 재생이 가능하지만 이번 단위에는 넣지 않았다. 사용자에게 다음 단위를 진행할지 확인하며, Phase 7b 완료나 7c 전환은 하지 않는다.

### 검증

- `LootNPopEditor Win64 Development` 전체 빌드 성공(19.81초, 컴파일 오류·경고 0).
- CSV 자동화 두 번 모두 `Result={Success}`·exit 0. 캐시 없는 설정 비교 17,908요청 × 3변형의 상태·경로·확장·cost 불일치 0.
- 전체 SurfaceNavigation 자동화 80/80·exit 0(`Saved/Logs/Phase07b_RequestCostReplay_AllTests.log`). 기존 79개가 통과했고 신규 비교 항목은 CSV 인자가 없어 안내 후 건너뛰었다(실제 CSV 검증은 위 두 별도 실행). 테스트가 저장한 세 에셋은 실행 전 백업 내용으로 복구해 에셋 diff가 없다. `git diff --check` 통과.
- runtime 코드는 이번 단위에서 바꾸지 않았으며 cook/package·리슨 2P는 재실행하지 않았다. 앞선 패키지 Gate 실패 상태를 유지한다.
