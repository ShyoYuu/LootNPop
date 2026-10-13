# Phase 10 — 대규모 추격 경로 비교와 목표 전환 큐 대기 개선

> 상태: 진행 중(단위 0·1·2·3 완료 2026-10-13, 다음 단위 4)
> 착수: 2026-10-13
> 선행: Phase 9 완료(V011 성능 기준선), Phase 7 일반 A*·요청 재생 harness
> 순서: 단위 0 → 1 → 2 → 3 → 4 → 5 → (6). 단위마다 검증한 뒤 사용자 진행 확인을 받는다.

## 배경

D-044에 따라 같은 benchmark harness에서 목표별 flow field와 계층형 A*의 최소 프로토타입을 일반 A*와 비교하고, 채택 기준을 통과한 방식만 production에 통합한다. 착수 확인에서 다음을 확인했다.

- **production에서 A*는 병목이 아니다.** V011 경로 tick P95는 0.014~0.021ms(Gate 1.5ms)다. V011B 캡처 436건의 큐 대기 P95는 6.4ms로, 다음 프레임에 시작되는 수준이다. 활동 대역(D-067) 때문에 활성 개체가 약 435기다.
- **전원 활성 다대소 몰림에서는 큐 대기가 크다.** Phase 7 합성 추격 700에서는 5초마다 약 550건이 동시에 제출됐다. 큐 대기 P95는 약 125ms, 실행 지연 P95는 약 0.35ms였다(`history/Phase07_Log.md` 2026-10-02). 처리량으로 대략 계산하면 550 × (평균 확장 약 134 + 시작 비용 16) ÷ 4,000 ≈ 20프레임이고, 관측 최대 약 220ms와 맞는다. 이것이 1순위 가설이며 아직 검증 전이다.
- **실제 플레이에도 몰림 원인이 있다.** `LNPEnemyPathProcessor`의 `bGoalChanged`는 플레이어가 Tile 경계(16셀 × 200cm)를 넘거나 400cm 넘게 이동하면 참이 된다. 그러면 그 플레이어를 쫓던 모든 개체가 같은 프레임에 재요청한다.
- **기존 캡처는 재생할 수 없다.** Phase 7 CSV는 Meadow 단독 풀에서 캡처했다. 재생기는 배치 seed `135792468`로 snapshot을 만들었고 generation·version만 확인했으므로, 다른 배치의 좌표도 통과시켰다.

## 사용자 결정(2026-10-13)

- 비교 자료와 목표 전환 큐 대기의 원인 분석·개선을 모두 한다. 원인 분석과 기준선 개선을 먼저 한다.
  - 기준선이 고칠 수 있는 것을 고치지 않은 A*이면 flow field의 이득이 부풀려진다.
  - 분석 결과가 "같은 목표 공유"를 가리키면, 그 개선은 flow field 단위로 넘긴다.
- 시나리오는 현재 혼합 풀(seed 1, 리슨 2P, 30초)의 세 가지다.
  - S1: production 자연(`Production`)
  - S2: 지상 700 합성 추격(`N700Chase`)
  - S3: 지상 약 2,000 합성 추격(`N2000Chase`, D-076 동시 활성 상한 근처). 배치가 모자라면 실제 배치 수를 S3로 기록한다.
- 프로토타입은 flow field를 먼저, 계층형 A*를 그다음에 만든다.
- 채택 기준(모두 만족해야 production 통합). 같은 재생 입력에서 개선된 A* 기준선과 비교한다.
  1. 경로 CPU 총합 30% 이상 감소, tick P95 악화 없음
  2. burst 묶음의 완료 tick 수 P95 50% 이상 감소
  3. 경로 cost 비율 P95 ≤ 1.05
  4. 추가 메모리 ≤ 8MiB
  5. Pod overlay 변경 시 재계산이 국소적(전체 재구축 없음)

## 단위

### 단위 0 — harness 재정비와 3종 캡처

- [x] 캡처 sidecar `NavRequests_*_Meta.txt`: `octantGenSeed`, `generation`, `graphVersion`, `slotN=<SurfaceData 경로>`(`ULNPNavPathSubsystem::EndRequestCapture`). CSV 스키마는 바꾸지 않는다.
- [x] `Nav.RequestCostReplay`: sidecar가 필수이며, 그 seed로 snapshot을 만든다. slot별 SurfaceData가 다르면 실패한다.
- [x] 요청 단위 결과 `<output>_Requests.csv`: `variant,row,batch,batchSize,status,doneTick,expansions,runningTicks,fromCache,cost`. `doneTick`은 묶음 제출 뒤 끝난 tick(1부터)이며 재생에서 큐 지연을 대신한다.
- [x] 캡처 실행: 별도 스크립트를 만들지 않고 `Scripts/Profiling/RunPhase13Gate.ps1`에 `N2000Chase` 케이스와 sidecar 보존을 추가했다. 출력은 `Saved/Profiling/Phase10/Capture`. 합성 추격은 `-IslandSlot 5`(seed 1 배치의 Meadow slot)로 실행한다. harness의 slot 4 상수 가정을 `-LNPLoadBaselineIslandSlot=`로 바꿨다(경위는 로그).
- [x] 검증: 에디터 빌드, 전체 `LootNPop.SurfaceNavigation` 자동화, 패키지, S1~S3 캡처와 재생 성공을 확인한다. 다른 seed의 sidecar로는 재생이 실패해야 한다.

계획에서 뺀 것: "seed 1 snapshot 위 시작·목표 스냅 표본 검사". slot별 SurfaceData 경로를 대조하는 것으로 배치 일치를 직접 확인하므로 중복이다.

### 단위 1 — 목표 전환 큐 대기 원인 분석(계측만, 동작 변경 없음) — 완료 2026-10-13

- [x] scheduler tick 기록(캡처 중에만): 대기·실행 수, 시작 수(즉시 종료·cache hit), 확장·시작 비용 차감, 종료 수, 정지 사유(예산/scratch)와 scratch에 막힌 순간의 잔여 예산. `<캡처>_Ticks.csv`
- [x] 요청 사유 비트(`_Reasons.csv`)와 활동 대역 깨움(`_Wakes.csv`). 요청 CSV 스키마는 그대로다. Gate 스크립트가 세 파일을 보존한다.
- [x] S1~S3 새 패키지 캡처·재생과 분석(`Scripts/Profiling/AnalyzePhase10QueueWait.py`)
- [x] 가설 판정(근거 수치는 `history/Phase10_Log.md` 2026-10-13 단위 1)
  - H1 예산 처리량 한계: **지지**. 큰 묶음 완료 tick = 확장 총량 ÷ 4,000과 일치하고 예산 사용률 100%다.
  - H2 scratch 4개 상한: **기각**. 정지 사유의 81~85%가 예산이고, scratch에 막혀도 예산은 실행 중 요청이 다 쓴다.
  - H3 FIFO 순서: **지지(크기 제한)**. 거리순이면 P50이 S3에서 약 절반, P95는 12~22% 감소, 묶음 완료 시각은 그대로다.
  - H4 cache 적중 붕괴: **기각**. 묶음 적중률이 평시와 같다. 대신 cache 합류 검증 거부가 묶음 cache 후 확장의 55~82%다.
  - H5 같은 목표 중복 탐색: **지지**. 큰 묶음은 목표 4개뿐이며 남은 비용의 주성분이다 → 단위 3으로 넘긴다.
- [x] 몰림 원인: 큰 묶음은 모두 목표 전환(목표 Tile 변경 약 70%·AfterDirect 약 30%)이다. 타겟 교체·이동 취소는 작은 묶음이고, Pod 깨움 몰림은 S1에서 최대 12건이다. overlay 무효화는 30초 창에 없어 판정하지 못했다.
- 한계: S1 창에는 Chase 요청이 없었다(355건 모두 Background). 실제 플레이어 추격의 Tile 경계·drift 몰림은 합성 추격의 목표 전환으로만 대표한다.

### 단위 2 — A* 기준선 개선 — 완료 2026-10-13

- [x] cache 합류 검증 완화: 공유 경로의 앞쪽 4개 waypoint 중 시작 node에서 직선 보행 가능한 첫 waypoint에 합류한다. 결과 `FirstWaypoint`로 넘기고 경로 추종이 그 index부터 시작한다. key는 그대로다.
- [x] 합류 거리 비 가드(계획 밖 추가, 결정론·cost 비율 검증에서 필요해짐): 합류 거리 a ≤ 0.1 × 합류점 이후 경로 길이 s. 단위 1까지의 cache도 보행 길이 비율 P95가 1.17~1.65로 기준 3을 넘었고, 가드 없는 앞쪽 합류는 1.42~1.66으로 더 나빴다. 가드는 요청별 비율을 (1+r)/(1−r) 이하로 묶는다(D-079).
- [x] 거리 기반 Chase 우선순위: 같은 tick 제출분 안에서 직선 거리 오름차순. 앞 tick 요청이 먼저다.
- [x] 몰림 시 예산 적응: 재생 상한 측정(예산 8000) 뒤 구현하지 않음. S3 burst P95 43→22지만 재생 tick P95가 887→1,452us로 늘어 실제 S3 경로 tick(1.133ms)이 Gate 1.5ms를 넘는다.
- [x] 재생기 변형 추가(`join4`, `cache256Lead0.1`, `join4Lead{0.05,0.1,0.2}`, `nearest`, `improved`, `improvedSerial`, `improvedBudget8000`)와 보행 길이 지표 `walk`. 비교표 `Scripts/Profiling/ComparePhase10Replay.py`.
- [x] 결정론: `improvedSerial`이 `improved`와 요청별로 같다(S1~S3 불일치 0). `Nav.PathScheduler`에 앞쪽 합류(Pod 가림막)·가드·Chase 거리 순서 검사를 추가했다.
- 결과(기준 cache256 → improved): burst 완료 tick P95 S2 12→8, S3 63→43, P50 S3 19→10. 보행 길이 비율 P95 S1 1.65→1.00, S2 1.17→1.01, S3 1.23→1.02. 확장 합은 가드 때문에 S2 +4%·S3 +10%·S1 +64%다. 수치는 `history/Phase10_Log.md` 2026-10-13 단위 2.
- 단위 3 이후의 비교 기준선은 `improved`(= 현재 runtime 기본값)다.
- 보류 유지: 재요청 시차 분산, 목표 이동 시 경로 재사용. 같은 목표 공유는 단위 3.

### 단위 3 — 목표별 flow field 프로토타입 — 완료 2026-10-13

`LNPNavFlowField.{h,cpp}`는 순수 함수이며 processor에 연결하지 않는다.

- [x] portal·seam link 역방향 대칭성: 회귀 fixture·production 두 배치에서 역방향 없는 edge 0, 두 방향 비용 불일치 0. 비용은 보행 방향(이웃 → 확정 node)으로 재므로 전제는 edge 양방향 존재뿐이고, `Nav.FlowField`가 계속 검사한다.
- [x] goal node에서 반경(경로 cost) 한정 Dijkstra를 여러 프레임에 나눠 짓는다. 예산 단위는 확정 node 수(확정당 0.40us, A* 확장당 0.45us). 저장은 overlay Tile key별 블록(거리·다음 node·확정 bit)이다.
- [x] 조회는 확정 node에서 다음 node 열을 따라간 뒤 `SimplifyPath`로 단순화한다. 짓는 중에도 확정된 node는 답한다.
- [x] 반경 밖·스냅 실패는 improved A*로 폴백하고, 그 비용(재스냅 포함)도 같은 프레임 예산에서 뺀다.
- [x] 재생 변형 `flowfield`(반경 12,000, burst마다 field 버림)와 스윕(반경 6,000~24,000, `Keep`, `FreeAnswer`), `flowfieldSerial` 결정론 대조. 요청 결과 `source` 열, `_FlowFields.csv` 요약, 비교표 확장.
- [x] 검증: 에디터 빌드, `LootNPop.SurfaceNavigation` 130/130, S1~S3 재생 Success·불일치 0.
- 예비 판정(improved 대비): 기준 1 충족(tick 합 S2·S3 −38%, tick P95 악화 없음), 기준 2 미충족(burst P95 S2 8→9, S3 43→23 = −47%), 기준 3 충족(보행 비율 P95 1.000), 기준 4 충족(field 최대 0.39MiB). 기준 5는 설계 메모만 남겼다. 수치는 `history/Phase10_Log.md` 2026-10-13 단위 3.
- 목표 양자화는 입력 목표가 고정점 4개라 평가할 수 없어 구현하지 않았다.

### 단위 4 — 계층형 A* 프로토타입

`LNPNavHierarchy.{h,cpp}`로 만든다.

- Cluster는 Layer별 16×16 Tile이다. 경계 span을 Portal로 압축하고, 내부 Portal 간 비용을 사전 계산한다.
- seam link·Layer portal은 고수준 edge로 둔다.
- query는 고수준 A* → corridor 안 저수준 A* 순서다.
- Pod overlay가 바뀌면 해당 Cluster만 dirty로 다시 계산한다.

### 단위 5 — 비교와 채택 판정

- 재생기에 변형(기준 A*+cache256, 개선 A*, flow field, 계층형)을 추가해 S1~S3로 실행한다. overlay 토글 재생으로 동적 재계산 범위를 잰다.
- 결과는 `history/Phase10_Log.md`와 `design/GroundNavigation.md`에 기록하고, `Decisions.md`에 새 결정을 한 줄 추가한다.

### 단위 6 — 채택된 방식만 production 통합(해당 시)

패키지 Gate(`RunPhase13Gate.ps1 -Cases Production`)와 리슨 2P·late join 스모크로 검증한다. 미채택이면 Phase 10은 비교 자료와 단위 2 개선으로 종료한다.
