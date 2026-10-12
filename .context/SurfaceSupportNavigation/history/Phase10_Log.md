# Phase 10 작업 로그

> 실행 문서: `phases/Phase10_ChasePathComparison.md`

## 2026-10-13 — 착수와 단위 0(harness 재정비)

### 착수 확인

- production에서 경로 비용은 병목이 아니다. V011 경로 tick P95는 0.014~0.021ms다. V011B production 캡처(30초 436건, Chase 18·Background 418)의 큐 대기는 P50 4.9ms, P95 6.4ms, 최대 13.2ms로, 대체로 다음 프레임에 시작된다. 같은 시각 제출 묶음은 최대 16건이다.
- Phase 7의 재생 CSV는 Meadow 단독 풀에서 캡처했다. 재생기는 배치 seed `135792468`로 snapshot을 만들고 generation·version만 확인했으므로, 다른 배치의 좌표도 통과시켰다. 현재 혼합 풀에서는 쓰지 않는다.
- 사용자 결정: 원인 분석과 기준선 개선을 먼저 하고 비교는 그다음에 한다. 시나리오 3종, flow field를 먼저 만들고, 채택 기준 5개를 모두 만족해야 한다(실행 문서).

### 단위 0 변경

- `ULNPNavPathSubsystem::EndRequestCapture`가 `<캡처>_Meta.txt`를 저장한다. 내용은 `octantGenSeed`, `generation`, `graphVersion`, `slotN=<SurfaceData 경로>`다.
- `Nav.RequestCostReplay`
  - sidecar가 필수다. 그 seed로 `BuildProductionSnapshot`을 만들고, slot별 SurfaceData 경로가 다르면 실패한다.
  - 요청 단위 결과 `<출력>_Requests.csv`를 추가했다. `doneTick`은 묶음 제출 뒤 끝난 tick(1부터)이며, 시간 측정 구간 밖에서 확인한다.
- `RunPhase13Gate.ps1`에 `N2000Chase` 케이스와 sidecar 보존(`*_NavRequests_Meta.txt`)을 추가했다.

### 합성 추격 harness의 slot 4 가정 수정

첫 N700Chase는 `Synthetic chase goals=0 (required=4)`로 중단됐다. 호스트만 종료하고 게스트가 남아 스크립트의 8분 제한에 걸렸다. 원인은 합성 부하 harness 전체(적 링 배치·링 중심·목표)가 `IslandSlot = 4`(Meadow 단독 풀의 큰 섬)를 상수로 가정한 것이다. seed 1 배치(OctantGenSeed 1768)에서 slot 4는 Hollow다. 그래서 링에는 적이 462/700기만 놓이고, 목표는 구조물 Layer(18·20)에 걸렸다.

- `GetIslandSlot()`: `-LNPLoadBaselineIslandSlot=`(기본 4, 기존 동작 유지). Gate 스크립트에 `-IslandSlot`을 추가했다. Phase 10 캡처는 seed 1 배치의 Meadow이면서 -Z 반구인 slot 5를 쓴다.
- 목표 탐색
  - 링 slot의 지각 Layer 0에서 반경 2,500을 먼저 보고, 없으면 2,000~4,000으로 바꿔 가며 찾는다.
  - 처음에는 Support 조회 handle로 slot을 얻었으나, 링 중심이 섬 가장자리 아래라 위 섬 때문에 HighConfidence가 나오지 않아 목표가 0개였다. 그래서 Layer 0을 직접 쓴다.
  - 링 중심 node와 같은 ReachabilityGroup을 먼저 찾고, 없으면 group과 무관하게 받는다. 링 중심 node는 섬 아래 작은 component(group 437)에 걸렸고, 목표 넷은 모두 지각 주 group 0의 반경 2,500에서 찾았다.
- 실패한 실행 기록은 `Saved/Profiling/Phase10/Capture/FailedGoals{,2..5}`에 보존했다.
- 수용한 차이: Phase 13 이후 production 편성에서 ActorPromoted가 빠져 `DA_EnemyEntityConfig_ActorPromoted_Melee01`이 쿡되지 않는다. 합성 부하의 인덱스 `% 10 == 0`(10%)은 `load baseline enemy config 0 failed to load`로 스폰되지 않는다. 실제 지상 개체는 N700에서 약 630, N2000에서 약 1,800이다. D-076의 PureEntity 90% 이상 구성과 맞아 쿡 설정은 바꾸지 않았다.

### 캡처(Development 패키지 `Saved/SurfaceNavigationPhase10Package`, 리슨 2P, seed 1, 30초)

| 시나리오 | frame P50/P95(ms) | 경로 tick P95(ms) | 요청 | 큐 대기 P50/P95/최대(ms) | 제출→종료 P95(ms) | 최대 동시 제출 묶음 | 종료 상태 |
|:---|:---|---:|---:|:---|---:|---:|:---|
| Production | 8.36/9.07 | — | 357 | 5.7/6.4/9.3 | 6.5 | 21 | Succeeded 357 |
| N700Chase(slot 5) | 9.21/10.43 | 0.254 | 7,881 | 6.6/93.3/117.8 | 94.3 | 550 | Succeeded 7,599·Unreachable 268·Cancelled 14 |
| N2000Chase(slot 5) | 18.86/21.40(FAIL) | 1.353 | 21,129 | 52.6/985.7/1,572.6 | 993.1 | 1,489 | Succeeded 18,714·Unreachable 749·Cancelled 816·미완료 850 |

- N2000의 frame FAIL은 D-076 동시 활성 경계(약 2,000기에 16.1ms)를 넘는 부하라서 예상된 결과다. 이 시나리오는 Gate가 아니라 몰림 재현용이다. 큐 대기 P95가 약 1초로, 목표 전환 몰림이 가장 뚜렷하다.
- 큰 묶음은 모두 5초 목표 전환 시각에 몰린다(550·542·517 / 1,489·1,474·1,462).
- 원본: `Saved/Profiling/Phase10/Capture/{Production,N700Chase,N2000Chase}_Repeat1_*`(요청 CSV·Timings·Meta·로그).

### 재생(`Nav.RequestCostReplay`, sidecar seed 1768로 snapshot 재구성)

| 입력 / 변형 | tick 수 | tick P50/P95/최대(us) | 확장 합 | cache hit | burst(묶음 ≥400) 완료 tick P50/P95/최대 |
|:---|---:|:---|---:|---:|:---|
| Production base | 288 | 20.0/86.5/2,953 | 15,767 | 0 | — |
| Production cache256 | 288 | 3.9/78.2/2,803 | 6,602 | 237 | — |
| N700Chase base | 2,572 | 77.3/840.5/4,239 | 1,281,061 | 0 | 13/27/32 |
| N700Chase cache256 | 2,463 | 22.2/328.8/4,316 | 548,662 | 4,618 | 5/11/14 |
| N2000Chase base | 2,445 | 571.8/1,043.4/5,988 | 5,416,689 | 0 | 44/134/197 |
| N2000Chase cache256 | 1,958 | 239.7/967.3/5,925 | 2,524,477 | 11,564 | 20/66/98 |

- 세 입력 모두 `Result={Success}`다. 캐시 없는 네 변형(base·budget2000·resolve64·serial) 사이의 상태·waypoint·확장·cost 불일치는 0이고, 예산 차감 상한도 통과했다.
- N700 실제 큐 대기 P95 93ms는 cache256 burst 완료 tick P95 11 × 프레임 약 9ms와 같은 규모다. 재생 완료 tick이 실제 지연을 대신하는 지표로 쓸 만하다.
- 음성 대조: Production CSV의 sidecar seed를 135792468로 바꾸면 `Slot 3 SurfaceData matches the capture`에서 `Result={Fail}`이 난다(`Saved/Logs/Phase10_Unit0_Replay_WrongSeed.log`).
- 원본: `Saved/Profiling/Phase10/Replay/*_RequestCosts{,_Requests}.csv`, 로그 `Saved/Logs/Phase10_Unit0_Replay_*.log`.

### 검증

- `LootNPopEditor` 빌드(최종 소스 포함 여러 번) 성공. 전체 `LootNPop.SurfaceNavigation` 자동화 129/129 Success(`Saved/Logs/Phase10_Unit0_AllTests.log`, harness 목표 수정 전 소스). 이후 변경은 `LNPLoadBaseline`의 합성 부하 경로뿐이라 자동화 범위 밖이며, 위 캡처 실행으로 검증했다.
- 테스트가 저장한 에셋 세 개는 `Saved/Tests/Phase10_Unit0_TestAssetBackup`에서 복구했다(Content diff 없음).
- BuildCookRun 성공(최종 `Saved/Logs/Phase10_Unit0_Package6.log`). 세 캡처 모두 호스트·게스트 exit 0이고, Gate 스크립트의 기능 검사(probe 3종 PASS, assert·ensure·UnknownHits·EnvelopeEscapes 0)를 통과했다.

### 단위 1로 넘기는 관찰

- N2000 cache256에서도 burst 완료 tick P95가 66이다. 프레임 예산 4,000 기준으로 보면, burst 하나(약 1,480건)의 확장 합이 예산의 수십 프레임 분량이다. H1(처리량 한계)과 맞는 규모이며 단위 1에서 정량화한다.
- cache 적중(같은 시작·목표 Tile)만으로 확장이 54%(N700)·53%(N2000) 줄었다. 같은 목표 공유(H5)의 여지가 크다는 신호다.
