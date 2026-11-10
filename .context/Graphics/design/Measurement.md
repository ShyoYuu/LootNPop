# 그래픽스 측정 규약

> 상태: 0-C에서 실행·집계 검증, 0-D 정식 반복 측정 대기
> 마지막 갱신: 2026-11-10

## 1. 목표 사양 (GX-002)

| 항목 | 값 | 상태 |
|:---|:---|:---|
| GPU | NVIDIA GeForce RTX 4060 Laptop GPU (iGPU Radeon 780M 병존) | 확정 |
| 출력 해상도 | 2560×1440 | 확정 |
| 내부 해상도·업스케일러 | 측정 기준은 TSR·ScreenPercentage100(네이티브). 예산 미달 시 렌더 해상도 유지 후보(TSR 히스토리·Lumen) → 77% → 67% 순, 업스케일러는 TSR만(GX-012). O-2 분해: GPU ≈ 고정 6.4ms + 화소 비례 13.3ms(네이티브), 16.6ms 손익분기는 렌더 해상도 약 85%([단위 O-2 로그](../history/UnitO2_Log.md)) | 0-C 구현·패키지 실효값 확인 / 운영 조건 확정(2026-11-10, GX-012) |
| 프레임 예산 | 60fps = 16.6ms | 확정 |
| 적 수 목표 | 화면 안·전체 적 수를 Phase 0 기준선을 보고 정함 | 미정 |

수치는 항상 이 예산 대비로 보고한다. "몇 ms 줄였다"보다 "16.6ms 예산 중 어느 항목이 몇 % 초과하던 것을 맞췄다"로 쓴다.

## 2. 측정 원칙 (GX-001)

- **패키지 빌드에서만 판정한다.** 에디터 `-game`은 `WITH_EDITOR` 코드와 디버그 드로우가 쌓여 오염된다. 에디터 PIE 수치는 경향을 볼 때만 쓰고 보고서에 넣지 않는다.
- **고정 카메라 경로와 고정 시드 편성.** 같은 프레임을 비교할 수 있어야 전후 수치를 믿을 수 있다.
- **3회 반복, 중앙값 보고.** 편차가 기준을 넘으면 그 회차를 버리지 말고 원인(써멀·백그라운드)을 먼저 찾는다.
- **한 번에 한 요인만 바꾼다.** 절제 측정과 분모 규약은 [Guide_NetBandwidth.md](../../Guide_NetBandwidth.md)의 방식을 그대로 쓴다. 여러 요인은 요인설계로 묶어 측정 횟수를 줄인다.
- 측정 편성은 별도 에셋으로 쓴다. 하네스가 활성일 때는 `LNP.Graphics.Benchmark.Profile` 또는 `-LNPGraphicsProfile=`로 선택하며 일반 PIE의 편성 오버라이드는 `LNP.Spawn.ConfigOverride`다. 기본 `DA_MassSpawnConfig`는 바꾸지 않는다.
- Phase 0 편성 단계는 Low·Medium·High로 구분하고 **실제 초기화된 적 수**를 표기한다(2026-11-07 사용자 선택). 일반 편성 산술값과 authored 편성이 반영된 계획 요청·계획 배치·실제 초기화를 구분한다. 1k·3k·5k를 맞추려고 기존 스폰 밖에서 부족분을 채우지 않는다. 각 시점의 생존 수·화면 가시 수·활성 수는 초기 생성 수와 별도 지표다.

## 3. 노트북 측정 함정

| 함정 | 대응 |
|:---|:---|
| 배터리 구동 시 GPU 전력 제한 | 전원 어댑터를 연결하고 측정한다 |
| Windows·제조사 전원 모드에 따라 클럭이 달라짐 | 측정마다 같은 성능 모드를 고정하고 보고서에 기록한다 |
| Optimus(iGPU + dGPU)로 게임이 iGPU에서 돌 수 있음 | 실행 로그의 RHI 어댑터 이름으로 dGPU 사용을 확인한다 |
| 디스플레이가 연결된 GPU에 따라 화면 전달 경로가 달라짐 | 어떤 GPU가 디스플레이를 구동하는지 기록한다(같은 1440p라도 165Hz·59Hz로 다르게 보고됨) |
| 써멀 스로틀링 | 측정 전 워밍업 구간을 두고, 측정 중 GPU 클럭·온도를 함께 기록한다 |
| 수직 동기·프레임 제한 | 측정 시 끈다(`r.VSync 0`, `t.MaxFPS 0`) |

## 4. 지표와 도구

| 지표 | 의미 | 주 도구 |
|:---|:---|:---|
| Frame / Game / Draw(RT) / GPU ms | 병목 위치 판정 | `stat unit`, CSV 프로파일러 |
| 패스별 GPU ms | 어떤 렌더 패스가 비싼지 | `stat gpu`, `ProfileGPU`, Insights GPU 트랙 |
| 드로우콜·프리미티브 수 | 인스턴싱·슬롯 분리 효과 | `stat rhi`, `stat scenerendering` |
| VRAM·텍스처 스트리밍 | 콘텐츠 투입 후 메모리 압박 | `stat streaming`, `rhi.DumpMemory` 계열 |
| 프레임 단위 분해 | 한 프레임의 패스·리소스를 직접 확인 | RenderDoc |
| 스레드별 시간선 | GT·RT·워커 스레드의 대기·병목 | Unreal Insights |

### Phase 0 하네스 실행

하네스는 기본 비활성이다. 일반 플레이의 편성·시드 경로는 유지하며 활성 실행에서만 시드1·PCG 오프셋0·선택 편성·밀도1을 사용한다. 렌더링 조건은 에픽 프리셋·TSR·ScreenPercentage100·동적 해상도/VSync/프레임 제한 해제다. 실행 중 적용한 CVar 값은 종료 때 복구하고, 실제 적용 값은 회차별 `Run.json`에 남긴다. 프로젝트의 더 높은 우선순위 설정 때문에 프리셋의 일부 하위 CVar가 바뀌지 않을 수 있으므로 프리셋 이름만으로 렌더러 설정을 추정하지 않는다.

| 용도 | 명령·진입점 |
|:---|:---|
| 패키지 생성 | `Scripts/Profiling/Graphics/PackageGraphicsBenchmark.ps1` — Game Development build/cook/stage/archive. UBT·cook 외부 캐시 쓰기를 위해 처음부터 권한 확장으로 실행한다 |
| 한 단계 실행 | `Scripts/Profiling/Graphics/RunGraphicsBenchmark.ps1 -Profile Low -Repeats 1` |
| 현재 공통 조건의 정식3회 실행 | 위 명령에 `-Repeats 3 -WarmupSeconds 180 -DurationSeconds 60`을 명시한다. 각 편성은 사용자 진행 확인 뒤 별도로 실행하며 기존60초 워밍업 세트와 분리한다 |
| 짧은 패키지 기능 검증 | `-WarmupSeconds 5 -DurationSeconds 6`을 추가한다. 성능 판정에서는 제외한다 |
| 결과 집계 | `Scripts/Profiling/Graphics/AnalyzeGraphicsBenchmark.ps1 -RunDirectory <Run.json이 있는 폴더>` |
| 예비 실행 집계 | 집계 명령에 `-AllowPreview`를 추가한다. 기본 집계는 예비 실행을 거부한다 |
| PIE 미리보기 | PIE 전에 `LNP.Graphics.Benchmark 1`, `LNP.Graphics.Benchmark.Profile Low` 설정. `LNP.Graphics.Benchmark.WarmupSeconds`·`DurationSeconds`로 짧게 조절할 수 있다. PIE 종료 후 Benchmark를0으로 복구한다 |

패키지 실행 파일은 `Saved/Graphics/Phase00Package/Windows/LootNPop/Binaries/Win64/LootNPop.exe`다. 런처는 DX12·단일 플레이어로 실행하고, 시드·편성·카메라 준비 → 워밍업 → CSV 시작 확인 → 경로 재생 → CSV 쓰기 완료 순서를 기다린다. 경로는 60초 기본값에서 A(하부 상향) → B(상부 조망) → C(근거리) → A로 선형 위치·쿼터니언 구면 보간한다. 워밍업도 같은 경로를 돌린다. 카메라 시점과 구도는 사용자 육안 검증이 필요하다.

이전 경로(Revision2)는 사용자 확인에서 세 구간 모두 관통 없이 통과했지만, Low의3번 구도에서 Pod가 보이지 않아 사용자 재선택 카메라로3번을 교체했다(Revision3, 2026-11-07). 신규 Low/Medium/High 패키지 재생 모두에서 사용자가 Pod·NPC와 무관통을 확인했다. 구도 확인만 빠르게 반복할 때는 `-WarmupSeconds 0 -DurationSeconds 30`을 사용한다. 이 경우 구간당10초이며 성능 기준선으로 쓰지 않는다. 워밍업이 있으면 해당 경로를 미리 돌고 촬영 시작 때1번으로 복귀한다.

경로 JSON은 NonUFS 파일이므로 카메라만 바꿀 때는 기존 파일을 보존한 뒤 패키지의 `Windows/LootNPop/Scripts/Profiling/Graphics/AcropolisViewpoints.json`에 같은 파일을 복사하고 해시를 확인할 수 있다. 런처는 이 스테이징된 파일을 읽는다. 실행 파일·cook 콘텐츠를 재빌드한 것과 구분해 기록하고, 경로 해시가 다른 회차는 같은 조건으로 묶지 않는다.

고정 시드는 같은 편성·베이크의 초기 계획을 재현한다. 편성별 적 배치 제약이 랜덤 Pod 후보 채택에 영향을 주므로 Low/Medium/High 사이의 Pod 위치 일치까지 보장하지 않는다. 구도를 잡은 임의 시드·밀도0 PIE와 실제 벤치마크의 군중 분포도 따로 확인한다. Revision3의3번은 시드1022·밀도0·Acropolis slot6에서 기록했으며 벤치마크 시드1·밀도1의 세 편성에서 Pod·근거리 NPC 화면 조건을 별도 확인했다. 이 확인은 편성 간 Pod 좌표가 같다는 검증이 아니다.

관찰자 원래 폰은 숨기고 정지시키며 입력·HUD를 끈다. 적의 인지 대상에서 관찰자를 제외하고, 활동 대역의 거리 원점을 카메라로 대체한다. 배회·애니메이션을 유지하는 **관찰 부하**이며 추격·공격·피격이 섞인 전투 부하와 구분한다. MassLOD는 엔진 기본값의 카메라 위치를 사용한다(`bUsePlayerPawnLocationInsteadOfCamera=false`).

### 산출물·지표 의미

런처 기본 출력은 `Saved/Profiling/Graphics/Phase00/Runs/`다. 회차 폴더에 `Launch.json`(실행 인자·실행 파일/스테이징된 경로 파일/패키지 컨테이너 SHA256)·`Runtime.log`·`Exit.json`이 있고, 그 아래 하네스가 만든 폴더에 다음을 쓴다. 컨테이너는 `.pak`·`.utoc`·`.ucas`를 모두 기록한다.

- `Run.json`: 상태·실제 설정·출력 크기·GPU·시드·옥탄트 배정·초기 EntityConfig/slot별 요청/초기화/거부 수. 여기의 요청은 **공간 배치 계획 뒤** 스폰 요청이며, 계획 자체의 미배치는 `Runtime.log`와 별도로 대조한다.
- `Viewpoints.json`: 실제로 읽은 로컬 경로 원문 사본.
- `Frames.csv`: 엔진 CSV 프로파일러 원자료. 기본 카테고리의 `LNPGraphics_` 접두사 열을 쓴다. `CaptureFrame=1` 행만 측정 구간이다. 시작·쓰기 종료용 행을 섞지 않는다.
- `Population.csv`: 1Hz 생존 표본. EnemyConfig·현재 Support slot·Dormant/Wander/Active·Actor/인스턴스/None·프러스텀 후보를 기록한다. Support를 잃었거나 동적 지지면에서 slot이 없으면−1이다.
- `Summary.json`: 검증 후 프레임/GT/RT/GPU/드로우콜 평균·Frame p95·초기 종류별 합계를 쓴다. 여러 회차의 중앙값·편차와16.6ms 예산 비교는 정식 반복 결과를 모아 보고한다.

CSV 열의 의미는 다음과 같다.

| 열 접미어 | 원본·해석 |
|:---|:---|
| `FrameMs` | 월드 틱 DeltaTime의 ms. 기본 시간 배율·가변 프레임 조건에서 읽는다 |
| `GTMs`, `RTMs` | 엔진 `GGameThreadTime`, `GRenderThreadTime`의 cycle→ms. 유휴 시간을 제외한 엔진 시간이며 하네스 함수 자체의 실행 시간만 잰 값이 아니다 |
| `GPUMs` | `RHIGetGPUFrameCycles()`의 cycle→ms. GPU에서 비동기로 해결된 최근 프레임 값이므로 카메라의 같은 프레임 번호와 일대일 정합한다고 해석하지 않는다 |
| `DrawCalls` | RHI가 완료한 `GNumDrawCallsRHI[0]`. 초기 원자0이 측정값에 들어가면 집계가 실패한다 |
| `Alive`, `Active`, `FrustumCandidates`, `Actors`, `Instances` | 직전1Hz 표본을 다음 표본까지 유지한 값. 프레임마다 전체 엔티티를 다시 세지 않는다 |

프러스텀 후보는 보수적인 몸 구의 카메라 프러스텀 교차 수다. 가려짐·실제 렌더링·픽셀 점유율을 판정하지 않으므로 **실제 가시 NPC 수로 부르지 않는다.** `Instances`는 ISM·ISKM 표현 상태를 합친 수이며 화면 가시 여부와 별개다. 초기 생성 수·현재 생존 수·활성 수·표현 상태·프러스텀 후보를 서로 대체해 쓰지 않는다.

집계기는 엔진의 다른 통계에 중복 열 이름이 있어도 `LNPGraphics_` 열 인덱스를 직접 찾는다. 측정 행 수=메타데이터 프레임 수, 필수 시간/드로우콜의 유효한 양수, 종류별 초기화 합=초기 합계, 활동/표현 분할 합=생존 수를 검사한다. 누락된 GPU 시간이나0을 평균에 섞지 않는다.

PIE·에디터 `-game`·60초 미만 워밍업/캡처 또는1440p가 아닌 출력은 `performance_eligible=false`로 기록한다. 이 필드가 true여도 전원·써멀·어댑터·동일 바이너리·3회 편차 확인을 대신하지 않는다. 트레이스와 RenderDoc 녹화는 기본 실행 인자에 없으며 원인 분석용 별도 실행에서 추가한다.

### GPU 환경 기록

0-D 준비에서 `nvidia-smi --query-gpu=timestamp,index,name,pstate,temperature.gpu,clocks.current.graphics,clocks.current.memory,power.draw,utilization.gpu --format=csv -l 1`의1초 간격CSV 저장을 확인했다. 측정 시에는 별도 숨김 프로세스로 워밍업부터 종료까지 기록하고 성공·실패 모두에서 해당 기록 프로세스를 종료한다. `Run.json`·실행 로그의 시각과 대조할 때 타임존을 맞춘다. 이 환경의 nvidia-smi 시각은KST이며 확인 근거는 `Saved/Profiling/Graphics/Phase00/FormalPreparation_20261107/GpuTelemetryProbe.{csv,json}`이다.

전력 상한 질의는 이 노트북에서N/A였다. GPU 온도·그래픽/메모리 클럭·전력·사용률 기록 가능 여부와 별도로 남긴다. `powercfg /getactivescheme`의 전원 계획만으로 Windows 설정의 전원 모드나 ASUS Armoury Crate의 작동 모드를 확정하지 않는다. 사용자 확인 값과 사용 화면·화면 연결 GPU도 회차 환경 기록에 포함한다.

워밍업 길이가 다른 세트는 별도로 집계하고 각 `Session.json`·`Run.json`에 실제 시간을 남긴다. 워밍업 중에도 AI 배회가 진행되므로 시간을 늘리면 열/클럭뿐 아니라 측정 시작 시 활동 상태도 달라질 수 있다. 편차가 줄었다는 사실만으로 열 안정화 원인이 해결됐다고 단정하지 않는다. 동일 조건의 편성 비교를 하려면 워밍업 시간도 맞춘다.

**회차 사이 냉각 대기(2026-11-09 채택).** 이 노트북은 지속 부하 약 5분 뒤 GPU 전력이 약 92~95W → 78~81W로 내려가 클럭이 함께 떨어진다. 그래서 회차를 연달아 돌리면 뒤 회차일수록 Frame·GPU가 늘어난다. 정식 반복은 회차를 1회씩 따로 실행하고, 각 회차 전에 GPU가 온도 ≤55°C·전력 ≤15W를 60초 연속 유지할 때까지 기다린다. 2회차부터는 최소 300초를 기다리고, 1200초가 지나면 조건을 못 맞춰도 진행하되 그 사실을 기록한다. 대기 시간과 시작 온도·전력은 `Session.json`의 `Runs`에 남긴다. 회차당 측정은 워밍업 180초를 포함해 약 4분이라 전력이 내려가기 전에 끝난다. 연속 실행 세트와 냉각 대기 세트는 섞어 집계하지 않는다.

초기 `initial_spawns` 비교가 보장하는 범위는 종류/slot별 요청·초기화·거부 수량의 일치다. 현재 로컬 요약의 `SameInitialPlansAndCounts`·`SameInitialPlans` 같은 이름도 이 수량 범위로 해석하며 개별 배치 위치 해시 검증을 뜻하지 않는다. 초기 수량·현재 생존·활동 상태·프러스텀 후보는 각각 따로 대조한다.

## 5. 보고 형식

### CPU 진단 구간 분리

2026-11-07 Medium 별도 진단에서 패키지 인자 `-trace=cpu,frame,bookmark,log,task -tracefile="<회차 폴더>/CPU.utrace"`를 사용했다. 전체 트레이스에는 시작·워밍업이 포함되므로 전체 세션 통계를60초 측정 통계로 쓰지 않는다. Insights Log에서 `[GraphicsBenchmark] Capture started`의 Time을 확인하고 그 시각부터60초만 분석한다. 이번에는 사용자 확인184.502898초를 시작으로 사용했다.

공식 `UnrealInsights.exe -OpenTraceFile=<trace> -ExecOnAnalysisCompleteCmd=@=<응답 파일>`로 분석 완료 후 내보낼 수 있다. 응답 파일은 명령마다 실제 줄바꿈을 넣는다. 검증한 명령은 `TimingInsights.ExportTimerStatistics "<출력 CSV>" -threads=GameThread -startTime=184.502898 -endTime=244.502898 -sortBy=TotalInclusiveTime`이다. 같은 구간 전체 스레드와20초 구간별 통계도 보존한다. 이번 구간별 내보내기는 `-NoUI -AutoQuit` 조합에서 분석 전 창 종료로 파일이 생성되지 않아 두 플래그를 뺀 GUI 실행으로 완료했다. 종료 코드0만으로 내보내기 성공을 판단하지 않고 파일·분석 완료 로그를 확인한다.

통계의 `Incl`·`Excl`은 초 단위 총합이며, 프레임당 비용은 같은 구간 `FEngineLoop::Tick` 횟수로 나눈다. 이번 전체60초는2983회다. 포함 시간은 하위 타이머와 겹치므로 합산하지 않는다. 렌더 동기화·작업 대기를 구분하고 엔진 CSV의 GT 시간과 전체 프레임 벽시계 시간을 동일시하지 않는다. 경계에 걸친 범위가 있어20초 구간 횟수의 합과 전체 횟수가 다를 수 있다.

진단은 `DiagnosticOnly=true`·`BaselineEligible=false`로 기준선에서 분리한다. 하네스의 `performance_eligible=true`만으로 트레이스 오버헤드·비정상 종료·분석 경고를 무시하지 않는다. 원본 실행 종료 코드와 트레이스 해석 가능 여부를 각각 기록하며, 단일 진단으로 다른3회 실행 간 편차의 원인을 확정하지 않는다.

호출 경로를GUI로 확인할 때는 Timing 타임라인에 포커스를 두고 `Ctrl+T`의 `Time`에 시작 시각(단위 `s` 명시), `Time Range Duration`에 측정 길이를 입력하고 `Select Time Range`·`Zoom Timing View`를 켠다. `CPU/GPU` 메뉴에서 `GPU Track(s)`·`CPU Thread Tracks`를 끈 뒤 `GameThread (1)`만 켜면 해당CPU 스레드의 통계로 좁힐 수 있다. Timers에서 타이머를 선택하고Callers를 펼쳐 상위 실행 문맥을 확인한다. 같은 타이머의 호출 수를 기존 구간CSV와 대조하며, 화면의 반올림 시간은 근삿값으로 기록한다. Callers는CPU 범위의 포함 관계이므로 실제 대기 대상 작업ID·임계 경로를 확정하려면 별도의Tasks 관계 확인이 필요하다.

대표 작업 관계를 확인할 때는 Timers의 `Find Instance → Maximum Duration Instance in Selection`으로 기존 측정 구간 안의 이벤트를 선택하고, 상위CPU 범위를 먼저 확인한다. 연결 대상이 숨겨졌으면 필요한CPU 워커 트랙을 켠 뒤 이벤트 툴팁의 대기 작업ID·작업 시작/완료 시각을 대조한다. Tasks Overview의 선행 이벤트를 선택하고 `F`를 누르면 해당 짧은 실행 구간을 확대할 수 있다. 최대 사례는 전형적인 사례를 보장하지 않는다.

UE5.8의 `FTasksProvider::TryGetWaiting`은 `WaitUntilTasksComplete`·`GameThreadWaitForTask`·`Tasks::Wait`·`Tasks::BusyWait`만 대기 작업으로 조회한다. `WaitForTasks`·`ProcessUntilTasksComplete`는 이 목록에 없으므로 직접 관계가 표시되지 않는 것을 트레이스 누락으로 단정하지 않는다. 대기 관계 표시는 최대30개 작업이며 숨겨진 트랙도 있을 수 있다. 특히 `ProcessUntilTasksComplete`는GT에서 실행한 작업을 포함하므로 전체 포함 시간을 순수 대기 시간으로 쓰지 않는다. 작업의 선행 관계와 해당 틱 완료 대기가 기다린 전체 작업 목록도 구분한다.

개별CPU 이벤트의 분포를 구할 때는GT 전용·같은 측정 선택에서 필요한 타이머를 각각 `Export Timing Events (Selection)`으로 저장하고, 스레드·타이머ID·호출 수·구간 경계·깊이를 기존 통계와 대조한다. 부모/자식 이벤트는 시간 포함과 깊이+1로 연결하며, 자식끼리 겹치거나 부모에 배정되지 않는 이벤트를 조용히 제외하지 않는다. 경계에 걸친 이벤트가 있으면 잘라 집계하는 규칙을 별도로 검증한다. 현재Wander 자료에는 경계 교차가 없다.

GUI 대신 명령줄로도 개별 이벤트를 내보낼 수 있다(2026-11-08 배회 전후 비교에서 검증).
- 응답 파일 한 줄: `TimingInsights.ExportTimingEvents "<csv>" -threads="GameThread" -timers="LNPEnemyWanderProcessor*,FEngineLoop::Tick,LNPWander_*" -columns="ThreadName,TimerName,StartTime,EndTime,Duration,Depth"`. `-timers`는 와일드카드를 받으므로 접미사가 붙는 프로세서 타이머도 잡힌다.
- 실행은 `UnrealInsights.exe -OpenTraceFile=<trace> -ExecOnAnalysisCompleteCmd=@=<응답 파일>`로 한다. `-NoUI -AutoQuit`를 붙이면 분석 전에 끝나 파일이 생기지 않는다. 그래서 창 모드로 띄우고, 로그에 `Exported N timing events`가 찍히면 프로세스를 끝낸다.
- 응답 파일 경로를 `@="<경로>"`처럼 따옴표로 감싸면 따옴표까지 경로로 읽혀 `Failed to open the response file`로 끝난다. 공백 없는 경로를 따옴표 없이 쓴다. 응답 파일 안의 출력 경로는 따옴표 안의 역슬래시가 이스케이프로 사라지므로 슬래시(`D:/...`)로 쓴다. 두 실수 모두 Insights 창은 정상으로 남아 있어, 대기 판정은 출력 파일이 아니라 로그의 `Commands executed in` 줄로 하고 그 뒤 파일 유무를 본다.
- 측정 구간은 손으로 고른 시각 대신 정렬로 정한다. 프레임당 Wander가 1회라는 전제에서 CSV 캡처 행과 이벤트 열을 이동량별로 대조한다. 이동량을 고를 때는 계수–시간 상관을 쓴다. 이를 독립적으로 검증할 때는 `LNPWander_WorldSeconds` 증분과 이벤트 시작 시각 증분의 상관을 쓴다. 맞는 위치에서만 상관이 0.95 이상이고, 한 프레임만 어긋나도 0 근처로 떨어진다. 스파이크가 없어진 실행에서는 계수 상관이 0.36까지 낮아지므로 증분 상관 검증을 꼭 함께 남긴다.
- 스크립트: `Saved/Profiling/Graphics/Phase00/WanderOptimized_20261108/AnalyzeWander.py`·`CheckAlignment.py`. 이 경로로 기존 수동 내보내기의 기준선 수치를 그대로 재현했다.

이벤트P50/P95/P99는 오름차순 자료의 `ceil(p×N)`번째 값(nearest-rank)이며 분모는 이벤트 실행 횟수다. 프레임당 비용의 분모와 구분한다. Insights의 기존 `I.Med`·`E.Med`는UE5.8 `TimelineStatistics`의100칸 히스토그램 기반 추정값이므로, 개별 이벤트의 정렬 분위수와 같다고 가정하지 않는다. 기존3회 기준선 중앙값 규약은 바꾸지 않는다.

기본 내보내기의 시작/종료 시각은 `AppendValueAsTimestamp`의 `%.9g` 형식이다. 현재184~244초 구간은1μs 격자로 반올림되므로 짧은 자식 질의를 많이 더할 때 오차가 누적될 수 있다. 타이머 합계와 원본 집계의 차이를 기록하고, 개별 실행의 자식 합계/잔여 시간은 유도값으로 표시한다. 질의 수K개를 합산한 보수적 오차 한계는 자식 합계Kμs·부모에서 뺀 잔여 시간(K+1)μs다. 잔여 시간을 내부의 특정 코드 단계 비용으로 귀속하지 않는다. 고비용 구분용 임계값은 설명용이며 새 성능 통과 기준으로 쓰지 않는다.

### GPU 패스 분해

패스별 GPU 비용은 성능 실행과 분리한 진단 실행에서 `-trace=cpu,gpu,frame,bookmark,region,log`로 수집한다. 2026-09-25 선행 측정([선행 렌더링 부하 측정](../analysis/RenderingLoadBaseline_20260925.md))에서 검증한 내보내기는 응답 파일 한 줄 `TimingInsights.ExportTimerStatistics "<csv>" -threads="GPU*" -region="<캡처 region>" -sortBy=TotalInclusiveTime`이며, 당시에는 `-NoUI -AutoQuit`로 파일이 생성됐다. 위 CPU 구간 내보내기처럼 파일·분석 완료 로그로 성공을 확인한다.

- **큐 합본 함정(2026-11-09 정정):** `Frame` Count가 실제 프레임의 2배인 것은 트랙 중복이 아니라 GPU 큐가 둘(그래픽 `GPU0-Graphics0`·비동기 계산 `GPU0-Compute0`)이고 두 큐 모두 프레임 전체 길이의 `Frame`·`SceneRender`를 남기기 때문이다. `ExportTimerStatistics`는 `-threads`로 큐 하나를 지정해도 모든 GPU 큐를 합쳐 내보낸다. 그래서 합본 `Incl ÷ Frame Count`는 한 큐에만 있는 패스를 **절반**으로 만들고, 계산 큐의 유휴 시간을 `SceneRender` 자기 시간으로 섞는다. 이전 규약(합본을 `Frame` Count로 나누기)은 폐기한다. 패스 표는 `TimingInsights.ExportTimingEvents "<csv>" -columns="*" -threads="GPU*" -region="<캡처 region>"`로 원시 이벤트를 받아 `ThreadName`별로, 그 큐의 `Frame` 수로 나눈다. 같은 이름이 중첩되면 바깥 것만 포함 시간에 넣는다. 그래픽 큐의 하위 패스 없는 구간은 계산 큐와 겹치는지 확인해 펜스 대기와 작업을 구분한다. 근거와 사례는 [렌더 동기화 인계](../analysis/RenderSyncOptimizationHandoff.md) §6.7.
- **구간 지정:** 그래픽스 하네스는 측정 구간을 trace region `LNPGraphicsCapture`로 남긴다(2026-11-08 추가, 이 변경이 들어간 패키지부터). 캡처 시작에 열고 CSV 종료나 중단 시 닫는다. 이전 패키지의 트레이스는 `[GraphicsBenchmark] Capture started` 로그 시각으로 `-startTime`/`-endTime`을 정한다.
- **진단 요인:** 하네스가 준비 단계에서 렌더 조건 CVar를 콘솔 우선순위로 다시 쓰므로, 해상도 비율 같은 요인은 `-ExecCmds` 대신 `-LNPGraphicsDiagnosticCVars="이름=값;이름=값"`으로 준다. 기본 조건 뒤에 적용·종료 때 복구되며, 없는 이름은 실행을 실패시킨다. 지정한 회차는 `Run.json`의 `diagnostic_cvars`에 남고 `performance_eligible=false`다. 런처는 `RunGraphicsBenchmark.ps1 -ExtraArguments @('-trace=...', '-tracefile="{RunDirectory}/Diag.utrace"', '-LNPGraphicsDiagnosticCVars=...')`처럼 받고 `Launch.json`에 기록한다. 진단 세트 구성은 [렌더 동기화 인계](../analysis/RenderSyncOptimizationHandoff.md) §6.5를 따른다.
- GPU 트레이스 실행은 `DiagnosticOnly=true`·`BaselineEligible=false`다. 패스 표는 같은 실행의 CPU 대기 서명(`GPUBound_WaitingForGPUForOcclusionQueries`·`WaitForVisibilityTasks`·`Sync_RenderingThread`)과 함께 보고한다.

### Wander 재선택 진단 계수

구현·정규Editor 빌드와 별도Game Development 패키지의 짧은0/1 실행까지 확인했다(2026-11-08). 기본값0은247개 측정 행에 진단 열이 없었고, 진단1은274개 행의9개 열·시각·계수 관계를 통과했다. 인코딩 거부는0으로 해당 분기는 미관측이다. `LNP.Enemy.Wander.Diagnostics`의 기본값은0이며,1일 때만 기본CSV 카테고리에 `LNPWander_` 접두사로 기록한다. 기존LoadBaseline 진단 모드와 독립적이다. 별도CSV 카테고리·엔티티별 로그·복제 상태는 추가하지 않는다.

계수는 서버/Standalone의 Wander 프로세서 실행마다0으로 초기화하고 종료 시 `Set`으로 쓴다. 단일월드·통상 프레임당1회 실행하는 현재 패키지 시나리오에서 사용한다. 같은CSV 프레임에 여러 번 기록하면 마지막 값이 남으므로 일반적인 누적 합계로 해석하지 않는다. CVar는 캡처 전에 정하고 캡처 중 변경하지 않는다.

| `LNPWander_` 뒤의 열 이름 | 의미 |
|:---|:---|
| `WorldSeconds` | 해당 프로세서 실행의 월드 게임 시각(초). 트레이스 세션 시각과 구분한다 |
| `Eligible` | Wander·안전 상태·유효Snapshot/리비전 조건을 통과한 개체 수 |
| `Invalid` | 기존 배회 구간이 유효하지 않아 재선택에 진입한 개체 수 |
| `Expired` | 기존 구간은 유효하지만 `Now >= StopUntil`로 재선택에 진입한 개체 수 |
| `OverlapRejected` | 재선택 진입 후 초기 겹침 때문에 후보 시도 전에 이탈한 개체 수 |
| `CandidateAttempts` | 후보 루프의 실제 시도 횟수. 개체 수가 아니며 개체당 최대2회(2026-11-08 최적화 전 계측 패키지는 4회) |
| `Accepted` | 후보 검사를 통과해(잘린 후보 포함) 목표·도착/정지 기한을 설정한 개체 수. 인코딩 전 결과 |
| `Failed` | 최대 시도에서 후보를 고르지 못한 개체 수. 인코딩 실패와 구분한다 |
| `CodecRejected` | 후보 선택 결과 처리 후 복제 데이터 인코딩이 비활성으로 판정된 개체 수. Accepted/Failed/Deferred와 겹칠 수 있다 |
| `Deferred` | 실행당 재선택 상한(16)을 넘어 후보 검사 없이 짧은 제자리 구간을 받은 개체 수 |
| `Truncated` | Accepted 중 막힌 자리 앞에서 잘라 쓴 구간 수 |
| `GroundAttempts`·`FlightAttempts`·`CrawlAttempts` | 모드별 후보 시도 수. 합이 `CandidateAttempts`다 |
| `GroundFailed`·`FlightFailed`·`CrawlFailed` | 모드별 Failed. 합이 `Failed`다 |
| `RejectGoalSupport`·`RejectEndpoint`·`RejectDirectWalk`·`RejectPathSurface`·`RejectPathSweep`·`RejectFlightSweep`·`RejectCrawl`·`RejectReservation`·`RejectTileOverflow` | 거부된 시도의 단계별 수. 합이 거부된 시도 수(`CandidateAttempts − Accepted`)다 |

런타임 기능 검증에서는 `R = Invalid + Expired`, `S = R - OverlapRejected`로 두고 아래 관계를 대조한다.

- `Eligible >= R >= OverlapRejected >= 0`
- `Accepted + Failed + Deferred = S`
- `Accepted + Failed <= CandidateAttempts <= 2 × (Accepted + Failed)`
- `Σ Reject* = CandidateAttempts − Accepted`, `Truncated <= Accepted`
- `0 <= CodecRejected <= S`

`Expired`의 같은 실행 내 개체 수는 기한 만료의 집중도를 보여주지만 직전 구간의 선택 실패 여부를 저장한 값은 아니다. 여러 행을 더한 계수는 개체-실행 합계이며 고유 개체 수가 아니다. 개체별 실패 이력을 복원하거나3초 집중의 원인을 이 계수만으로 자동 확정하지 않는다. 진단을 켠 실행은 별도 자료로 남기고 기준선에 섞지 않는다. 계측이 포함된 별도 패키지는 `Saved/Graphics/Phase00WanderDiagnosticsPackage_20261108/`이며, 기존 `Phase00Package`는 보존한다.

동일 행의 계수와 GT를 비교할 때 `LNPGraphics_GTMs`의 지연을 구분한다. 하네스 Tick에서 읽은 `GGameThreadTime`은 직전 프레임 값이다. 2026-11-08 긴 진단의 2977개 인접 행에서 `LNPGraphics_GTMs[i] == GameThreadTime[i-1]`를 확인했다. 같은 행의 연관성 조사에는 기본CSV의 `GameThreadTime`을 사용하며, 이 값을 Wander 자체 시간으로 쓰지 않는다. 트레이스와 CSV는 실제 캡처 시작 Time·`PathSeconds`·GT 프레임 포함 관계로 대조하고, 월드 게임 시각이나 이벤트 순서만으로 직접 연결하지 않는다. 경계에 걸린 프레임을 확인할 수 있도록 이벤트 내보내기 구간에 여유를 둔다.

긴 진단의 첫CSV 프레임에서는 Wander가 캡처 시작 로그보다 먼저 실행됐다. 로그 시각185.032992~245.032992초 안의 Wander2977회와 CSV 프레임 기준2978회를 구분한다. 실제 로그 구간 앞뒤0.1초를 더한 범위에서 `FEngineLoop::Tick`·실행된 `LNPEnemyWanderProcessor_2147482013`의 이벤트를 받아 프레임 포함 관계로 연결했다. Timers의 접미사 없는 `LNPEnemyWanderProcessor`는 사용자 확인상 Count/Incl/Excl이0이므로 선택하지 않았다. 프로세서 이름 접두사만 보고0회 타이머를 내보내지 않는다. 2978개 시각 대응점은 프레임 시작/끝에서 최소5.680/3.128ms 떨어져 있었고, 후보 시도와 Wander의 같은 프레임 상관0.9385는 앞뒤 한 프레임 대조에서-0.0089/-0.0035였다. 근거·입력 해시·정밀도는 해당 실행의 `WanderCounterJoinValidation.json`을 따른다. 이 결과를 임의의 하네스/다중월드에 일반화하지 않는다.

CSV 프레임 표 뒤에는 반복 헤더와 `[HasHeaderRowAtEnd],`로 시작하는 엔진 메타데이터가 붙는다. `-ExecCmds`처럼 따옴표·쉼표가 들어간 명령행은 후행 메타데이터에서 일반CSV 인용 규칙을 만족하지 않을 수 있다. 집계기는 이 예약 표식에서 프레임 표 읽기를 마치며 메타데이터를 프레임 행으로 파싱하지 않는다. 원본CSV를 고쳐 쓰지 않는다. 이번 진단1 자료에서 이 문제를 재현했고, 수정 후 기본값0 집계가 동일함과 진단1의274개 측정 행 검증을 확인했다.

측정 보고는 Phase 로그(`history/PhaseNN_Log.md`)에 다음 표로 남긴다. 원자료(CSV·트레이스)는 `Saved/` 경로만 적는다.

| 시나리오 | 빌드·해상도 | 편성 | 변경 요인 | Frame | GT | RT | GPU | 드로우콜 | 비고 |
|:---|:---|:---|:---|---:|---:|---:|---:|---:|:---|
| 예: 기준선 3k | 패키지 Development, 1440p | `DA_..._Bench3k` | 없음 | | | | | | 중앙값(3회) |

- 전후 비교는 같은 시나리오 행을 나란히 두고 차이와 예산 대비 비율을 함께 쓴다.
- 포트폴리오에 쓸 그래프는 이 표에서 만든다.
