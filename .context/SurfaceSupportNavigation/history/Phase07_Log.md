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
