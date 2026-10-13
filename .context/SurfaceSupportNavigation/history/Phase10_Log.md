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
