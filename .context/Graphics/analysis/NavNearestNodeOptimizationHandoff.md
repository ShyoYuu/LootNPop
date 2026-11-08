# Nav 최근접 노드 탐색 최적화 인계

> 상태: 분석 인계 · 호출 원인별 비용 미분해 · 최적화 미구현
> 갱신: 2026-11-08
> 목적: 사용자가 별도 세션에서 `LNPEnemyPath_NearestNode`의 반복 조회를 조사·개선할 근거

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
