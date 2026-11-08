# 그래플 앵커 효과 최적화 인계

> 상태: 분석 인계 · 최적화 미구현
> 갱신: 2026-11-08
> 목적: 사용자가 별도 세션에서 `NS_GrappleAnchor`와 관련 작업 대기를 조사·개선할 근거

## 1. 확인한 문제와 해석 범위

그래플 앵커 Niagara는 배회 외의 후속 조사 후보다. 기존 CPU 진단에서 GT의 효과 처리와, 해당 효과를 포함한 워커 작업 때문에 GT가 기다린 대표 사례를 확인했다. **효과 내부의 어느 단계가 비싼지, 전체 월드 후처리 대기의 얼마를 차지하는지는 미확정**이다.

| 관측 항목 | 측정값 | 의미 |
|:---|---:|:---|
| GT `NS_GrappleAnchor` | 66194회·포함1.312407초·제외1.295940초 | GT2983프레임으로 나누면 포함0.4400ms/프레임 |
| 전체 스레드 `NS_GrappleAnchor` | 1643633회·제외8.090761초 | 병렬 스레드 합계다. 같은 길이의 프레임 지연이나 고유 앵커 수를 뜻하지 않음 |
| 전체 `WaitForTasks` | 10.856891초·3.6396ms/프레임 | 아래 월드 후처리와 틱 완료 경로가 섞임 |
| 월드 후처리 분기 | 약7.31초·2.45ms/프레임·67.31% | GUI 반올림 값. 이 전체를 앵커 효과 비용으로 귀속하지 않음 |

대표 최대 사례의 GT 대기는239.319501~239.322189초·약2.69ms다. `OnWorldPostActorTick → WaitUntilTasksComplete`에서 **Task15523600 하나**를 기다렸다. 해당 작업은 Foreground Worker(Standby #1)에서239.319253~239.322174초·약2.92ms 실행됐고 내부에 `NS_GrappleAnchor` CPU 범위를 확인했다. 구체적인 C++ 작업 타입과 내부 단계별 비중은 확인하지 않았다. 대표 최대 사례가 평균 사례를 보장하지도 않는다.

## 2. 코드·에셋 진입점

- [LNPGrappleAnchor.cpp](../../../Source/LootNPop/WorldDevice/LNPGrappleAnchor.cpp): `PrimaryActorTick.bCanEverTick=false`, Niagara `EffectComponent` 생성·부착, 상호작용 등록. Actor Tick을 끄는 제안은 이미 적용된 상태와 중복된다.
- [LNPGrappleAnchor.h](../../../Source/LootNPop/WorldDevice/LNPGrappleAnchor.h): 효과 컴포넌트와 앵커 인터페이스.
- 에셋: `/Game/WorldDevice/BP_GrappleAnchor`, `/Game/Effects/WorldDevice/NS_GrappleAnchor`.
- [월드 장치 설계 §9](../../TechDesign_WorldDevice.md#9-앵커-시각-표현과-비가시-회귀-2026-10-08): BP 배선과 이전 비표시 회귀의 원본 기록.
- [LNPGrappleAnchorVisualTest.cpp](../../../Source/LootNPop/Tests/LNPGrappleAnchorVisualTest.cpp): `LootNPop.WorldDevice.GrappleAnchor.VisualAsset`.

기존 설계 문서는 Core/Halo/Ping/Light의 CPU 에미터·무한 루프와 라이트 렌더러, 거리 컬링 비활성 상태를 기록한다. **이번 인계 작성에서 에셋 내부를 라이브 조회한 것은 아니므로** 최적화 세션에서 실제 에셋/실효 설정을 먼저 확인한다.

## 3. 별도 세션의 조사·개선 후보

| 조사 지점 | 확인할 근거 |
|:---|:---|
| 실제 활성 시스템 수 | 총 앵커 수·활성/비활성·카메라 거리·가시성별 수. 타이머 Count를 개체 수로 쓰지 않음 |
| CPU 단계 분해 | 시스템/에미터/파티클 갱신·동적 렌더 데이터·작업 수와 동기화의 비중 |
| 거리·가시성별 갱신 | 효과 중요도·갱신 빈도·컬링/재활성화·Bounds 정책과 시각 품질 |
| 에미터·렌더러별 비용 | Core/Halo/Ping/Light를 각각 분리해 CPU/GT 대기/RT/GPU 영향 측정. 한꺼번에 끈 결과로 특정 에미터를 지목하지 않음 |
| 다수 소형 작업 | 앵커별 작업 분산/배치가 작업 수·스케줄링·월드 후처리 임계 경로에 미치는 영향 |

CPU 시뮬레이션의 GPU 전환은 검토 후보일 수 있으나, 현재 Medium GPU도 약19.92ms로16.6ms 예산을 넘는다. CPU 시간 감소와GPU 비용 이동을 함께 확인해야 한다. 어느 방법도 이 문서에서 확정하지 않았다.

## 4. 보존할 동작과 검증 기준

- 과거 `EffectComponent.bAutoActivate=false`로 효과가 완전히 안 보였어도 상호작용과 그래플 이동은 동작했다. **게임 기능 정상만으로 시각 회귀가 없다고 판정하지 않는다.** 위 기존 시각 테스트는 에셋 연결·자동 활성화·가시성을 확인한다.
- 구면 월드의 여러 구역·거리·카메라 회전에서 효과 표시와 재활성화를 확인한다. 앵커 발견성·상호작용 프롬프트·그래플 이동·복제된 존재/ID를 함께 보존한다.
- 효과 개수·입자 수·라이트·갱신 빈도가 바뀌면 시각 품질 차이를 기록한다. 효과를 없애서 줄어든 부하와 같은 표현의 계산 효율 향상을 구분한다.
- 같은 패키지 측정 조건에서 GT 효과 시간, 워커 작업 합, 후처리 대기 분포, Frame/RT/GPU를 비교한다. 대표 최대 대기 한 건만으로 개선을 판정하지 않는다.
- PIE·Niagara 편집기 조작이 빠르면 사용자에게 바로 요청한다. 기존 시각 테스트와 수동 품질 확인은 역할이 다르다. 이 인계 단위에서는 새 실행·에셋 변경·테스트를 하지 않았다.

## 5. 원자료와 실행 식별

프로젝트 루트 기준 원자료:

`Saved/Profiling/Graphics/Phase00/MediumCPUTrace_20261107_102250_f65aca/`

- 트레이스: `Medium_20261107_102251_8b8bb9/CPU.utrace`
- 근거: `CpuCostBreakdown.json`, `WaitCallPathObservation.json`, `TaskRelationObservation.json`, `AnalysisValidation.json`.
- 타이머 원본: `Exports/Measured_GameThread.csv`, `Measured_AllThreads.csv`, `Threads.csv`, `Timers.csv`.
- 환경: `Session.json`, `Environment.json`, `GpuTelemetry.csv`, `DiagnosticSummary.json`.

패키지 Development·Medium 초기 적2223명·시드1·Revision3 카메라·180초 워밍업/60초 경로이며, **분석 구간은184.502898~244.502898초·GT2983프레임**이다. 전체 트레이스에는 워밍업이 포함돼 있으므로 전체 세션 통계를 쓰지 않는다. GT 전용과 전체 스레드 집계를 구분하고 중첩 포함 시간을 합산하지 않는다.

실제 종료777003(`CrashReporterCrashed`)·원인 미확정·Insights 메타데이터 경고를 보존했다. `DiagnosticOnly=true`·`BaselineEligible=false`이며 정상 종료 기준선이 아니다. `Saved/`는 로컬 git 제외 자료다. 원자료가 없으면 자동으로 긴 실행을 시작하지 않는다.

## 6. 결과를 돌려줄 때

변경한 코드/에셋과 패키지 식별값, 효과 개수·실효 설정, 시각 검증, GT/워커/대기/RT/GPU 전후 결과를 남긴다. 다른 세션의 배회·렌더 동기화·Nav 변경이 섞이면 각각의 절감량을 단독 효과로 귀속하지 않는다. 기존 Medium GT7.2288% 재현성 미통과·Phase0 미완료는 별도 검증 전까지 유지한다.

관련 인계: [GT 렌더 동기화](RenderSyncOptimizationHandoff.md), [배회 프로세서](WanderOptimizationHandoff.md), [Nav 최근접 노드](NavNearestNodeOptimizationHandoff.md).

네 개선 완료 후의 통합 검증·새 반복 측정·Phase0 마무리는 [개선 후 재개 절차](../phases/Phase00_PostOptimization.md)를 따른다.
