# GT 렌더 동기화 비용 분석·최적화 인계

> 상태: GT 대기 원인은 GPU 역압으로 확인(§6.6) · 동기화 설정 변경 기각 · GPU 패스 표 확보, 다음은 해상도(O-2)·Lumen 단계 비교
> 갱신: 2026-11-09
> 목적: 사용자가 별도 세션에서 GT의 렌더 대기와 RT/GPU 비용을 조사·개선할 근거

## 0. 진행 시점과 범위 (2026-11-08 사용자 확인)

- **시점:** 배회·그래플 앵커 세션이 끝난 뒤에 한다. 그래플 앵커의 Niagara 변경이 GPU 부하를 바꾸므로, 그 변경이 들어간 바이너리로 잰다. Nav 최근접 노드 세션과는 파일이 겹치지 않아 병행할 수 있다.
- **하는 것:** GPU 패스 분해와 CPU 대기 서명(§1 추가 근거의 세 타이머)을 같은 진단 실행에서 수집하고 GT 대기의 원인을 확정한다. 이 패스 표가 그래픽스 트랙 O-1을 충족하며, 재개 절차 R-C는 같은 바이너리·조건이면 새 GPU 트레이스 없이 이 표를 재사용한다.
- **고쳐도 되는 것:** 화질과 렌더 조건을 바꾸지 않는 낭비 제거(동기화 설정, 버그성 비용 등). 전후 수치를 남긴다.
- **미루는 것:** 적 Lumen 기여 제거·표면 캐시 예산·해상도·업스케일처럼 화질이나 렌더 조건이 바뀌는 변경. Phase 0 기준선을 엔진 기본 렌더링 상태로 지켜야 O-2·O-3의 전후 비교가 성립한다. 후보로 찾으면 근거와 예상 효과만 돌려준다([로드맵](../Roadmap.md) §4 트랙 O).

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

**추가 근거(2026-11-08):** 같은 CPU 진단 구간에서 `GPUBound_WaitingForGPUForOcclusionQueries_SeeGPUTrack` 14.148ms/프레임, `WaitForVisibilityTasks` 13.836ms/프레임이 확인됐다. 렌더 스레드가 가림 질의 결과를 받으려고 GPU를 기다리는 구간이다. 2026-09-25 선행 측정에서도 같은 순서의 서명이 있었고, 그때 GPU 패스 분해는 적 수에 비례하는 증가분 중 Lumen 장면 갱신·조명이 가장 큰 확인된 몫이라고 봤다. 따라서 GT 대기는 **GPU 처리량 부족이 역류한 대기일 가능성이 높고**, 동기화 설정보다 GPU 패스 비용을 먼저 본다. 인과는 아직 확정하지 않았다. 수치·조건 차이·후보는 [선행 렌더링 부하 측정](RenderingLoadBaseline_20260925.md)에 있다. 이후 세 최적화 실행과 편성별 정식 측정을 대조해 GPU 역압으로 판정했다(§6).

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
| GPU 패스 | 그림자·기하·머티리얼·후처리·Niagara/라이트 등 패스별 비용. 현재 어느 것도 주범으로 확정하지 않음. 첫 후보는 Lumen 장면 갱신(`MeshCardCapture` 등)·장면 조명과 해상도 비례 패스(스크린 프로브·TSR·후처리). 수집·분모 함정은 [측정 규약](../design/Measurement.md) §5 "GPU 패스 분해" |
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

## 6. 기존 자료 재집계 판정 (2026-11-08)

새 실행 없이 `Saved/Profiling/Graphics/Phase00/`의 `Frames.csv`(`CaptureFrame=1` 행 평균)와 CPU 진단의 `Measured_AllThreads.csv`를 다시 집계했다. 각 행은 서로 다른 패키지의 단일 실행이며 정식3회 중앙값이 아니다.

### 6.1 GT가 줄어도 Frame은 그대로다

| 실행(Medium, 180/60초) | Frame | GT | RT | GPU |
|:---|---:|---:|---:|---:|
| CPU 진단 `MediumCPUTrace_20261107_102250_f65aca` | 20.12 | 10.90 | 20.02 | 19.52 |
| 배회 최적화 `WanderOptimizedTrace_20261108_075540_32879c` | 20.08 | 5.72 | 20.07 | 19.55 |
| 그래플 앵커 `GrappleAnchorGTTrace_20261108_101420_99bdbe` | 20.03 | 5.89 | 20.03 | 19.53 |
| Nav 최근접 `NavHintedNearestTrace_20261108_120257_203a03` | 19.94 | 6.00 | 19.94 | 19.43 |

GT가 약 절반으로 줄었는데 Frame 변화는 0.2ms 이내다. 대기가 동기화 오버헤드였다면 Frame이 GT를 따라 줄었어야 한다. 그래플 앵커의 동적 라이트 제거 뒤에도 GPU는 변하지 않았다. 최적화 뒤 GT의 렌더 대기는 오히려 늘었을 것이다(새 트레이스에서 미확인).

### 6.2 GPU는 적 편성과 거의 무관하다

| 편성(정식 180/60초 3회) | 초기 적 | 프러스텀 후보 평균 | ISM/ISKM 인스턴스 평균 | GPU 중앙값 |
|:---|---:|---:|---:|---:|
| Low | 1053 | 약99 | 약271 | 19.93 |
| Medium | 2223 | 약294 | 약420 | 19.92 |
| High | 3257 | 약462 | 약410 | 21.74 |

인스턴스 수가 MassLOD 정원 때문에 400개대에 묶여 Low→Medium 차이가 0이다. 2026-09-25의 "적 수에 비례하는 Lumen 비용"은 720p·한 시야 최악 조건의 결론이며, 현재 1440p 기준선에서는 **기본 장면 비용이 지배적**이다. 이 문서의 첫 후보는 1440p 원본 해상도의 해상도 비례 패스로 바꾼다.

### 6.3 RT도 대부분 GPU 대기다

CPU 진단(2983프레임)에서 `FDeferredShadingSceneRenderer_Render` 18.26ms 중 `InitViews` 13.93ms, 그 안의 `RHIGetRenderQueryResult_GPU_Wait` 14.15ms다. RT의 실제 작업은 약4~6ms로 본다. CSV의 `RTMs`(`GRenderThreadTime`)는 이 대기를 포함하므로 RT 비용으로 읽지 않는다.

엔진의 `r.NumBufferedOcclusionQueries` 기본값은1이다(`ConsoleManager.cpp`). RT는 프레임 N의 가시성에서 N−1 가림 질의 결과를 기다리므로(`SceneVisibility.cpp` `WaitForLastOcclusionQuery`), 14ms 대기는 GPU가 직전 프레임의 가림 단계에도 도달하지 못했다는 뜻이다. Frame과 GPU의 차이가 약0.5ms라 GPU 측정 구간 밖의 유휴도 작다.

`IDXGIOutput::WaitForVBlank_NoBusyWait` 20.1ms/프레임은 60초에 3598회(60Hz)인 별도 VBlank 대기 스레드이며 원인이 아니다.

### 6.4 결론과 기각한 변경

- **동기화 설정(`r.OneFrameThreadLag`·`r.GTSyncType`·`r.NumBufferedOcclusionQueries`)은 바꾸지 않는다.** 예상 Frame 변화는0이고, 대기가 Present·Sync 쪽으로 옮겨 가며 입력 지연과 가림 결과 지연만 늘어난다. `NumBufferedOcclusionQueries=2`는 이 판정을 확인하는 진단 요인으로만 쓴다(§6.5 D4).
- 개선 대상은 약19.5ms의 기본 장면 GPU 비용이다. 해상도·업스케일·적 Lumen 기여 변경은 §0대로 O-2·O-3에서 다룬다.
- 패스 표가 나오면 볼 화질 중립 후보(미검증): 레이 트레이싱 동적 지오메트리 갱신(RT CPU `RayTracingDynamicGeometry` 1.77·`RayTracing_FinishGatherInstances` 1.10·`RayTracing_GatherDynamicRayTracingInstances` 0.79·`BuildAccelerationStructure_BottomLevel` 0.78ms/프레임의 GPU 짝), VSM 캐시 무효화(WPO·움직이는 그림자 캐스터), 편성 무관 고정 비용인 Pod 빔 ISM 120개(Unlit·Additive), Mass 가시성 변화로 인한 인스턴스 제거·재추가와 Lumen 카드 재캡처(낮은 우선순위 — Low/Medium GPU가 같다).

### 6.5 GPU 패스 진단 준비

하네스와 런처에 진단 경로를 추가했다(게임 타깃 빌드 성공, 패키지 미생성).

- 측정 구간 trace region `LNPGraphicsCapture`: 캡처 시작에 열고 CSV 종료 또는 중단 시 닫는다. GPU 내보내기는 `-region="LNPGraphicsCapture"`를 쓴다.
- `-LNPGraphicsDiagnosticCVars="이름=값;이름=값"`: 하네스 기본 렌더 조건 뒤에 덮어쓴다. 하네스가 `r.ScreenPercentage` 등을 콘솔 우선순위로 다시 쓰므로 `-ExecCmds`로는 이 요인이 적용되지 않는다. 없는 이름은 실행을 실패시킨다. 지정한 회차는 `Run.json`의 `diagnostic_cvars`에 남고 `performance_eligible=false`다.
- `RunGraphicsBenchmark.ps1 -ExtraArguments`: 추가 실행 인자를 넘기고 `Launch.json`에 기록한다. 회차 폴더는 `{RunDirectory}`로 쓴다.

진단 세트(모두 Medium·180초 워밍업/60초·같은 새 패키지·같은 트레이스 인자 `-trace=cpu,gpu,frame,bookmark,region,log -tracefile="{RunDirectory}/Diag.utrace"`). 트레이스 오버헤드를 회차 사이에 같게 두어 요인끼리만 비교하고, 정식 기준선과는 섞지 않는다.

| 회차 | 진단 CVar | 확인할 것 |
|:---|:---|:---|
| D0 | 없음 | O-1 GPU 패스 표, 같은 실행의 CPU 대기 서명(§1의 세 타이머) |
| D1 | `r.ScreenPercentage=67` | 해상도 비례 몫(O-2 근거) |
| D2 | `r.ScreenPercentage=50` | 위와 함께 비례/고정 비용 분리 |
| D3 | `ShowFlag.Translucency=0` | 반투명 전체 비용 = Pod 빔 비용 상한(스캔 시야 대체 판단 근거) |
| D4 | `r.NumBufferedOcclusionQueries=2` | 대기 위치만 바뀌고 Frame·GPU가 그대로인지(§6.4 확인) |

D1~D4는 화질이나 지연이 바뀌는 조건이라 결과를 개선으로 채택하지 않는다.

### 6.6 진단 결과 (2026-11-09)

패키지 `Saved/Graphics/RenderSyncDiagPackage_20261108`(HEAD `0e433b8` + 이 문서의 하네스·런처 변경, 패치는 같은 폴더 `SourcePatch.diff`). 원자료 `Saved/Profiling/Graphics/RenderSyncDiag_20261108/D0~D4/`, 회차마다 `Exports/GPU.csv`·`AllThreads.csv`. 다섯 회차 모두 `status=complete`이고 종료 코드는 기존 trace 실행과 같은777003이다(정상 종료로 표시하지 않고 보존). 진단 CVar 적용은 각 `Runtime.log`의 `Diagnostic CVars` 줄로, D3의 반투명 꺼짐은 `RenderTranslucency` 패스가0이 된 것으로 확인했다.

**프레임(CSV 측정 구간 평균, ms)**

| 회차 | Frame | GT | GPU | Frame P95 | 프레임 수 |
|:---|---:|---:|---:|---:|---:|
| D0 기준 | 20.33 | 5.75 | 19.82 | 21.25 | 2951 |
| D1 SP67 | 13.14 | 5.53 | 12.63 | 14.05 | 4565 |
| D2 SP50 | 10.81 | 5.62 | 10.25 | 11.64 | 5548 |
| D3 반투명 끔 | 21.19 | 5.34 | 20.69 | 22.42 | 2833 |
| D4 가림 버퍼2 | 21.36 | 6.13 | 20.87 | 22.28 | 2810 |

**CPU 대기 서명(Insights 같은 구간 포함 시간 ÷ 프레임 수, ms)**

| 타이머 | D0 | D1 | D2 | D3 | D4 |
|:---|---:|---:|---:|---:|---:|
| `Sync_RenderingThread` | 14.60 | 7.63 | 5.21 | 15.87 | 15.24 |
| `GPUBound_WaitingForGPUForOcclusionQueries_SeeGPUTrack` | 16.14 | 9.11 | 6.78 | 17.26 | 16.47 |
| `WaitForVisibilityTasks` | 15.82 | 8.80 | 6.46 | 16.96 | 16.24 |

**GPU 주요 패스(D0, ms/프레임 — `Incl ÷ GPU Frame Count`, Measurement.md §5 분모 규약)**

| 패스 | 포함 | D1 | D2 | 비고 |
|:---|---:|---:|---:|:---|
| Lumen `DiffuseIndirectAndAO`(스크린 프로브 4.15 포함) | 4.68 | 2.37 | 1.54 | 해상도 비례 |
| `PostProcessing`(TSR 3.43 포함) | 3.91 | 2.59 | 2.19 | TSR은 출력 1440p 고정분이 남는다 |
| `SceneRender` 자기 시간(이름 없는 구간) | 3.10 | 2.42 | 2.25 | 하위 패스에 속하지 않는 몫, 미분해 |
| Lumen 반사·간접광 `DiffuseIndirectAndAO #2` | 2.79 | 1.51 | 1.00 | |
| `RenderDeferredLighting #2`(직접광·VSM 투영 0.73) | 1.84 | 1.05 | 0.81 | |
| `UpdateWorldRadianceCaches` | 1.33 | 0.77 | 0.60 | |
| `LumenSceneLighting` | 1.07 | 0.85 | 0.77 | 거의 고정 |
| `BasePass` | 0.46 | 0.36 | 0.33 | |
| `ShadowDepths` | 0.19 | 0.15 | 0.14 | |
| 반투명(`Translucency`) | 0.19 | 0.06 | 0.04 | D3에서0 |
| `RayTracingDynamicGeometryUpdate` | 0.21 | 0.19 | 0.20 | |

**판정**

1. **해상도가 가장 큰 지렛대다.** 화소 비율(1.0/0.449/0.25)에 GPU 합을 최소제곱으로 맞추면 고정 약7.5ms + 화소 비례 약12.8ms(네이티브 기준)다. 렌더 해상도 67%에서 Frame 13.14ms로 60fps 예산(16.6ms) 안에 든다. 화질 변화가 있으므로 채택 여부·기본값·옵션 노출은 O-2에서 사용자가 정한다.
2. **GT 대기는 해상도와 함께 줄었다.** GT 작업은 5.3~6.1ms로 그대로이고 `Sync_RenderingThread`만 14.6→7.6→5.2ms로 줄었다. GT 대기가 GPU 역압이라는 §6.4 판정을 직접 확인했다. 이 Frame에서의 대기 14.6ms(§1의9.27ms보다 큼)는 GT 작업이 줄어든 만큼 대기가 늘어난 것이다(Frame ≈ GT 작업 + 대기).
3. **가림 버퍼2(D4)는 기각을 확인했다.** RT의 가림 대기 16.5ms, GT 대기 15.2ms가 거의 그대로이고 Frame도 줄지 않았다.
4. **Pod 빔 비용 상한은 약0.3ms다.** 반투명 패스·반투명 속도 벡터를 합쳐 D0 기준 약0.3ms(1.5%)이며, D3 전체 Frame은 오히려 0.9ms 늘었다(아래 드리프트). 성능을 이유로 Pod 빔을 스캔 시야로 바꿀 근거는 없다. 대체 여부는 게임 디자인으로 정한다.
5. **동적 RT 지오메트리 GPU 몫은 작다(0.21ms).** §6.4의 화질 중립 후보 중 RT 동적 지오메트리는 GPU 쪽 이득이 작고, CPU(RT 스레드) 쪽 1.8ms만 남는다.

**드리프트 주의.** 실행 순서는 D0→D1→D2→D3→D4이고, D3·D4는 거의 모든 패스가 D0보다 4~5% 균일하게 느리다(TSR 3.43→3.91, `SceneRender` 자기 3.10→3.30). 특정 패스가 아니라 전체가 같은 비율로 커진 모양이라 노트북 GPU의 클럭·온도 드리프트로 본다. 따라서 이 세트에서 ±1ms(약5%) 이하 차이는 판정하지 않는다. 1·2·3번 판정은 이 폭보다 훨씬 크다.

**다음 후보(미검증)**

- O-2 해상도: 67%·75%와 TSR 품질 단계를 화질 비교와 함께 정식 3회 측정.
- Lumen 비용(위 표 `DiffuseIndirectAndAO` 두 항목만 7.5ms, 월드 래디언스 캐시·장면 조명과의 중첩 관계는 미확인, 해상도 비례가 대부분): `sg.GlobalIlluminationQuality`·`sg.ReflectionQuality` 단계와 스크린 프로브 다운샘플을 같은 진단 경로로 분리.
- `SceneRender` 자기 시간 3.1ms: GPU 트랙 계층을 Insights GUI로 열어 이름 없는 구간의 위치를 확인(선행 측정 §6 3번의 미설명 몫과 같은 대상인지).
