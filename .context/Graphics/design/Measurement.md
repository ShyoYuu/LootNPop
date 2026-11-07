# 그래픽스 측정 규약

> 상태: 0-C에서 실행·집계 검증, 0-D 정식 반복 측정 대기
> 마지막 갱신: 2026-11-07

## 1. 목표 사양 (GX-002)

| 항목 | 값 | 상태 |
|:---|:---|:---|
| GPU | NVIDIA GeForce RTX 4060 Laptop GPU (iGPU Radeon 780M 병존) | 확정 |
| 출력 해상도 | 2560×1440 | 확정 |
| 내부 해상도·업스케일러 | Phase 0 기준선은 TSR·ScreenPercentage100 | 0-C 구현·패키지 실효값 확인 |
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
| 정식 3회 실행 | 위 명령의 `-Repeats 3`, 각 Low/Medium/High는 사용자 진행 확인 뒤 별도로 실행한다 |
| 짧은 패키지 기능 검증 | `-WarmupSeconds 5 -DurationSeconds 6`을 추가한다. 성능 판정에서는 제외한다 |
| 결과 집계 | `Scripts/Profiling/Graphics/AnalyzeGraphicsBenchmark.ps1 -RunDirectory <Run.json이 있는 폴더>` |
| 예비 실행 집계 | 집계 명령에 `-AllowPreview`를 추가한다. 기본 집계는 예비 실행을 거부한다 |
| PIE 미리보기 | PIE 전에 `LNP.Graphics.Benchmark 1`, `LNP.Graphics.Benchmark.Profile Low` 설정. `LNP.Graphics.Benchmark.WarmupSeconds`·`DurationSeconds`로 짧게 조절할 수 있다. PIE 종료 후 Benchmark를0으로 복구한다 |

패키지 실행 파일은 `Saved/Graphics/Phase00Package/Windows/LootNPop/Binaries/Win64/LootNPop.exe`다. 런처는 DX12·단일 플레이어로 실행하고, 시드·편성·카메라 준비 → 워밍업 → CSV 시작 확인 → 경로 재생 → CSV 쓰기 완료 순서를 기다린다. 경로는 60초 기본값에서 A(하부 상향) → B(상부 조망) → C(근거리) → A로 선형 위치·쿼터니언 구면 보간한다. 워밍업도 같은 경로를 돌린다. 카메라 시점과 구도는 사용자 육안 검증이 필요하다.

최종 경로는 사용자 확인에서 세 구간 모두 관통 없이 통과했다(2026-11-07). 구도 확인만 빠르게 반복할 때는 `-WarmupSeconds 0 -DurationSeconds 30`을 사용한다. 이 경우 구간당10초이며 성능 기준선으로 쓰지 않는다. 워밍업이 있으면 해당 경로를 미리 돌고 촬영 시작 때1번으로 복귀한다.

경로 JSON은 NonUFS 파일이므로 카메라만 바꿀 때는 기존 파일을 보존한 뒤 패키지의 `Windows/LootNPop/Scripts/Profiling/Graphics/AcropolisViewpoints.json`에 같은 파일을 복사하고 해시를 확인할 수 있다. 런처는 이 스테이징된 파일을 읽는다. 실행 파일·cook 콘텐츠를 재빌드한 것과 구분해 기록하고, 경로 해시가 다른 회차는 같은 조건으로 묶지 않는다.

고정 시드는 같은 편성·베이크의 초기 계획을 재현한다. 편성별 적 배치 제약이 랜덤 Pod 후보 채택에 영향을 주므로 Low/Medium/High 사이의 Pod 위치 일치까지 보장하지 않는다. 구도를 잡은 임의 시드·밀도0 PIE와 실제 벤치마크의 군중 분포도 따로 확인한다. 현재3번 주변의 편성별 Pod·근거리 NPC 조건은 다음 세션의 확인 항목이다.

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

## 5. 보고 형식

측정 보고는 Phase 로그(`history/PhaseNN_Log.md`)에 다음 표로 남긴다. 원자료(CSV·트레이스)는 `Saved/` 경로만 적는다.

| 시나리오 | 빌드·해상도 | 편성 | 변경 요인 | Frame | GT | RT | GPU | 드로우콜 | 비고 |
|:---|:---|:---|:---|---:|---:|---:|---:|---:|:---|
| 예: 기준선 3k | 패키지 Development, 1440p | `DA_..._Bench3k` | 없음 | | | | | | 중앙값(3회) |

- 전후 비교는 같은 시나리오 행을 나란히 두고 차이와 예산 대비 비율을 함께 쓴다.
- 포트폴리오에 쓸 그래프는 이 표에서 만든다.
