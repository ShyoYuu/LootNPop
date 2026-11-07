# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: 세션 마감·Medium GT 추가 호출 경로 분석을 새 세션으로 인계
> 마지막 갱신: 2026-11-07

## 1. 바로 다음 작업

공통180초 워밍업+60초 측정으로 **세 편성 각3회 수집**을 마쳤다. 아홉 회차 모두 정상 종료·수집 검증 통과다. Low/High는 반복 편차 기준을 통과했고 **Medium은GT7.2288%로5% 기준 미통과**다. Frame/RT/GPU는 통과했으나 전체 지표 재현성 통과로 표시하지 않는다. 이전60초 시리즈와 실패 세트도 따로 보존했다.

측정 환경 기록을 마쳤다. 전원 어댑터 연결·Windows 전원 모드 Best Performance·외부 모니터의 연결 GPU NVIDIA GeForce RTX4060 Laptop GPU를 사용자가 확인했다. 게임 RHI도RTX4060이며 Windows 전원 계획 조회는Balanced다. Armoury Crate는 미설치이며 제조사 작동 모드는 미확인으로 기록한다. 측정 동안 현재 전원·화면 설정을 유지한다.

1. 사용자 승인으로 **Medium CPU 트레이스1회(180초 워밍업+60초 경로)**를 수집하고 비용을 분해했다. 사용자가 Insights 로그에서 확인한 시작184.502898초부터244.502898초까지2983프레임을 분석했다. 프레임당 렌더 동기화 대기9.2741ms, `WaitForTasks`3.6396ms, Wander 처리2.1712ms(하위 범위 제외1.6231ms)다. 포함 시간은 중첩되므로 합산하지 않는다. 한 회차 자료만으로 기존3회 GT 편차의 원인을 확정하지 않는다.
2. 사용자는 다음 단위를 **새 세션으로 넘기고 문서·현재 변경을 한 번에 커밋**하도록 요청했다. 다음 세션의 첫 제안은 **기존 트레이스의 `WaitForTasks` 호출 경로와 관련 Mass 작업 의존성 조사**다. 새 게임 실행 없이 대기 구간의 소유 작업을 좁히고, 코드 변경·추가 측정의 필요성을 판단한다. 다음 단위의 실행 승인은 아직 받지 않았다.
3. 새 워밍업 연장이나 동일3회 재측정은 아직 계획하지 않는다. 기존 자료에서3회차 GT가 세 경로 구간 모두 낮았고 활성 NPC 수도 달랐지만 원인은 미확정이다. GT 재현성·영상·전체 보고·깨끗한 트리 빌드 조건이 남아 있으므로 Phase0 완료·단위N 착수로 확대하지 않는다.

CPU 진단은 CSV·트레이스 분석이 가능하지만 게임 종료 코드777003(`CrashReporterCrashed`)이 남아 정상 종료로 판정하지 않는다. 로그의 종료 요청은0이며 실제 원인은 미확정이다. Insights 메모리 태그·타이머 메타데이터 경고도 보존했다. `DiagnosticOnly=true`·`BaselineEligible=false`이며 공통 기준선 수치·재현성 판정을 바꾸지 않는다.

아래는180초 워밍업 세트의 회차별 값3개 중앙값이다. Medium은GT 재현성 미통과 상태다.

| 편성 | 초기 적 | Frame ms | GT ms | RT ms | GPU ms | 드로우콜 | Frame p95 ms | Frame 예산 초과 |
|:---|---:|---:|---:|---:|---:|---:|---:|---:|
| Low | 1053 | 20.4710 | 7.7682 | 20.4648 | 19.9319 | 578.5815 | 21.2990 | 23.32% |
| Medium(GT 미통과) | 2223 | 20.5220 | 10.5710 | 20.3779 | 19.9153 | 580.8286 | 21.7615 | 23.63% |
| High | 3257 | 22.5891 | 12.0197 | 21.6355 | 21.7444 | 588.1148 | 25.0178 | 36.08% |

Frame/GT/RT/GPU 편차는Low1.7016/2.7559/1.7028/1.8084%, Medium4.1996/7.2288/4.6052/4.4545%, High3.9926/1.4134/4.2631/3.8507%다. Medium의 GT만5%를 넘었다. 회차별 클럭 감소와 활동 분포 차이를 관측했으나 열/클럭 또는 AI 어느 하나의 원인으로 확정하지 않는다. 이전60초 시리즈 수치·실패 경위는 로그를 따른다.

준비 근거는 `Saved/Profiling/Graphics/Phase00/FormalPreparation_20261107/`의 `EditorBuild.log`·`PackageBuild.log`·`PackageIdentity.json`·`Environment.json`·`SourceCommit.txt`·`SourceChanges.patch`·`SourceHashes.json`이다. GPU1Hz 기록은 `GpuTelemetryProbe.{csv,json}`로 확인했다. 제조사 작동 모드 미확인과 전력 상한N/A도 기록에 남기며 성능 판정 시 온도·클럭·전력·회차 편차와 함께 검토한다.

정식 원자료 루트는 `Saved/Profiling/Graphics/Phase00/`다. 각 세션의 `Session.json`·`Environment.json`·회차별 원자료/`Summary.json`·`RepeatedSummary.json`·`GpuTelemetry.csv`를 보존했다.

- Low: `FormalLow_20261107_052246_552813/`
- Medium: `FormalMedium_20261107_053900_73b311/`(`LowMediumComparison.json` 포함)
- High: `FormalHigh_20261107_054854_14f080/`(`VariationInspection.json`에 구간별 RT·PSO/GC·프레임 이탈 확인)
- High180초 별도 세트: `HighWarmup180_20261107_070251_2b2b92/`(`WarmupComparison.json`에60초 세트와 조건/수치 비교)
- Low180초 세트: `LowWarmup180_20261107_090057_fd977e/`(`WarmupComparison.json` 포함)
- Medium180초 세트: `MediumWarmup180_20261107_100022_361426/`(`WarmupComparison.json`·`VariationInspection.json` 포함)
- Medium CPU 진단: `MediumCPUTrace_20261107_102250_f65aca/`(`CaptureInterval.json`·`CpuCostBreakdown.json`·`AnalysisValidation.json`·`Exports/Measured_*.csv` 포함)
- 전체 잠정 요약: `FormalBaseline_20261107_Summary.json`(60초 시리즈와180초 추가 세트를 분리·공통 기준선 미완료)
- 180초 공통 조건 요약: `FormalBaseline180_20261107_Summary.json`(수집 완료·Low/High 통과·Medium GT 미통과)

편성별 초기 적/Pod와 측정 중 생존 수, 종류/slot별 초기 요청·초기화·거부 수, 실효 조건과 바이너리/컨테이너/경로를 대조했다. 이는 개별 배치 위치 해시 검증이 아니다. GPU 온도는Low84.48~88.04°C·Medium84.11~87.67°C·High80.67~88.16°C다. 자세한 표와 해석 한계는 [Phase0 로그](history/Phase00_Log.md)를 따른다.

### 3번 주변 Pod에 관한 주의점

같은 시드·편성·베이크 입력의 생성 계획은 결정론적이다. 그러나 2번 기록 PIE는 시드3962·밀도0, 최신3번 기록 PIE는 시드1022·밀도0, 벤치마크는 시드1·밀도1이다. 기록 PIE와 벤치마크의 Pod 위치가 같다고 볼 수 없다. Low/Medium/High 사이에도 적 후보 소비·편성 전량 배치 조건 때문에 랜덤 Pod 위치가 달라질 수 있다. 고정 시드만으로 세 편성의 동일 Pod 배치를 보장하지 않는다.

기본 스폰 유지·실제 생성 수 표기라는 사용자 선택을 유지한다. Pod 강제 추가나 부족 적 수 충원은 승인된 범위가 아니다. 신규 구도의 Pod·NPC 조건이 부족하면 다시 논의한다.

## 2. 이번 세션에서 완료한 것

- **0-A/0-B:** Acropolis·기존8-slot 월드·시드1·PCG0·slot6 촬영을 선택했다. 별도 편성3개를 저장했고 기본 편성·풀·GameState BP 파일 해시를 보존했다.
- **0-C:** 기본 비활성 `ULNPGraphicsBenchmarkSubsystem`, 관찰 카메라, CSV·초기/1Hz 개체 계수, 패키지 생성·실행·집계 스크립트를 구현했다. 선택 편성·밀도1·에픽·TSR100%·1440p를 적용하고 실효값을 기록한다. 관찰자는 인지 대상에서 제외되며 AI 활동 거리는 카메라를 따른다. 전투 부하가 아닌 배회 관찰이다.
- **실제 초기 적 수:** Low1053, Medium2223, High3257, Pod는 모두120이다. High는 계획요청3393 중136미배치·스폰 초기화 중 추가 거부0이다. 일반 편성 산술합1080/3240/5400을 실제 수로 쓰지 않는다. 종류별 상세는 [Phase 0 로그](history/Phase00_Log.md)에 있다.
- **검증:** 정규 Editor 빌드·Game Development 패키지·세 편성의 짧은 패키지 예비 실행·CSV 집계를 통과했다. 최초 경로/기존 스폰 자동화3건, 최종 경로 수정 뒤 경로 자동화1건을 통과했다. 일반 패키지 실행에서 벤치마크0·하네스 비활성을 확인했다. 깨끗한 트리의 최종 전체 빌드는 아직 확인하지 않았다.
- **크래시 수정:** CSV 신규 카테고리를 제거하고 기본 카테고리의 `LNPGraphics_` 열을 사용한다. 서브시스템은 TestMap03에만 생성해 시작용 임시 월드 폐기 때 조기 종료하지 않는다. 수정 뒤 패키지 실행은 정상 종료했다. 상세 경위·실패 자료는 로그에 보존했다.
- **경로:** 1(A 하부 상향)→2(B 상부 조망)→3(C 근거리 섬)→1이다. 1번은 최초 자세, 2번 위치는 **캐릭터 위치+캐릭터 전방50cm**·회전은 기록 카메라, 3번은 이번 사용자 재선택 카메라 그대로다. 이전 경로의 무관통 확인을 신규3번에 적용하지 않는다.
- **Revision3 검증:** 1·2번·렌즈 보존, 새3번의 위치 왕복 오차0cm, Live Coding 성공·경로 자동화1/1통과·오류/경고0, 세 편성 화면 확인·정규 Editor 빌드·패키지 갱신을 통과했다. 60초 시리즈는High RT 미통과, 180초 시리즈는Medium GT 미통과다. 공통 기준선의 전체 지표 재현성 완료는 아니다.

## 3. 파일·실행·증거 위치

- 구현: `Source/LootNPop/Graphics/LNPGraphicsBenchmark.{h,cpp}`, `Source/LootNPop/Tests/LNPGraphicsBenchmarkTest.cpp`와 스폰·GameMode·관찰자 관련 최소 연결 변경.
- 편성: `Content/SpawnData/Graphics/DA_MassSpawnConfig_Graphics_{Low,Medium,High}.uasset`. 구성·cook root는 `Scripts/Profiling/Graphics/SpawnProfiles.json`에 있다.
- 경로: `Scripts/Profiling/Graphics/AcropolisViewpoints.json`. 촬영용slot6의 옥탄트 로컬 위치·쿼터니언이다. Revision3 SHA256은 `46DF16A132082BCD1CB10C312C5BA662D0DC9B9795B7DB14211B1DAFA3B877BC`다. 카메라 원자료는 `Saved/Profiling/Graphics/Phase00/CameraPointC_Revision3{,_Raw}.json`, 왕복 검사는 `CameraRevision3_Roundtrip.json`이다.
- 도구: `Scripts/Profiling/Graphics/PackageGraphicsBenchmark.ps1`, `RunGraphicsBenchmark.ps1`, `AnalyzeGraphicsBenchmark.ps1`. 정확한 명령·지표 의미는 [측정 규약](design/Measurement.md)에 있다.
- 로컬 패키지: `Saved/Graphics/Phase00Package/Windows/LootNPop/Binaries/Win64/LootNPop.exe`. 0-D 준비에서 build/cook/stage/archive로 갱신했다. 최신 식별 기록은 `Saved/Profiling/Graphics/Phase00/FormalPreparation_20261107/PackageIdentity.json`이다. 실행 파일SHA256은 `4F4D6B5A66EF0A5547ACB4CF9021778496D022D8E70FCB9D8E3A7807BAA8B5F2`, 경로는 위 Revision3과 같다. 앞선 미리보기와 새 바이너리/컨테이너를 같은 조건으로 묶지 않는다.
- 세 편성 예비 실행 요약: `Saved/Profiling/Graphics/Phase00/PackagedPreflightSummary.json`. **이 세 실행은 이전 경로**이며 최신 경로의 Medium/High 화면 검증을 대신하지 않는다.
- 최신 경로 자동화: `Saved/Profiling/Graphics/Phase00/CameraRevision3_Automation.json`. 세 편성 화면 확인 원자료는 [Phase0 로그](history/Phase00_Log.md)의 `CameraRevision3Preview/` 기록을 따른다.
- 빌드/cook 로그·해시: 같은 폴더의 `PackageBuild/`. 기본 에셋 해시: `Recovery/PreservedHashes.json`. `Saved/`의 빌드·원자료는 로컬에만 있고 git에 포함하지 않는다. 다른 환경에서는 커밋한 코드·편성·경로로 패키지를 다시 만든다.

모든 예비 실행·기능 검증은 `performance_eligible=false`다. 새 경로의 세 편성 화면 조건은 별도 사용자 확인을 통과했지만 성능 기준선·전투 부하 검증은 아니다. 프러스텀 후보는 가림을 포함하므로 실제 가시 NPC 수가 아니며 ISM/ISKM은 합산된다.

## 4. 환경과 협업 규칙

- 0-D 준비의 정규 빌드·패키징 및 정식 측정 전에 에디터 종료를 확인했다. 세 편성 정식 실행과 GPU 기록 프로세스도 모두 종료했다. CDO·일반 플레이 설정·전원 모드는 이번 작업에서 변경하지 않았다. 기존 기본 편성·풀·GameState 에셋3개의 해시 일치를 확인했다.
- 앱에 Unreal 도구가 보이지 않아도 프로젝트 `.mcp.json`의 로컬 `http://127.0.0.1:8000/mcp`는 읽기 호출로 연결을 확인했다. 재접속 시 스키마부터 조회하고 프로젝트·PIE 상태를 확인한다.
- 각 구현 단위가 끝나면 계속 진행할지 확인한다. 수동 PIE·GUI 조작이 더 빠르면 자동화 구현 전에 요청한다.
- 성능 판정은 패키지 빌드에서만 한다. 전체 빌드/cook/package는 처음부터 권한 확장으로 실행한다. 에디터가 열려 있으면 전체 빌드와 Live Coding을 동시에 사용하지 않는다.
- 전체 순서는 기준선→콘텐츠→군중 렌더링→렌더 패스→지형 흔적이며, 단위N은 Phase0 직후다. 고밀도 아트는 Phase1이다. JustDodge 이관 범위는 [현재 인계](../JustDodge/Current.md)를 따른다.

## 5. 새 세션 시작 절차

1. `README.md`·이 문서·`Decisions.md`를 읽고 [Phase0 실행서](phases/Phase00_MeasurementBaseline.md)와 [측정 규약](design/Measurement.md)의 CPU 진단 절차를 확인한다. 사용자에게 다음 분석 단위의 범위와 진행 여부를 확인한다. 이번 세션은 추가 분석·게임 실행 없이 마감했다.
2. 로컬 진단 루트 `Saved/Profiling/Graphics/Phase00/MediumCPUTrace_20261107_102250_f65aca/`의 `CaptureInterval.json`·`CpuCostBreakdown.json`·`AnalysisValidation.json`과 `Exports/Measured_GameThread.csv`부터 읽는다. 실제 트레이스는 그 아래 `Medium_20261107_102251_8b8bb9/CPU.utrace`다. `Exports/Measured.rsp`에 정확한 구간별 내보내기 명령이 있다. 전체 세션 통계는 워밍업을 포함하므로 쓰지 않는다.
3. Insights 실행 파일은 `C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealInsights.exe`다. 기존 GUI가 열려 있으면 재사용한다. 분석 구간은184.502898~244.502898초다. GT 작업 대기의 호출 경로·대상 작업과 Mass 의존성을 확인하되, 타이머 이름만으로 CPU 포화나 Wander 원인으로 단정하지 않는다. GUI에서 사용자가 직접 확인하는 편이 빠르면 즉시 요청한다.
4. `Saved/`는 git 제외 로컬 자료다. 현재 PC에서는 기존 자료를 재사용하고, 자료가 없는 환경에서 진단 재실행이나 재패키징을 자동으로 시작하지 않는다. 패키지는 기준 커밋6171904의 변경 중 상태에서 빌드했고 준비 폴더에 소스 패치·해시를 보존했다. 이번 커밋만으로 깨끗한 트리 빌드 완료로 바꾸지 않는다.
5. 분석 단위가 끝나면 관측·미확정 사항·다음 제안을 기록하고 다시 진행 여부를 확인한다. GT 반복 편차·30초~1분 영상·최종 보고·깨끗한 트리 빌드가 남아 있으므로 Phase0 완료·단위N/Phase1 착수로 넘어가지 않는다.
