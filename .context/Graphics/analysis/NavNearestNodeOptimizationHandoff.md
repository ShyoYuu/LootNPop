# Nav 최근접 노드 탐색 최적화 인계

> 상태: 완료 — 호출자 분해(§7.1) · 힌트 반경 최근접 구현(§7.2) · 자동화 일치(§7.4) · 패키지 호출당 3.72→1.27μs(§7.5). 자동화 실패 2건은 별도 분석
> 갱신: 2026-11-08
> 목적: 사용자가 별도 세션에서 `LNPEnemyPath_NearestNode`의 반복 조회를 조사·개선할 근거

## 0. 진행 시점과 첫 확인 (2026-11-08 사용자 확인)

- **시점:** 배회 세션이 끝난 뒤에 한다. GT 렌더 동기화 세션과는 파일이 겹치지 않아 병행할 수 있다.
- **배회 세션과 겹치는 범위:** 배회 세션의 미커밋 변경이 [LNPNavGraph.h](../../../Source/LootNPop/SurfaceNavigation/LNPNavGraph.h)에 `ResolveEndpointsCached`와 `FStartCache`를 추가했다(2026-11-08 작업 트리 기준). 같은 시작점의 최근접 노드를 한 번만 구하고, 시작·목표의 최근접 노드가 같은 group이면 후보 수집·정렬 없이 답하는 빠른 경로다. 이 문서의 원래 대상인 `LNPEnemyPath_NearestNode` 래퍼와 같은 함수인지는 확인하지 않았다.
- **첫 작업:** 배회 세션의 최종 변경을 받은 뒤 `LNPEnemyPath_NearestNode` 호출 수·프레임당 시간을 다시 재고, 아래 §1 수치와 비교한다. 줄어든 몫을 뺀 나머지만 이 세션의 범위로 삼는다. 남은 비용이 작으면 개선 없이 측정 결과만 돌려주는 것도 완료로 친다.

## 1. 확인한 수치와 우선순위의 한계

기존 CPU 진단의 GT 집계는 다음과 같다. 호출 수는 많지만 측정된 평균 비용은 배회의 순간 부하나 렌더 대기보다 작다. **단일 호출이 느리다거나 최우선 병목이라고 확정한 것은 아니다.**

| 타이머 | 호출 수 | 포함 시간 합 | GT2983프레임당 포함 시간 |
|:---|---:|---:|---:|
| `LNPEnemyPath_NearestNode` | 169284 | 0.629969초 | 0.2112ms |
| `LNPEnemyPathProcessor_2147482008` | 2982 | 0.872564초 | 0.2925ms |

최근접 타이머는 프레임당약56.75회·호출당약3.72us이며 집계의 단일 호출 최댓값은0.534ms다. 개별 이벤트 분포·호출 경로별 비율은 아직 내보내지 않았다. 상위 PathProcessor의 포함/하위 타이머 시간을 합산하지 않는다. 최근접 래퍼는 여러 프로세서에서 호출되므로169284회를 전부 EnemyPathProcessor나 배회의 호출로 귀속하지도 않는다.

## 2. 코드 진입점과 이미 있는 최적화

- [LNPEnemyPathProcessor.cpp](../../../Source/LootNPop/Enemy/LNPEnemyPathProcessor.cpp)의 익명 namespace `NearestNode` 약60행이 계측 위치다.
- 래퍼는 Surface 유효성·SnapshotGeneration을 확인하고 `CollectNodesNear(..., NavSnapRadius=300, ..., bNearestOnly=true)`를 호출해 첫 노드 또는INDEX_NONE을 반환한다.
- [LNPNavGraph.cpp](../../../Source/LootNPop/SurfaceNavigation/LNPNavGraph.cpp)의 `CollectNodesNear` 약467행은 slot/layer를 찾고 `ScanLayer`에 위임한다. `bNearestOnly=true`일 때 일반 후보 정렬을 하지 않는다. **이미 최근접1건 전용 경로가 있으므로 모든 후보 정렬을 제거하자는 제안과 중복되지 않게 한다.** 하위 공간 탐색 구조는 함수 본문을 확인한 뒤 판단한다.
- EnemyPath의 목표 투영에는 `FGoalProjectionKey{Position, Surface}`와 `GoalProjections` 캐시가 이미 있다. 키는 Generation·OctantSlot·LocalLayerId 등을 포함하고 좌표의+0/-0 해시도 정규화한다. 캐시 범위·적중률·무효화를 확인한 뒤 추가 재사용을 설계한다.

확인된 래퍼 호출 지점은 같은 파일의 다음 경로다.

| 경로 | 코드 기준 위치 | 입력·특징 |
|:---|:---|:---|
| PlayerNav | 약141행 | 바닥 Hit의 표면/위치로 플레이어 노드 계산. Overlay 인자는nullptr |
| PodNav | 약236행 | Pod의 표면/위치로 노드 계산. Overlay 인자는nullptr |
| EnemyPath 시작 투영 | 약392행 | 적의 현재 시작점. Overlay 사용 |
| EnemyPath 목표 투영 캐시 미스 | 약406행 | 목표 위치/표면. Overlay 사용, 기존 목표 캐시 조회 후 실행 |

위 위치는 인계 당시 소스 기준이다. 다른 세션의 변경 후에는 함수/분기로 찾는다. [배회 프로세서](WanderOptimizationHandoff.md)의 직접경로 검사와 이 래퍼의 호출은 구분한다. 같은 Nav 하위 함수를 공유한다고 해서 계측 타이머의 비용이 자동으로 배회에 포함되지는 않는다.

## 3. 별도 세션의 조사·개선 후보

| 조사 지점 | 필요한 근거 |
|:---|:---|
| 호출자별 비중 | Player/Pod/Enemy 시작/목표 캐시 미스별 호출 수·시간·활성 개체 수 |
| 변경 없는 입력의 반복 | 같은 위치/Surface/Snapshot/Overlay 조건의 연속 조회와 반환 노드 안정성 |
| 기존 목표 캐시 | 수명·적중/미스·동일 목표 공유·키별 재사용 횟수. 이미 있는 기능을 중복 구현하지 않음 |
| 시작/정적 대상 재사용 | 이동량·현재 노드/타일·Generation·Overlay 변경에 따른 재조회 필요성과 실제 적중률 |
| 하위 공간 조회 | 방문 셀/후보 수·할당량·거리 비교·타일 차단 검사·최근접 동률 처리의 비용 |
| 최댓값 사례 | 드문 긴 조회가 많은 후보/상태 변경/OS 선점 중 무엇과 관련되는지 |

각각의 절감량·개선 방향은 아직 미확정이다. NavSnapRadius를 줄이거나 조회 빈도를 낮춰 부하를 줄인다면 유효 노드를 놓치거나 반응 지연을 늘리지 않는지 함께 검증한다.

## 4. 보존해야 할 조회 의미와 검증

- 구면 월드의 slot/layer/Surface 정체성과 SnapshotGeneration을 유지한다. 위치만 같은 캐시 키로 다른 표면을 혼용하지 않는다.
- Overlay 차단·타일 리비전·연결성 변경과 스냅샷 교체 시 재사용 가능 여부를 확인한다. 현재 호출별nullptr/Overlay 차이를 보존한다.
- 실제 최근접 노드·범위 밖INDEX_NONE·무효/오래된 Surface·경계/레이어·동률의 기존 선택 규칙을 보존한다.
- PlayerNav·PodNav·EnemyPath의 서로 다른 갱신/무효화 조건과 목표 캐시를 함께 검토한다. 추격/복귀/동적 지형/경로 재계산 결과도 확인한다.
- 기존 검증 진입점은 [LNPSurfaceNavigationRuntimeTest.cpp](../../../Source/LootNPop/Tests/LNPSurfaceNavigationRuntimeTest.cpp)와 관련 Nav 테스트다. 이 테스트 하나가 최근접 선택·캐시의 모든 조건을 검증한다고 가정하지 않는다. 이 인계 작성에서 테스트를 새로 실행하지 않았다.
- 같은 입력의 결과 일치와 실제 호출/방문 후보/할당 감소를 검증한 뒤, 패키지에서 평균·P95/P99/최대와 전체GT 영향을 비교한다. 작은 타이머 개선과 전체프레임 개선을 구분한다.

## 5. 원자료와 실행 식별

프로젝트 루트 기준 원자료:

`Saved/Profiling/Graphics/Phase00/MediumCPUTrace_20261107_102250_f65aca/`

- 트레이스: `Medium_20261107_102251_8b8bb9/CPU.utrace`.
- **분석 구간184.502898~244.502898초·GT2983프레임**. 시작/워밍업을 포함한 전체세션 통계와 구분한다.
- 근거: `CpuCostBreakdown.json`, `AnalysisValidation.json`, `Exports/Measured_GameThread.csv`, `Measured_AllThreads.csv`, `Threads.csv`, `Timers.csv`.
- 환경/실행: `Session.json`, `Environment.json`, `DiagnosticSummary.json`, `GpuTelemetry.csv`.

패키지 Development·Medium 초기 적2223명·시드1·Revision3 경로·180초 워밍업/60초 캡처의 배회 관찰 장면이다. 호출 패턴을 전투 장면으로 일반화하지 않는다. 실제 종료777003·원인 미확정·Insights 메타데이터 경고를 보존했으며 `DiagnosticOnly=true`·`BaselineEligible=false`다.

원래 기준선 패키지 `Saved/Graphics/Phase00Package/`와 별도 배회 계측 패키지 `Saved/Graphics/Phase00WanderDiagnosticsPackage_20261108/`를 혼동하지 않는다. `Saved/` 원자료가 없는 환경에서 재패키징/긴 실행을 자동으로 시작하지 않는다. 자세한 실행/패키지 식별은 각 세션의 기록과 [측정 규약](../design/Measurement.md)을 따른다.

## 6. 결과를 돌려줄 때

호출자별 비용과 반복 입력 근거, 변경한 캐시/조회 의미, 무효화·결과 일치 검증, 소스/패키지 식별 및 전후 호출 수·시간을 남긴다. 다른 세션도 `LNPEnemyPathProcessor.cpp`나 공통 `LNPNavGraph`를 바꿀 수 있으므로 현재 변경을 먼저 확인하고 출처를 구분한다. 기존 Medium GT7.2288% 재현성 미통과와 Phase0 미완료는 별도 검증 전까지 유지한다.

관련 인계: [배회 프로세서](WanderOptimizationHandoff.md), [그래플 앵커](GrappleAnchorOptimizationHandoff.md), [GT 렌더 동기화](RenderSyncOptimizationHandoff.md).

네 개선 완료 후의 통합 검증·새 반복 측정·Phase0 마무리는 [개선 후 재개 절차](../phases/Phase00_PostOptimization.md)를 따른다.

## 7. 조사 결과와 개선 (2026-11-08)

### 7.1 호출자 분해 — 기존 트레이스만으로 끝난다

§5 원자료의 `Exports/Measured_GameThread.csv`(같은 분석 구간)에 호출 지점별 하위 타이머가 이미 있었다.

| 타이머 | 호출 수 | 포함 시간 |
|:---|---:|---:|
| `LNPEnemyPath_NearestNode` | 169284 | 0.630초 |
| `LNPEnemyPath_StartProjection` | 84642 | 0.345초 |
| `LNPEnemyPath_GoalProjectionMiss` | 84642 | 0.307초 |
| `LNPEnemyPath_GoalProjection`(Hit 포함 상위) | 84642 | 0.323초 |
| `LNPEnemyPath_DirectGoal` | 84642 | 0.089초 |
| `LNPEnemyPath_FollowWaypoints` | 11878 | 0.008초 |
| `LNPEnemyPath_TargetReachability` | 0(행 없음) | — |

- 시작 84642 + 목표 미스 84642 = 169284 = 래퍼 전체. **이 장면의 PlayerNav·PodNav 호출은 0회**이고, 프레임당 약 28.4명이 시작·목표를 1회씩 매 프레임 조회한다.
- **실행 내 목표 캐시 적중 0%**(Hit 타이머 행 없음, Miss = 전체). 목표가 개체마다 다른 `FeetOf(MoveTarget.Center)`라 실행 안에서 공유되지 않는다. 대신 같은 개체의 목표는 새 배회 목표 전까지 프레임마다 그대로다.
- 대상은 Active 대역에서 직선 보행 중인 비추격 적이다(DirectGoal = 시작 수, 경로 추종 14%, 제출 112회).
- §0의 배회 세션 변경(1de54ff `ResolveEndpointsCached`·`FStartCache`)은 `LNPEnemyWanderProcessor`만 쓰는 **다른 함수**다. `LNPEnemyPathProcessor.cpp`는 그 뒤 바뀌지 않았으므로 §1 수치가 현재 코드의 기준이다.
- 조회 1회는 300cm 구를 감싸는 격자 창(`GetGridSearchBounds`, ±1셀 여유)을 전부 훑고 셀마다 쿼터니언 회전 + 거리 비교를 한다. Crust 200cm·Layer 100cm 간격이라 창은 수십~백여 셀이다. 비용은 단일 호출이 느려서가 아니라 **같거나 거의 같은 입력으로 넓은 창을 매 프레임 다시 훑는 것**에서 나온다.

### 7.2 변경 — 힌트 node로 탐색 반경을 줄이는 정확 최근접

- `LNPNavGraph::FindNearestNode(Nav, Slot, LayerId, Position, Radius, Overlay, HintNode)` 추가. 힌트가 같은 slot·Layer에 있고 정적·Overlay 차단이 아니며 Radius 안이면 반경을 `min(Radius, d(P, 힌트) + 1cm)`로 줄여 같은 `ScanLayer`를 돈다.
- **결과 의미는 그대로다.** 최근접 거리는 힌트 거리 이하이므로 최근접과 동률 node가 모두 줄인 창 안에 남는다(거리→Node index 동률 규칙 유지). 반경은 원래 값을 넘지 않아 범위 밖 `INDEX_NONE`도 같고, 거리식이 같아 `Distance`도 같다. 1cm 여유는 제곱·제곱근 반올림으로 경계 node가 빠지는 것을 막는다.
- **힌트는 무효화하지 않는다.** 아무 node여도 상한으로만 쓰이고 조건이 맞지 않으면 전체 반경으로 찾는다. 그래서 스냅샷 세대·Overlay revision을 키로 저장하지 않는다.
- `FLNPEnemyPathFragment`에 `StartHintNode`·`GoalHintNode`를 두고 EnemyPath 시작 투영과 목표 투영 미스에 지난 결과를 넘긴다. 실행 내 `GoalProjections` 캐시는 추격 장면 적중률을 재기 전까지 유지한다. PlayerNav·PodNav는 바꾸지 않았다.
- 내부 `ScanLayer`의 출력 배열 타입을 템플릿 인자로 일반화해 새 경로는 `TInlineAllocator<1>`을 쓴다. 래퍼의 호출당 힙 할당이 없어진다.
- 기대치(추정): 시작 창 반경 300 → 약 60~120cm, 목표는 몇 셀 수준. 이 장면 래퍼 0.211ms/프레임 → 0.03~0.06ms 예상. GT 전체로는 최대 0.15ms 안팎의 **작은 개선**이며 최우선 병목 해소가 아니다.

### 7.3 하지 않은 것

- 셀마다 회전하지 않고 로컬 좌표로 비교: 더 빠르지만 거리가 ULP 단위로 달라져 동률·`Distance` 일치가 깨진다. 힌트 적용 뒤에도 비용이 남을 때의 후속 후보로만 둔다.
- 제곱 거리 비교로 `Sqrt` 생략: 다른 d²가 같은 제곱근으로 반올림되는 경우 동률 선택이 바뀐다. 이득도 작다.
- NavSnapRadius 축소·조회 빈도 낮추기: 의미가 바뀐다. 힌트로 의미 변경 없이 같은 효과를 얻는다.
- `TargetReachability`의 `ResolveEndpoints`(전체 후보+정렬): 이 장면 0회라 근거가 없다. 전투 장면 측정에서 따로 본다.

### 7.4 검증 현황

- 게임 타깃·에디터 타깃 `Build.bat ... Win64 Development` 모두 통과.
- 자동화 `LootNPop.SurfaceNavigation.Nav.HintedNearest` 추가(`LNPEnemyActivityTest.cpp`, 회귀 지형 픽스처). 지면점과 반경 밖 띄운 점마다 전체 반경 기준과 비교한다. 힌트는 없음·범위 밖 index·다른 slot/Layer/먼 node 표본·정답 자체·이웃 점의 정답·Overlay로 막은 정답·이동 경로 연쇄. node·거리·group이 `==`로 같아야 한다. **통과: 1232점(node 있음 616)·117589회 조회 전부 일치.**
- 헤드리스 자동화 `Nav`·`Wander`·`WanderReplication`·`Runtime`·`Activity`·`EnemyMovement` 묶음(`Saved/Logs/HintedNearestAutomation.log`): `Wander.CachedEndpoints` 포함 나머지 통과, **2건 실패**:
  - `Nav.Hierarchy`: 생산 hierarchy 9.44 MiB > 8 MiB 예산, seed 1768 waypoint 길이 비 P95 1.0541 > 1.05.
  - `Nav.ProductionSpawnProjection`: 후보 87244개 중 1개 투영 실패(slot 4 layer 0).
  - 두 테스트 모두 이번 변경 경로를 쓰지 않는다. 투영은 수정하지 않은 `LNPNavQuery`이고, hierarchy는 메모리 예산·경로 비율이다. `ScanLayer` 변경은 출력 배열 타입만 일반화해 기존 호출의 동작이 같다. 문서 기록상 생산 후보 수는 9990개(Phase07a)였는데 지금은 87244개라 생산 콘텐츠 변화로 생긴 실패로 추정한다. **이번 변경 전 코드에서 다시 돌려 확인하지는 않았다.**
### 7.5 패키지 측정 (2026-11-08, 1회)

사용자 결정으로 **NearestNode 계열 타이머만** 비교하고 전체 GT 영향은 재지 않았다. 원래 Nav가 병목이 아니었기 때문이다. 기준은 §5 트레이스이고, 참고로 1de54ff(배회 최적화 후·이번 변경 전) 트레이스도 같은 방법으로 집계했다.

- 변경 후 실행: 패키지 `Saved/Graphics/NavHintedNearestPackage_20261108/`, 원자료 `Saved/Profiling/Graphics/Phase00/NavHintedNearestTrace_20261108_120257_203a03/`. 인자는 §5와 같다(Medium·180초/60초·`-trace=cpu,frame,bookmark,log,task`). 카메라 경로 해시는 기준 패키지와 같다. 종료 코드 777003·`Status=complete`는 §5와 같은 기존 현상이다. `DiagnosticOnly=true`·`BaselineEligible=false`.
- 스크립트와 식별값: `Saved/Profiling/Graphics/Phase00/NavHintedNearest_20261108/`. 실행은 `RunMeasurement.ps1`, 내보내기는 `ExportEvents.ps1`, 집계는 `AnalyzeNearest.py`이고 `PackageIdentity.json`·`SourceCommit.txt`(6461246+이번 미커밋 변경. 1de54ff 뒤의 그래플 앵커 최적화·Bastion/Hollow 외곽 엄폐 지형 보강이 함께 들어 있다)·`SourceChanges_BeforePackage.patch`를 남겼다. 세 트레이스 모두 GameThread 개별 이벤트를 전체 트레이스에서 내보내 같은 스크립트로 자른다. 기준 구간은 기존 통계(169284회·0.629969초·2982프레임)를 그대로 재현했다.
- 구간 시작: 로그에는 캡처 시작 시각만 있고 트레이스 시각은 없다. 그래서 기준 실행의 "로그 첫 줄 → 캡처 시작" 경과와 확인된 트레이스 시각의 차이(0.296초)를 다른 실행에 더해 추정했다. 1de54ff 트레이스는 정렬로 확정한 시작(188.808초)과 0.2초 차이가 났다. 시작을 ±1초 옮겨도 호출당 평균은 0.01μs 이내, 프레임당 시간은 0.001ms 이내로만 바뀐다.

| 실행 | 프레임 | 래퍼 호출(프레임당) | 래퍼 포함 시간 | 프레임당 | 호출당 평균 | P50 / P95 / P99 / 최대(μs) | 시작 투영 평균 | 목표 미스 평균 | PathProcessor 프레임당 |
|:---|---:|---:|---:|---:|---:|:---|---:|---:|---:|
| 기준 §5 (11-07) | 2982 | 169284 (56.8) | 0.6300초 | 0.2113ms | 3.72μs | 3.4 / 5.9 / 9.8 / 533.7 | 4.08μs | 3.63μs | 0.2926ms |
| 1de54ff (참고) | 2988 | 131274 (43.9) | 0.5964초 | 0.1996ms | 4.54μs | 4.3 / 7.5 / 11.8 / 122.3 | 5.00μs | 4.39μs | 0.2824ms |
| **변경 후** | 3009 | 234910 (78.1) | 0.2993초 | **0.0995ms** | **1.27μs** | 1.1 / 2.6 / 5.3 / 222.4 | 1.45μs | 1.34μs | 0.2177ms |

- **호출당 평균이 3.72~4.54μs에서 1.27μs로 줄었다(약 2.9~3.6배).** 분위수 P50·P95·P99도 모두 절반 이하로 내려갔다.
- **호출 수는 실행마다 크게 달랐다**(프레임당 43.9~78.1회). 이번 변경은 호출 수를 바꾸지 않는다. 시작+목표 미스 = 래퍼, 목표 캐시 적중 0은 세 실행 모두 같다. 그래서 차이는 Active 대역에서 직선 보행하는 적의 수가 실행마다 달랐기 때문이다(변경 후 프레임당 약 39명, 기준 28명). 소스 차이(배회 최적화·지형 보강)와 실행 간 편차 중 어느 쪽 몫인지는 나누지 않았다. 프레임당 시간은 호출 수에 비례하므로 **호출당 평균이 이번 비교의 기준값**이다. 기준 호출 수(56.8회/프레임)로 환산하면 프레임당 약 0.072ms다.
- 프레임당 시간은 호출이 1.4배 많은데도 0.2113 → 0.0995ms로 줄었다. PathProcessor 포함 시간은 같은 이유로 호출 수가 섞여 있어 참고값이다.
- 목표 미스 평균(1.34μs)이 시작(1.45μs)과 비슷하다. 목표 창이 몇 셀로 줄어도 남는 고정 비용(타이머 범위 자체, Layer 탐색, 격자 범위 계산)이 1μs 안팎인 것으로 보인다. 더 줄이려면 탐색 바깥 단계를 봐야 하는데, 절대값이 작아 지금은 진행하지 않는다.
- 최댓값 222.4μs는 단일 이벤트다. 원인(선점·캐시 미스)은 확정하지 않았다.
- 1회 측정이며 전체 GT·프레임 시간은 판정하지 않았다.
