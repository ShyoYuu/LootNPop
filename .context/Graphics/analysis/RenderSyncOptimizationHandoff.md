# GT 렌더 동기화 비용 분석·최적화 인계

> 상태: 분석 인계 · 원인 패스 미분해 · 최적화 미구현
> 갱신: 2026-11-08
> 목적: 사용자가 별도 세션에서 GT의 렌더 대기와 RT/GPU 비용을 조사·개선할 근거

## 1. 관측한 문제

기존 CPU 진단에서 GT가 렌더링 쪽을 기다리는 큰 구간을 확인했다. **대기9.27ms를 GT 계산9.27ms로 해석하지 않는다.** 어떤RT/GPU 패스·RHI/프레젠트 경로가 지연을 만드는지는 아직 분해하지 않았다.

| 타이머 | 호출 수 | 포함 시간 합 | GT2983프레임당 포함 시간 |
|:---|---:|---:|---:|
| `Sync_RenderingThread` | 2983 | 27.695381초 | 9.2844ms |
| 그 안의 `GameThreadWaitForTask` | 2949 | 27.664501초 | 9.2741ms |

두 행은 중첩 관계이므로 합산하지 않는다. `WaitForTasks`3.6396ms/프레임은 별도의 월드 후처리/틱 완료 경로로, 같은 이름의 렌더 동기화 대기가 아니다. 틱 완료 최대약29.60ms에는 GT Wander27.34ms 실행이 포함돼 있어 순수 대기29.60ms로도 쓰지 않는다.

별도 정식180초 워밍업 세트의 Medium 회차별3개 값 중앙값은 다음과 같다.

| Frame | GT | RT | GPU | 목표 예산 |
|---:|---:|---:|---:|---:|
| 20.5220ms | 10.5710ms | 20.3779ms | 19.9153ms | 16.6ms |

GPU 시간 자체가 예산을 넘고 Frame과 가까워 렌더링 측 비용 분석의 필요성을 뒷받침한다. 이것만으로 특정GPU 패스가 원인이라고 확정하지 않는다. RT/GPU/GT는 병렬·파이프라인 관계이며 더해서 Frame 시간을 만들지 않는다. CPU 진단과 정식 측정도 서로 다른 실행이다.

## 2. 코드·설정 진입점

- 설치 엔진: `C:\Program Files\Epic Games\UE_5.8\Engine`.
- `Source/Runtime/RenderCore/Private/RenderingThread.cpp`: `GameThreadWaitForTask` 약1158행, `FFrameEndSync` 관련 `Sync`/`Sync_RenderingThread` 약2482행. 엔진 소스는 동기화 의미 확인용이다. 프로젝트 그래픽스 범위는 엔진 수정 없이 진행한다.
- 위 `Sync`는 RenderThread fence 및 조건에 따른 RHI/스왑체인 fence로 GT가 표시 프레임보다 지나치게 앞서지 않도록 동기화한다. `r.OneFrameThreadLag`와 GT sync 설정 등에 따른 경로가 있으나, 이 진단에서 모든 실효값을 별도 분해한 것은 아니다.
- [LNPGraphicsBenchmark.cpp](../../../Source/LootNPop/Graphics/LNPGraphicsBenchmark.cpp): 측정 조건 적용과 CSV 수집. 준비 코드의 요청은1440p·에픽·TSR100%·동적 해상도0·`r.VSync=0`·`t.MaxFPS=0`이다. 실제 비교에는 각 `Run.json`의 실효 조건을 확인한다.
- [측정 규약](../design/Measurement.md)과 [그래픽스 결정](../Decisions.md): 패키지 성능 판정, 고정 경로·시드·3회 반복, 엔진 수정 없는 렌더 확장 원칙.

## 3. 다음 세션에서 분리할 원인

| 조사 지점 | 필요한 구분 |
|:---|:---|
| RT의 실제 작업 | 뷰/가시성/드로우 준비·효과 렌더 데이터 등과 RT가 다른 파이프라인을 기다리는 시간 |
| GPU 패스 | 그림자·기하·머티리얼·후처리·Niagara/라이트 등 패스별 비용. 현재 어느 것도 주범으로 확정하지 않음 |
| RHI·프레젠트 | 작업 제출·GPU 완료·표시/프레임 제한·동기화 설정에 따른 대기 |
| 환경 변화 | 온도·클럭·전력·활성/가시 개체 분포·PSO/GC와 프레임 이탈 |
| 동기화와 처리량 | 대기 시간이 줄어도 파이프라인 선행/입력 지연만 바뀐 것인지, 실제Frame/GPU 처리량이 개선된 것인지 |

fence나 대기를 제거하거나 선행 프레임 수를 늘린 결과만으로 렌더 작업이 빨라졌다고 판정하지 않는다. 원인 확인 후 프로젝트 설정·콘텐츠·프로젝트 렌더 코드 범위에서 개선한다. 그래플 앵커 효과 변경은CPU/GT 작업뿐 아니라 GPU 부하도 바꿀 수 있어 변경 식별값을 함께 남긴다.

## 4. 원자료와 재현 범위

CPU 진단 루트:

`Saved/Profiling/Graphics/Phase00/MediumCPUTrace_20261107_102250_f65aca/`

- 트레이스: `Medium_20261107_102251_8b8bb9/CPU.utrace`.
- 분석 구간: **184.502898~244.502898초·GT2983프레임**. 전체 세션의 시작/워밍업 통계와 구분한다.
- 근거: `CpuCostBreakdown.json`, `AnalysisValidation.json`, `Exports/Measured_GameThread.csv`, `Measured_AllThreads.csv`, `Threads.csv`, `Timers.csv`.
- 다른 작업 대기와 구분할 근거: `WaitCallPathObservation.json`, `TaskRelationObservation.json`.
- 환경: `Session.json`, `Environment.json`, `GpuTelemetry.csv`, `DiagnosticSummary.json`.

정식 측정은 `Saved/Profiling/Graphics/Phase00/MediumWarmup180_20261107_100022_361426/`와 `FormalBaseline180_20261107_Summary.json`을 따른다. 원래 기준선 패키지는 `Saved/Graphics/Phase00Package/`, 별도 배회 계측 패키지는 `Saved/Graphics/Phase00WanderDiagnosticsPackage_20261108/`다. 새 바이너리·설정과 과거 값을 동일 실행 조건으로 묶지 않는다.

조건은 Medium 초기 적2223명·시드1·Revision3 카메라·180초 워밍업/60초 경로다. 관찰 카메라를 따라 AI 활동 거리도 바뀌는 배회 장면이다. 세 편성은 초기 적뿐 아니라 배치/부하 분포가 달라 편성 차이를 단일 렌더 기능의 절제로 쓰지 않는다.

CPU 진단 종료777003·원인 미확정과 Insights 메타데이터 경고를 보존했다. `DiagnosticOnly=true`·`BaselineEligible=false`다. GPU 패스별 프로파일은 아직 수집/분석하지 않았다. `Saved/`는 로컬 자료이므로 없는 환경에서 새 긴 실행을 자동으로 시작하지 않는다.

## 5. 검증과 결과 인계

1. 먼저 기존 자료로 구분 가능한 범위를 확인하고, 새 GPU/RT 진단이 필요하면 독립 실행의 조건과 범위를 사용자와 정한다. 수동 Insights/GPU 캡처 조작이 빠르면 바로 요청한다.
2. 전후 해상도·TSR·품질·VSync/제한·RHI·동기화 실효값·카메라·패키지 식별을 남긴다. 표시 품질이나 지연 정책이 바뀌면 수치와 함께 밝힌다.
3. GT 대기 감소, RT 실제 작업/대기, GPU 패스, Frame 평균/꼬리 지연을 각각 비교한다. 원인이 확인되지 않은 큰 대기 시간만 최적화 성과로 삼지 않는다.
4. 같은 전원·화면 조건과 온도/클럭 기록을 유지하고, 필요한 정식3회 검증은 진단 트레이스와 분리한다. 에디터 `-game`을 패키지 성능 기준선으로 쓰지 않는다.
5. 원인·변경·품질/지연 차이·검증 수치와 남은 병목을 그래픽스 세션에 돌려준다. 기존 Medium GT7.2288%/허용5% 미통과·영상/최종 보고/깨끗한 트리 빌드 게이트는 별도다.

관련 인계: [그래플 앵커](GrappleAnchorOptimizationHandoff.md), [배회 프로세서](WanderOptimizationHandoff.md), [Nav 최근접 노드](NavNearestNodeOptimizationHandoff.md).

네 개선 완료 후의 통합 검증·새 반복 측정·Phase0 마무리는 [개선 후 재개 절차](../phases/Phase00_PostOptimization.md)를 따른다.
