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

## 2026-10-13 — 단위 1(목표 전환 큐 대기 원인 분석, 계측만)

### 변경(게임 동작 없음)

- `FLNPNavPathScheduler`: 타이밍 캡처 중에만 tick 처리량 기록 `FLNPNavSchedulerTickRecord`를 모은다. 항목은 시작 전 대기·실행 수, 시작 수(즉시 종료·cache hit 포함), 확장 수, 시작 비용 차감, 종료 수, 최대 실행 수, 끝난 뒤 대기·실행 수, 정지 사유다. 정지 사유는 대기열이 남았을 때 마지막 시작 루프가 멈춘 이유(`1` 예산 < 시작 비용, `2` scratch 만석)이며, scratch에 막힌 횟수와 처음 막혔을 때의 잔여 예산도 남긴다. 캡처 밖에서는 시각 조회·배열 기록을 하지 않는다.
- `ULNPNavPathSubsystem::EndRequestCapture`가 `<캡처>_Ticks.csv`·`_Reasons.csv`·`_Wakes.csv`를 함께 저장한다. 요청 CSV 스키마는 그대로다.
  - `_Reasons.csv`: `LNPEnemyPathProcessor`가 제출할 때 계산한 사유 비트(`ELNPNavRequestReason`)와 processor 기준 시작 Tile·목표 node·목표 Tile. 재요청 조건 7종에 더해, 버려진 요청의 재요청을 `AfterDirect`(직선 보행 구간에서 버림)와 `AfterCancel`(이동·도착·타겟 상태·배회 대역)로 나눴다. 직선 보행 분기만 `Status`를 None으로 되돌리는 점으로 가른다.
  - `_Wakes.csv`: 활동 processor에서 활동 대역이 더 활성 쪽으로 바뀐 개체와 월드 시각.
- `RunPhase13Gate.ps1`이 세 파일을 `<실행>_Nav{Ticks,Reasons,Wakes}.csv`로 보존한다.
- 분석 스크립트 `Scripts/Profiling/AnalyzePhase10QueueWait.py`(표준 라이브러리만): 캡처 5종과 같은 캡처의 재생 `_Requests.csv`를 요청 번호로 잇는다.
- 회귀: `Nav.PathScheduler`에 tick 기록 검사를 추가했다. 예산 부족 정지, scratch 만석 정지(잔여 예산 16 보존), 캡처 밖 tick 미기록을 확인한다.

### 캡처(새 패키지, 리슨 2P, seed 1, 30초, 합성 추격은 slot 5)

| 시나리오 | frame P50/P95(ms) | 경로 tick P95(ms) | 요청 | 큐 대기 P50/P95/최대(ms) | 최대 묶음 | 종료 상태 |
|:---|:---|---:|---:|:---|---:|:---|
| S1 Production | 7.29/7.80 | 0.015 | 355 | 5.0/5.4/6.9 | 12 | Succeeded 355 |
| S2 N700Chase | 7.65/8.28 | 0.169 | 7,894 | 5.3/81.7/107.0 | 535 | Succeeded 7,568·Unreachable 309·Cancelled 17 |
| S3 N2000Chase | 15.54/17.53(FAIL) | 1.133 | 21,528 | 24.3/742.4/1,177.3 | 1,507 | Succeeded 19,715·Unreachable 739·Cancelled 528·미완료 546 |

- 단위 0 캡처와 같은 규모다(N700 P95 93→82ms, N2000 986→742ms). N2000 frame FAIL은 단위 0과 같이 D-076 경계를 넘는 부하라 예상된 결과다.
- 세 캡처를 다시 재생했다(`Nav.RequestCostReplay` Success 3/3). 분석의 요청별 확장 수·cache 여부는 같은 캡처의 재생 결과다.
- S1의 요청 355건은 모두 Background(배회·귀속)다. 30초 창에서 헤드리스 플레이어를 확정 추격한 개체가 없었다. S1은 실제 플레이의 Chase 몰림을 대표하지 못하며, 깨움·배회 몰림만 본다.
- 원본: `Saved/Profiling/Phase10/Unit1/*`(캡처·`Replay/`·`Analysis.{json,txt}`), 로그 `Saved/Logs/Phase10_Unit1_*.log`.

### 원인별 큐 대기 기여

사유는 비트 묶음이다. 주 사유는 `NoPrior` > `AfterDirect` > `AfterCancel` > 타겟 교체 > 목표 Tile 변경 > drift > 경로 무효화 > 경로 없음 overlay > Stale 순으로 하나만 고른다. 앞의 셋은 비교 대상인 직전 요청이 없거나 버려진 상태라 나머지 비트가 의미를 잃기 때문이다. 대기는 시작-제출이며, 시작 전 끝난 요청은 종료 시각, 미완료는 캡처 끝까지로 센다. "목표 이동"은 목표 Tile 변경이나 drift 비트가 함께 켜진 비율이다.

| 시나리오 | 주 사유 | 요청 비율 | 대기 합 비율 | 대기 P95(ms) | 목표 이동 | 주도한 묶음(2건 이상 / ≥400) | 최대 묶음 |
|:---|:---|---:|---:|---:|---:|:---|---:|
| S2 | 목표 Tile 변경 | 32% | **60%** | 88.6 | 100% | 169(85 / 6) | 535 |
| S2 | AfterDirect | 26% | 29% | 87.7 | 51% | 643(317 / 0) | 5 |
| S2 | 타겟 교체 | 22% | 5% | 5.6 | 1% | 877(423 / 0) | 88 |
| S2 | AfterCancel | 20% | 5% | 5.6 | 10% | 914(389 / 0) | 6 |
| S3 | 목표 Tile 변경 | 34% | **55%** | 926.3 | 100% | 161(160 / 6) | 1,507 |
| S3 | AfterDirect | 26% | 27% | 681.5 | 48% | 377(364 / 0) | 15 |
| S3 | AfterCancel | 21% | 11% | 679.0 | 10% | 762(736 / 0) | 50 |
| S3 | 타겟 교체 | 19% | 6% | 383.8 | 1% | 637(623 / 0) | 81 |
| S1 | AfterDirect | 86% | 86% | 5.4 | 27% | 271(15 / 3) | 12 |
| S1 | NoPrior(깨움 2초 안) | 5% | 5% | 6.5 | 100% | 8(5 / 0) | 4 |
| S1 | NoPrior(그 밖) | 8% | 8% | 5.5 | 100% | 23(0 / 0) | 1 |

- **몰림은 목표 전환 하나뿐이다.** S2·S3의 큰 묶음 6개씩은 모두 5초 목표 전환 시각이다. 구성은 목표 Tile 변경 약 70%와 AfterDirect 약 30%다. AfterDirect도 옛 목표에 직선 보행으로 다가가 있던 개체가 목표가 바뀌어 다시 요청한 것이다. 큰 묶음이 큐 대기 합의 83%(S2)·65%(S3)를 차지한다.
- 타겟 교체(S2 22%·S3 19%)와 AfterCancel은 잔잔한 작은 묶음이고 대기 기여가 작다. 합성 추격의 목표는 타겟과 무관하므로, 타겟 교체 재요청은 harness 산물이다(목표가 같아 대부분 cache로 끝난다).
- drift 400cm 단독, 경로 무효화, 경로 없음 overlay, Stale은 주 사유로 나오지 않았다. 30초 창에 Pod overlay 변경이 없었다(Replan 재큐 0, PathInvalid 비트 0). **overlay 무효화 재계획은 이번 창에서 판정 불가**다.
- **Pod 깨움 몰림은 작다.** S1의 깨움은 776건·19프레임(한 프레임 최대 184기)이지만, 깨움 2초 안의 첫 요청은 16건이고 그런 요청이 과반인 묶음도 최대 12건이다. 깬 개체 대부분은 경로 없이 배회·직선 보행으로 시작한다. S2·S3는 전원 활성이라 깨움이 0이다.

### 가설 판정

| 가설 | 측정 | 판정 |
|:---|:---|:---|
| H1 예산 처리량 한계 | 큰 묶음의 예상 tick = Σ(재생 cache256 확장 + 16) ÷ 4,000. S2는 예상 11.2~14.9 vs 실제 12~13, S3는 34.4~91.4 vs 35~86(마지막 묶음은 캡처 끝에서 잘려 36 vs 24). 대기열이 남은 tick은 모두 예산을 다 썼고, 묶음 창 전체의 사용률은 97~100%다. | **지지.** 큐 대기 P95는 묶음 확장 총량 ÷ 예산으로 정해진다. |
| H2 scratch 4개 상한 | 대기열이 남은 tick(S2 74·S3 465)의 정지 사유는 예산 85%·81%, scratch 15%·19%다. 모든 tick이 시작 직후 이월된 4개 실행 때문에 한 번 막히지만(잔여 4,000), 그 예산은 실행 중 요청이 다 쓴다. | **기각.** 처리량 손실이 없다. scratch는 같은 tick 안의 시작 순서만 늦춘다. |
| H3 FIFO 순서 | 묶음 안 대기와 제출 순서의 순위 상관 0.94~1.00, 대기와 확장 수의 상관 약 0. 같은 확장 총량을 단일 예산 스트림으로 처리하는 모형의 완료 tick P50/P95: S2 FIFO 5~6/11~13 → 직선 거리 오름차순 4~5/10~13 → 확장 오름차순(사후 최적) 2/8~10. S3 FIFO 16~38/33~87 → 거리순 9~18/28~73 → 확장순 3~4/20~57. | **지지(크기 제한).** 순서만 바꾸면 P50이 S3에서 약 절반이 되지만, P95는 S2 0~17%·S3 12~22%만 줄고 묶음 전체 완료 시각은 그대로다. |
| H4 목표 이동 뒤 cache 적중 붕괴 | 재생 cache256 적중률은 묶음 0.58(S2)·0.59(S3)로 묶음 밖 0.59·0.54와 같다. (StartTile, GoalTile) 고유 쌍이 정하는 상한은 0.87~0.96이다. 상한과의 차이를 나누면 동시 miss(앞선 같은 key 탐색이 진행 중)는 묶음당 0~3건, 앞선 같은 key 경로가 이미 있는데 놓친 miss가 묶음의 26~35%다. 이 miss가 묶음의 cache 후 확장의 **55~82%**를 차지한다. | **기각(붕괴 없음).** 대신 cache 합류 검증 거부가 남은 비용의 대부분이다. 재생에는 overlay가 없고 묶음 고유 key(최대 142)가 용량 256보다 작으므로, 이 miss는 사실상 시작 node → 공유 경로 첫 waypoint 직선 보행 검사의 거부다. |
| H5 같은 목표 중복 탐색 | 큰 묶음은 모두 goal node 4개(합성 목표)다. 확장 합 대비 그룹별 최대 단일 확장(고유 탐색 하한)은 S2 약 12~26배, S3 약 33~59배다. cache 후 남은 확장(S2 3.6만~5.1만, S3 11.4만~34.2만)을 목표별 Dijkstra 추정(최대 경로 cost 반경 원판 node 수, S2 4.3만~7.8만·S3 8.4만~11.2만)과 비교하면, S3에서만 cache 후 A*가 1.3~3.1배 크다(S2는 0.5~0.9배). | **지지.** 남은 묶음 비용은 같은 목표 요청의 반복 탐색이다. 원판 추정은 지형 차단을 무시한 상한이라 flow field 이득의 대략적 규모만 본다. |

### 단위 2 개선 후보 순위

1. **cache 합류 검증 완화.** 공유 경로의 첫 waypoint 대신 앞쪽 몇 waypoint 중 직선 보행 가능한 첫 지점에 합류하게 한다. H4의 거부 miss가 묶음 cache 후 확장의 55~82%이므로 상한 효과가 가장 크다(H1에 따라 완료 tick도 비례해 준다). key·자료 구조는 그대로인 기존 cache의 결함 수정이라 기준선 몫으로 본다. 결정론·경로 cost 비율(채택 기준 3과 같은 1.05 이내)을 함께 검증해야 한다.
2. **거리 기반 Chase 우선순위.** CPU를 늘리지 않고 묶음 P50을 S3에서 약 절반으로 줄인다. P95는 12~22%(S2는 0~17%)만 준다. 가까운 개체(교전에 영향이 큰 쪽)가 먼저 움직이는 체감 이득이 있다.
3. **몰림 시 예산 적응.** H1이 지지되므로 예산을 올린 만큼 완료 tick이 준다. 그러나 S3에서는 묶음 tick이 전체 tick의 약 15%라 경로 tick P95 Gate(1.5ms, 현재 1.133ms)를 넘길 위험이 있다. Gate는 바꾸지 않으므로 S2 규모에서만 여지가 있다. 후순위다.
4. 보류: **재요청 시차 분산.** 처리량 한계(H1)에서는 묶음 확장 총량과 완료 시각이 그대로이고, 제출 시각만 미뤄 큐 대기 숫자를 줄일 뿐이다. **목표 이동 시 기존 경로 재사용**은 합성 목표가 반대편으로 바뀌어 이 데이터로 평가할 수 없고, S1에는 Chase 요청이 없었다.

- **같은 목표 공유는 단위 3(flow field)으로 넘긴다.** H5가 지지되며 묶음 비용의 주성분이다. 1번은 같은 Tile 쌍 안의 합류만 고치며, 시작 Tile이 다른 같은 목표 요청의 반복 탐색은 flow field 비교의 몫으로 남긴다.
- 단위 2의 비교 입력은 이번 `Saved/Profiling/Phase10/Unit1` 캡처 3종이다. 재생기의 burst 완료 tick 지표(채택 기준 2)로 개선 전후를 비교한다.

### 검증

- `LootNPopEditor` 빌드 성공. 전체 `LootNPop.SurfaceNavigation` 자동화 129/129 Success(`Saved/Logs/Phase10_Unit1_AllTests.log`). 테스트가 덮어쓴 에셋 세 개는 `Saved/Tests/Phase10_Unit1_TestAssetBackup`에서 복구했다(Content diff 없음).
- BuildCookRun 성공(`Saved/Logs/Phase10_Unit1_Package.log`). 세 캡처 모두 호스트·게스트 exit 0이고, Gate 스크립트의 기능 검사를 통과했다. 재생 3종 Success.

## 2026-10-13 — 단위 2(A* 기준선 개선)

### 변경

- **cache 앞쪽 합류**(`FLNPNavPathScheduler::FindCachedPath`): 공유 경로의 앞쪽 `CacheJoinWaypoints`(4)개 waypoint 중 시작 node에서 직선 보행 가능한 첫 waypoint에 합류한다. 결과 `FLNPNavPathResult::FirstWaypoint`로 넘기고 `LNPEnemyPathProcessor`가 그 index부터 따른다. key는 그대로다.
- **합류 거리 비 가드**(`CacheJoinMaxLeadRatio` r = 0.1): 합류점까지 직선 거리 a ≤ r × 합류점 이후 공유 경로 꺾은선 길이 s. 합류점은 원래 요청의 최단 경로 위에 있으므로 최적 길이 ≥ s − a, 합류 경로는 a + s다. 요청별 길이 비율이 (1+r)/(1−r) ≈ 1.22 이하로 묶인다. 첫 waypoint 합류에도 적용한다(D-079).
- **Chase 거리 순서**(`bChaseNearestFirst`): Chase 대기열을 (제출 tick, 시작·목표 직선 거리 제곱, serial) 최소 heap으로 바꿨다. 앞 tick 요청이 늘 먼저라 굶주림이 없다. Replan·Background는 먼저 온 순서 그대로다.
- **재생기**: 변형을 표(`FVariant`)로 바꾸고 단위 2 변형을 추가했다. 요청 결과에 `firstWaypoint`·`walk` 열을 붙였다(뒤에 추가해 단위 1 분석 스크립트와 호환). `improvedSerial`이 `improved`와 요청별(상태·waypoint·합류점·확장·cost·doneTick·cache 여부)로 같아야 한다.
- **보행 길이 지표 `walk`**: 처음에는 "시작 → 합류 waypoint부터 꺾은선 → 목표"로 쟀으나, 합류점으로 되돌아가는 길이를 그대로 세어 기존 cache256도 S1 비율 P95가 3.4로 나왔다. 경로 추종의 지름길을 흉내 내도록 바꿨다. 도착한 node에서 목표가 곧게 보이면 목표로, 아니면 곧게 보이는 가장 먼 연속 waypoint로 간다(걷는 도중의 지름길은 세지 않는다).
- 비교표 스크립트 `Scripts/Profiling/ComparePhase10Replay.py <재생 폴더>`(표준 라이브러리만). burst는 묶음 ≥400 요청의 doneTick, 비율은 같은 행 base 대비 walk(둘 다 Succeeded인 행).
- 회귀(`Nav.PathScheduler`): 7) 고정 입력의 시작 Tile은 모두 트여 있어(첫 waypoint가 안 보이는 node 0개) 시작 Tile에 Pod를 세워 가림막을 만든다. Tree에서 Pod 921cm, waypoint 2/3 합류를 확인하고, 첫 waypoint만 보는 설정과 아주 작은 r은 거부함을 확인한다. 8) 같은 tick의 가까운 Chase가 먼저 시작하고, 끄면 먼저 온 순서, 앞 tick 요청은 거리와 무관하게 먼저다.

### 재생 비교(`Saved/Profiling/Phase10/Unit1` 캡처 3종, sidecar seed 1768)

| 입력 / 변형 | 확장 합 | cache hit(앞쪽 합류) | burst 완료 tick P50/P95/최대 | 보행 길이 비율 P50/P95/최대 | 비율 P95(hit) |
|:---|---:|:---|:---|:---|---:|
| S1 cache256(기준) | 5,933 | 243(0) | — | 1.000/1.652/4.536 | 1.663 |
| S1 join4 | 5,834 | 249(6) | — | 1.000/1.657/16.391 | 1.668 |
| S1 **improved** | 9,746 | 166(1) | — | 1.000/1.000/1.000 | 1.000 |
| S2 base(cache 없음) | 1,321,183 | 0 | 13/27/31 | 1 | — |
| S2 cache256(기준) | 561,898 | 4,641(0) | 6/12/15 | 1.000/1.169/3.720 | 1.289 |
| S2 join4 | 260,835 | 6,551(3,061) | 3/6/11 | 1.000/1.423/6.193 | 1.483 |
| S2 cache256Lead0.1 | 846,444 | 2,319(0) | 8/16/20 | 1.000/1.002/1.274 | 1.057 |
| S2 join4Lead0.05 / 0.1 / 0.2 | 925,773 / 649,095 / 404,794 | 1,901 / 3,505 / 5,035 | 9/17/20 · 6/11/15 · 4/8/13 | P95 1.000 / 1.021 / 1.090 | 1.016 / 1.061 / 1.137 |
| S2 nearest | 453,113 | 5,203(0) | 3/7/12 | 1.000/1.175/3.720 | 1.296 |
| S2 **improved** | 582,172 | 3,898(873) | **4/8/14** | 1.000/**1.013**/1.335 | 1.042 |
| S2 improvedBudget8000 | 575,468 | 3,915(876) | 2/4/7 | 1.000/1.014/1.335 | 1.042 |
| S3 base(cache 없음) | 5,142,520 | 0 | 42/126/183 | 1 | — |
| S3 cache256(기준) | 2,379,220 | 12,136(0) | 19/63/92 | 1.000/1.227/4.537 | 1.376 |
| S3 join4 | 1,281,875 | 17,463(8,328) | 10/48/69 | 1.000/1.600/7.267 | 1.709 |
| S3 cache256Lead0.1 | 3,408,368 | 5,992(0) | 26/88/126 | 1.000/1.005/1.324 | 1.077 |
| S3 join4Lead0.05 / 0.1 / 0.2 | 3,701,114 / 2,824,485 / 1,961,698 | 5,046 / 8,405 / 12,128 | 27/95/136 · 19/77/109 · 13/59/85 | P95 1.000 / 1.031 / 1.130 | 1.029 / 1.093 / 1.204 |
| S3 nearest | 2,157,744 | 13,265(0) | 8/34/79 | 1.000/1.208/7.570 | 1.305 |
| S3 **improved** | 2,617,091 | 9,565(1,887) | **10/43/93** | 1.000/**1.024**/1.334 | 1.065 |
| S3 improvedBudget8000 | 2,635,190 | 9,515(1,877) | 5/22/46 | 1.000/1.024/1.334 | 1.066 |

- improved = 앞쪽 4 waypoint 합류 + r 0.1 + Chase 거리 순서(현재 runtime 기본값). 세 입력 모두 `Result={Success}`, 모든 변형의 결정론 대조 불일치 0, 예산 차감 상한 통과.
- **기존 cache는 기준 3(경로 cost 비율 P95 ≤ 1.05)을 이미 넘고 있었다.** cache256의 보행 길이 비율 P95가 S1 1.65·S2 1.17·S3 1.23이다. 같은 (StartTile, GoalTile)이라도 32m Tile 안 시작점 차이로 공유 경로가 크게 돌아간다(S1 짧은 배회 경로에서 최대 4.5배). 가드 없는 앞쪽 합류는 이것을 키운다(P95 1.42~1.66, 최대 16배: 목표 3m 앞 개체가 46m 돌아가는 공유 경로의 waypoint 1에 합류).
- r 선택: 0.05는 비율이 거의 1이지만 cache 이득을 대부분 잃는다(S3 확장이 cache256보다 +56%). 0.2는 S2·S3 P95가 1.09·1.13으로 기준 3을 넘는다. **0.1**이 세 입력 모두 전체 P95 ≤ 1.031을 지키는 가장 느슨한 값이다. hit만 보면 S3 P95가 1.065로 1.05를 조금 넘는다.
- 개선 효과(cache256 대비 improved): burst 완료 tick P95 S2 12→8(−33%)·S3 63→43(−32%), P50 S3 19→10. 확장 합은 가드 때문에 S2 +4%·S3 +10%·S1 +64%(S1은 절대량 9.7k로 작다). 효과의 대부분은 거리 순서다. 앞쪽 합류는 가드가 버리는 hit를 일부 되찾는다(같은 r에서 첫 waypoint만 보는 cache256Lead0.1 대비 join4Lead0.1의 S3 확장 −17%, burst P95 88→77).
- 거리 순서는 단위 1 모형(P50 약 절반, P95 12~22% 감소)보다 크게 듣는다(cache256 → nearest: S3 P95 63→34). 가까운 요청이 먼저 끝나 같은 Tile 쌍 cache를 채우므로 hit도 늘기 때문이다(12,136→13,265).
- **몰림 시 예산 적응은 구현하지 않는다.** 예산 8000 상한 변형은 burst P95를 S2 8→4·S3 43→22로 반으로 줄이지만, 재생 tick P95가 S3 887→1,452us(+64%)로 늘었다. 실제 S3 경로 tick P95(아래 1.387ms)에 같은 배수를 걸면 Gate 1.5ms를 넘는다. S2만 여지가 있으나, 몰림 판정 임계와 tick 시간 상한을 새로 정해야 하는 데 비해 단위 3(같은 목표 공유)이 같은 총작업을 직접 줄인다.
- 원본: `Saved/Profiling/Phase10/Unit2/Replay/*_RequestCosts{,_Requests}.csv`, 로그 `Saved/Logs/Phase10_Unit2_Replay_*.log`.

### 패키지 Gate와 리슨 2P(새 패키지, seed 1, 30초)

| 시나리오 | frame P50/P95(ms) | 경로 tick P95(ms) | 요청 | 큐 대기 P50/P95/최대(ms) | 종료 상태 |
|:---|:---|---:|---:|:---|:---|
| S1 Production 1 | 7.06/7.64 | 0.000 | 242 | — | Gate PASS |
| S1 Production 2 | 7.20/8.58 | 0.014 | 276 | — | Gate PASS |
| S2 N700Chase(slot 5) | 9.58/10.50 | 0.254 | 7,764 | 6.7/66.9/98.1 | Succeeded 7,430·Unreachable 319·Cancelled 15 |
| S3 N2000Chase(slot 5) | 19.34/22.51(FAIL) | 1.387 | 21,246 | 70.8/961.5/1,858.4 | Succeeded 20,267·Unreachable 810·Cancelled 858 |

- **Production은 V011 기준선(frame P95 7.65~9.28ms) 안이고 경로 tick Gate를 통과한다.** 네 실행 모두 호스트·게스트 exit 0, probe 3종 PASS, UnknownHits·EnvelopeEscapes 0, assert·ensure 0이다(리슨 2P 스모크). `exact ≤2ms` FAIL은 Gate가 아니다.
- S2·S3 frame은 단위 1보다 높지만 단위 0 캡처와 같은 수준이다(S2 10.43·S3 21.40). 이번 변경과 무관한 단계도 같이 올랐다(consumer P50 S2 1.00→1.19·S3 3.01→3.76, slots S2 0.48→0.65). 단위 0→1에서도 같은 runtime으로 S2 frame P95가 10.43→8.28로 바뀐 기계 편차다.
- 실제 큐 대기: S2 P95 82→67ms(단위 0 93ms). S3는 742→962ms로 줄지 않았다. S3는 대기열이 비지 않는 포화 상태(queued P95 839)라, 앞 tick 우선 규칙 아래 거리 순서의 이득이 작고 가드로 늘어난 확장(tick 확장 P50 549→1,367)이 상쇄한다. 프레임 수로 환산하면 약 48→50프레임이다. S3는 D-076 경계를 넘는 몰림 재현용이며 Gate가 아니다.
- 원본: `Saved/Profiling/Phase10/Unit2/{Gate,Capture}/*`, 패키지 로그 `Saved/Logs/Phase10_Unit2_Package.log`.

### 검증

- `LootNPopEditor` 빌드 성공. 전체 `LootNPop.SurfaceNavigation` 자동화 129/129 Success(`Saved/Logs/Phase10_Unit2_AllTests.log`). 테스트가 덮어쓴 에셋 세 개는 `Saved/Tests/Phase10_Unit2_TestAssetBackup`에서 복구했다(Content diff 없음).
- BuildCookRun 성공. 위 Gate·캡처.

### 판정과 단위 3으로 넘기는 것

- 단위 3 이후의 A* 비교 기준선은 **improved**다(`Nav.RequestCostReplay`의 `improved` 변형 = runtime 기본값). 기준값(Unit1 캡처 재생): burst 완료 tick P95 S2 8·S3 43, 확장 합 S1 9,746·S2 582,172·S3 2,617,091, 보행 길이 비율 P95 S1 1.000·S2 1.013·S3 1.024.
- 남은 묶음 비용은 같은 목표의 반복 탐색(H5)이다. 가드가 Tile 쌍 공유를 줄인 만큼 flow field의 상대 이득은 단위 1 추정보다 커질 수 있다.

## 2026-10-13 — 단위 3(목표별 flow field 프로토타입)

### 변경(runtime 동작 없음)

- `LNPNavFlowField.{h,cpp}`(순수 함수, processor·scheduler 미연결)
  - `BeginBuild`·`StepBuild`: goal node에서 Dijkstra로 펴는 다중 프레임 빌드. 예산 단위는 확정(갱신) node 수이며 A* 확장 수와 같은 단위로 센다. 확정 순서는 (거리, node index)라 예산 분할과 무관하다. 경로 cost `MaxCost`(반경)를 넘는 node는 확정하지 않는다.
  - 저장은 overlay Tile key(`GetTileKey`)별 블록(256 cell × 거리 float + 목표 쪽 다음 node int32 + 확정 bit, 2,080B)이다. 전역 node 수 크기 배열을 쓰지 않는다.
  - `GetSettledDistance`·`ExtractNodePath`: 확정된 node에서 다음 node를 따라 goal까지의 node 열. 짓는 중에도 확정된 node는 최종값이라 바로 답할 수 있다.
  - snapshot generation·connectivity version·overlay revision이 시작 때와 다르면 Invalid다. overlay 차단 node는 확정하지 않는다.
- `Nav.RequestCostReplay` flow field 변형(`FFlowFieldReplay`, 테스트 코드)
  - Chase는 묶음 안 직선 거리 오름차순(improved와 같은 순서)으로 스냅하고, 목표 node의 field에 대기시킨다. field가 없으면 만들고, 대기자가 있을 때만 이어 짓는다(lazy). 시작 node가 확정되면 답한다.
  - 다음 경우는 improved A* scheduler로 폴백한다. 폴백은 scheduler가 다시 스냅하므로 시작 비용을 한 번 더 낸다. Chase가 아닌 요청은 처음부터 scheduler다.
    - field를 다 지었는데 시작 node가 확정되지 않았다(반경 밖).
    - 시작·목표 node 직선 거리가 반경보다 크다(경로 cost의 하한이라 기다릴 필요가 없다).
    - 스냅이 Reachable이 아니다.
  - 프레임 예산 4,000 = 스냅 × 16 + field 확정 수 + 답한 경로 node 수 + A* 차감. 짓는 field가 있으면 스냅은 예산의 절반까지만 쓴다. field 확정은 scheduler처럼 균등 몫으로 병렬 진행한다. A*는 남은 예산으로 같은 tick에 돈다.
  - 답한 경로는 scheduler와 같은 조립(`SimplifyPath`·통과 Tile fingerprint)을 해서 tick 시간을 같은 기준으로 잰다. 조립 비용을 경로 node당 1로 뺀다(실측 node당 약 0.14us라 보수적 회계). 세지 않는 쪽은 `FreeAnswer` 변형이다.
  - field 수명: 기본은 burst(묶음 ≥400 = 목표 전환)마다 모두 버린다. 실제 플레이의 목표 전환은 플레이어가 Tile을 넘어 목표 node가 바뀐 것이기 때문이다. `Keep`은 목표가 멈춰 있을 때의 재사용 상한이다.
  - 요청 결과 CSV 뒤에 `source` 열(0 scheduler, 1 field, 2 폴백)을 붙였다. 변형별 요약 `<출력>_FlowFields.csv`(field 수·확정 합·폴백·응답 node·최대 메모리)를 함께 쓴다. `ComparePhase10Replay.py`는 tick 합(ms)·field/폴백 열과 field 요약 표를 낸다.
  - 결정론: `flowfieldSerial`이 `flowfield`와 요청별로 같아야 한다. Chase가 없는 입력(S1)에서는 field 변형 모두가 improved와 같아야 한다.
- `Nav.FlowField` 자동화(신규)
  - 대칭성(아래 표)
  - field 거리 = A* 최적 cost, 다음 node 열이 이웃 edge로 이어진 그 cost의 경로, 반경 밖 시작은 A* 최적 cost가 반경보다 크고 A* 폴백이 푼다.
  - 예산 1·7·333·4,000으로 나눠 지은 field가 한 번에 지은 field와 바이트 단위로 같다. 짓는 중 확정된 node의 경로는 완성 field의 경로와 같다.
  - overlay 차단 node 제외, 차단 goal Invalid, 짓는 중 revision 변경 Invalid.

### portal·seam link 역방향 대칭성

field는 goal에서 정방향 이웃 목록(`ForEachNeighbor`)을 역방향 edge로 쓰며 편다. 비용은 개체가 걷는 방향(이웃 → 확정 node)으로 재므로, 전제는 **edge가 양방향으로 있음**뿐이다. 모든 정방향 edge에 대해 역방향 edge 존재와 두 방향 비용의 비트 단위 일치를 검사했다.

| snapshot | grid edge | seam link·portal edge | seam link | 역방향 없음(grid/extra) | 비용 불일치 |
|:---|---:|---:|---:|:---|---:|
| 회귀 fixture | 3,474,304 | 9,912 | 4,428 | 0/0 | 0 |
| production seed 135792468 | 5,816,564 | 28,644 | 4,281 | 0/0 | 0 |
| production seed 1768(재생 입력) | 6,059,918 | 25,848 | 4,281 | 0/0 | 0 |

- 구성상으로도 대칭이다. grid `EdgeMask`는 베이커가 양쪽에 함께 세우고, seam link는 양방향으로 넣고, D-060 막힌 이음매 edge는 양방향으로 막는다. portal은 베이커가 늘 `Bidirectional`로 굽는다.
- `ComputeEdgeCost`는 `Delta` 부호만 바뀌고 Up이 `From + To`로 정해져 수식상 대칭이며, 실측도 비트 단위로 같았다.
- **역방향 edge 비용 처리 결정: 별도 처리하지 않는다.** 보행 방향으로 재므로 비용이 비대칭이어도 결과가 맞고, edge 대칭성은 `Nav.FlowField`가 세 snapshot에서 계속 검사한다. codec은 단방향 portal(`Bidirectional` 없는 Flags)을 허용한다. 베이커가 그런 portal을 만들기 시작하면 field 빌드에 역방향 link 표가 필요하며, 검사가 실패로 알린다.

### 단위 비용(`Nav.FlowField`, seed 1768, 직선 20~80m 쌍 40개)

- 반경 12,000 field: field당 확정 21,596 node, **확정당 0.403us** vs A* 확장당 0.452us(요청당 686 확장). 같은 예산 단위로 세도 된다.
- field 최대 586KiB(블록 2,080B × Tile). 40쌍 중 39개가 반경 안이며, 밖의 1개는 A* 최적 cost가 반경보다 컸다(폴백 성립).
- 재생 목표 4점 주변은 섬 가장자리라 field당 최대 확정이 5,017(반경 12,000)로 위보다 훨씬 작다.

### 파라미터 스윕(Unit1 캡처 3종, sidecar seed 1768)

입력의 Chase 목표는 S2·S3 모두 고정점 4개이며 burst마다 4개가 섞여 들어온다(목표당 약 120~390건, 시작–목표 직선 거리 P95 51~85m, 최대 123m). S1의 355건은 모두 Background라 field가 쓰이지 않는다(모든 field 변형이 improved와 요청별로 같음, 불일치 0).

| 입력 / 변형 | tick 합(ms) | tick P95(us) | 확장+확정 합 | field 응답 / 폴백(반경 밖·스냅) | burst 완료 tick P50/P95/최대 | 보행 비율 P95/최대 | field 최대 메모리 |
|:---|---:|---:|---:|:---|:---|:---|---:|
| S2 improved | 213.9 | 259 | 582,172 | — | 4/8/14 | 1.013/1.335 | — |
| S2 R6000 | 166.4 | 235 | 365,775 | 5,004 / 2,581·309 | 5/9/14 | 1.000/1.220 | 0.19MiB |
| S2 R9000 | 138.8 | 176 | 203,263 | 7,382 / 203·309 | 5/**7**/13 | 1.000/1.220 | 0.24MiB |
| S2 **R12000(flowfield)** | **132.6** | 69 | 217,816 | 7,478 / 107·309 | 5/**9**/13 | 1.000/1.107 | 0.39MiB |
| S2 R15000 | 138.0 | 61 | 246,440 | 7,551 / 34·309 | 5/13/15 | 1.000/1.009 | 0.61MiB |
| S2 R18000 | 151.0 | 73 | 272,408 | 7,581 / 4·309 | 5/13/16 | 1.000/1.000 | 0.90MiB |
| S2 R24000 | 164.4 | 68 | 316,207 | 7,585 / 0·309 | 5/13/20 | 1.000/1.000 | 1.04MiB |
| S2 R12000 Keep | 112.8 | 55 | 121,846 | 7,478 / 107·309 | 4/5/9 | 1.000/1.107 | 0.39MiB |
| S2 R12000 FreeAnswer | 140.8 | 70 | 217,816 | 7,478 / 107·309 | 3/7/11 | 1.000/1.107 | 0.39MiB |
| S3 improved | 755.7 | 895 | 2,617,091 | — | 10/43/93 | 1.024/1.334 | — |
| S3 R6000 | 679.1 | 892 | 2,014,399 | 12,170 / 8,577·781 | 12/69/95 | 1.004/1.305 | 0.18MiB |
| S3 R9000 | 557.3 | 901 | 1,365,878 | 18,752 / 1,995·781 | 9/35/86 | 1.000/1.305 | 0.23MiB |
| S3 **R12000(flowfield)** | **466.4** | 893 | 962,761 | 19,672 / 1,075·781 | 9/**23**/65 | 1.000/1.243 | 0.39MiB |
| S3 R15000 | 375.9 | 906 | 582,519 | 20,483 / 264·781 | 9/22/47 | 1.000/1.190 | 0.60MiB |
| S3 R18000 | 338.3 | 873 | 411,815 | 20,716 / 31·781 | 9/26/34 | 1.000/1.000 | 0.97MiB |
| S3 R24000 | 337.6 | 866 | 429,508 | 20,747 / 0·781 | 9/28/33 | 1.000/1.004 | 1.21MiB |
| S3 R12000 Keep | 461.0 | 941 | 903,418 | 19,672 / 1,075·781 | 10/17/65 | 1.000/1.243 | 0.38MiB |
| S3 R12000 FreeAnswer | 478.0 | 1,019 | 961,512 | 19,672 / 1,075·781 | 6/19/55 | 1.000/1.243 | 0.39MiB |

- 스냅 수는 S2 7,894·S3 21,528(요청마다 한 번)이고, 응답 경로 node 합은 R12000에서 S2 145,407·S3 397,671이다. 확장+확정 합에는 둘 다 들어 있지 않다(improved도 스냅을 확장 합에 넣지 않는다).
- **burst 꼬리는 A* 폴백이다.** R12000 burst 요청을 출처별로 나누면 field 응답은 P95 S2 8·S3 16이고, 폴백은 S2 11(263건)·S3 49(1,138건)다. 폴백은 field 작업이 끝난 뒤 남은 예산을 받으므로 늦게 시작한다. S3 burst별 P95는 improved 74·42·32·24·19·16 → flowfield 44·24·21·18·18·18이다.
- **반경 trade-off.** 반경을 넓히면 폴백이 줄어 S3 CPU가 계속 준다(R18000 tick 합 −55%). 그러나 대기자가 멀수록 field를 끝까지 지어야 답하므로 묶음 전체가 그 field 작업 뒤로 밀린다. S2는 R15000 이상에서 burst P95가 13으로 나빠지고, S3도 R18000 이상에서 P95가 26~28로 되돌아간다(최대는 계속 준다). 작은 반경(R6000·R9000)은 S3 폴백이 2천~9천 건으로 늘어 P95가 35~69다.
- **채택 설정 R12000.** S2 burst P95를 거의 유지(8→9)하면서 S3를 43→23(−47%)으로 줄이는 유일한 반경이다. R9000은 S2가 가장 좋지만(7) S3가 35이고, R15000은 S3 22지만 S2가 13이다.
- 직선 거리 조기 폴백은 효과가 거의 없었다. 넣기 전 1차 스윕과 R12000 수치가 같고, R6000만 확장 합이 0.4% 줄었다. 경로 cost가 반경을 넘는 요청도 직선 거리는 대부분 반경 안이다.
- 목표 양자화는 스윕하지 않았다. 입력 목표가 고정점 4개라 모든 요청이 같은 goal node를 가지므로 양자화가 결과를 바꿀 수 없다. 실제 플레이의 목표 이동은 burst마다 field를 버리는 수명 규칙으로 대신 모사했다.
- field 수명: 기본값(burst마다 버림)과 `Keep`(목표 정지 재사용 상한)의 차이는 burst P95 S2 9→5, S3 23→17이다. 목표가 움직이는 실제 추격에서는 기본값 쪽이 맞다.
- 응답 회계: `FreeAnswer`(A*처럼 경로 조립을 예산에서 빼지 않음)는 burst P95 S2 7·S3 19다. 기본 회계(node당 1)와 이 값 사이가 응답 비용 가정에 따른 범위다. tick P95는 기본 회계 쪽이 낮다(S3 893 vs 1,019us). 조립이 한 tick에 몰리지 않게 막기 때문이다.
- tick 최대는 두 변형 모두 스냅 단계의 일회성 이상치(접근점 탐색, 한 tick 시작 1.6~5.4ms)가 정한다. field 고유 비용이 아니다.
- 원본: `Saved/Profiling/Phase10/Unit3/Replay/*`(요청·tick·FlowFields CSV), 비교표 `Saved/Profiling/Phase10/Unit3/Compare.md`, 로그 `Saved/Logs/Phase10_Unit3_Replay_*.log`. 반경 15,000을 주 변형으로 둔 같은 스윕은 `Unit3/Sweep_R15Main`이다.

### 채택 기준 예비 판정(improved 대비, R12000·burst마다 field 버림)

판정은 단위 5가 소유한다. 아래는 같은 재생 입력의 예비 수치다.

| 기준 | S1 | S2 | S3 | 예비 판정 |
|:---|:---|:---|:---|:---|
| 1. 경로 CPU 총합 −30% 이상, tick P95 악화 없음 | 변화 없음(Chase 없음) | tick 합 213.9→132.6ms(**−38%**), 확장+확정 −63%, tick P95 259→69us | tick 합 755.7→466.4ms(**−38%**), 확장+확정 −63%, worker 합 1,047→417ms, tick P95 895→893us | **충족**(S2·S3) |
| 2. burst 완료 tick P95 −50% 이상 | 해당 없음 | 8→9(**+12%**) | 43→23(**−47%**) | **미충족**(S3 근접, S2 악화) |
| 3. 경로 cost 비율 P95 ≤ 1.05 | 1.000 | 1.000(최대 1.107) | 1.000(최대 1.243) | **충족** |
| 4. 추가 메모리 ≤ 8MiB | 0 | field 최대 0.39MiB | field 최대 0.39MiB | **충족**(A* scratch 4개 48.6MiB와 별개) |
| 5. overlay 변경 시 국소 재계산 | — | — | — | 설계 메모(아래), 미측정 |

- 기준 2를 막는 것은 폴백 꼬리(S3)와 요청당 고정비(스냅 16 + 응답 경로 node, S2)다. S2의 improved는 cache·앞쪽 합류로 이미 싸서, field의 요청당 고정비가 상대적으로 크다. 같은 반경에서 `Keep`(S3 −60%)·`FreeAnswer`(S3 −56%)는 기준을 넘지만, 각각 목표 정지와 조립 무료라는 가정에 기댄다.
- 비율 최대 1.107·1.243은 폴백 A*(improved cache 합류)의 값이다. field 응답은 정의상 최적 경로(보행 비율 1.000)다.

### 기준 5(overlay 국소 재계산) 설계 메모

- field 블록 key가 overlay Tile key와 같다. overlay가 바뀐 Tile 중 field 블록이 있는 것만 영향을 받으므로, 무관한 Pod 변경은 field를 건드리지 않는다(블록 map 조회 한 번).
- 프로토타입은 짓는 중 overlay revision이 바뀌면 Invalid로 끝내고, 다 지은 field에는 재검사가 없다. 통합하려면 다음 중 하나가 필요하다.
  - 차단(Pod 생성): 막힌 node의 다음 node 트리 하위(그 node를 거쳐 goal로 가던 node)만 확정을 풀고, 그 경계의 확정 이웃에서 Dijkstra를 이어 편다. 재계산이 하위 트리 크기에 비례하는 국소 갱신이다.
  - 해제(Pod 소멸): 풀린 node를 이웃 최소값으로 다시 열고 거리가 줄어드는 쪽으로만 전파한다(증분 Dijkstra).
  - 가장 단순한 대안은 영향받은 field를 통째로 다시 짓는 것이다. 반경 12,000 field는 최대 5천~2만 확정(예산 4,000 기준 2~6 tick)이라 비용은 작지만, "전체 재구축 없음"에는 맞지 않는다.
- 단위 5에서 overlay 토글 재생으로 하위 트리 크기를 재야 기준 5를 판정할 수 있다.

### 검증

- `LootNPopEditor` 빌드 성공. 전체 `LootNPop.SurfaceNavigation` 자동화 130/130 Success(`Saved/Logs/Phase10_Unit3_AllTests.log`, 신규 `Nav.FlowField` 포함). 테스트가 덮어쓴 에셋 세 개는 `Saved/Tests/Phase10_Unit3_TestAssetBackup`에 보관하고 git으로 되돌렸다(Content diff 없음).
- 재생 S1~S3 `Result={Success}`, 모든 변형의 결정론 대조 불일치 0, 예산 차감 상한 통과.
- runtime(processor·scheduler)은 바꾸지 않았으므로 패키지 Gate는 돌리지 않았다.
