# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: 세션 종료·네 개선 별도 인계·완료 후 통합 검증/재측정 절차 기록
> 마지막 갱신: 2026-11-08

## 1. 바로 다음 작업

그래픽스 메인 작업은 멈춘 상태이고, 네 개선을 다른 세션에서 다음 순서로 진행한다(2026-11-08 사용자 확인).

1. **배회 + 그래플 앵커**(병행 진행 중)
2. **GT 렌더 동기화(범위 축소) + Nav 최근접 노드**: 두 작업은 파일이 겹치지 않고 Nav는 GPU에 영향이 없어 순차·병행 모두 가능하다. 렌더 동기화는 그래플 변경이 들어간 바이너리로 GPU 패스 분해를 하고, 화질을 바꾸는 렌더 변경(적 Lumen 기여·해상도·업스케일)은 O-2·O-3으로 미룬다([인계](analysis/RenderSyncOptimizationHandoff.md) §0). 렌더 동기화는 기존 자료 재집계로 GT 대기를 GPU 역압으로 판정하고 동기화 설정 변경을 기각했다. 하네스 trace region·진단 CVar 경로를 넣고 진단 세트 D0~D4로 GPU 패스 표를 확보했다(같은 인계 §6.6). 렌더 해상도 67%에서 Frame 20.33→13.14ms이고 GT 대기도 함께 줄어 역압 판정을 확인했다. 가림 버퍼2는 효과가 없고, Pod 빔(반투명) 상한은 약0.5ms다. 미분해 3.1ms는 GPU 큐 합본 집계 착시(계산 큐 유휴)로 밝혀져 패스 표를 큐별로 다시 집계했다(§6.7). 그래픽 큐가 병목이며 TSR(히스토리 200%) 5.6ms·비동기 Lumen 대기 약5ms가 크다. TSR 히스토리 100% 진단(D5)은 Frame 20.33→18.70ms(−8%)이고 이득은 TSR에서만 나왔다(§6.8). **렌더 동기화 작업은 마무리했다(2026-11-09, §6.9).** 동기화 쪽 수정은 없고, 프로토타입 에셋에서는 화질 판정이 불가해 TSR 히스토리·Lumen 설정은 Epic 기본값을 유지한 채 벤치마크 콘텐츠 이후 재측정으로 보류했다. 해상도 하향은 최후의 카드다. Nav는 완료했다. 힌트 반경 최근접으로 호출당 3.72→1.27μs가 됐다([인계](analysis/NavNearestNodeOptimizationHandoff.md) §7).
3. **그래픽스 메인 재개**

현재 프레임은 GPU 쪽에서 막혀 있다(Medium GPU 19.92ms·GT 10.57ms). 배회·Nav 개선은 평균 Frame보다 꼬리 지연과 Medium GT 재현성에 효과를 기대한다. 네 결과가 모이면 [개선 후 재개 절차](phases/Phase00_PostOptimization.md)의 **R-A 결과 수령·통합 확인**부터 시작한다. 이어서 빌드/기능 검증→필요한Medium 비교 진단→새 바이너리의Medium·Low·High 정식3회씩→보고/영상과Phase0 마감 순서다. 이번 세션에서 후속 실행을 시작하지 않는다.

공통180초 워밍업+60초 측정으로 **세 편성 각3회 수집**을 마쳤다. 아홉 회차 모두 정상 종료·수집 검증 통과다. Low/High는 반복 편차 기준을 통과했고 **Medium은GT7.2288%로5% 기준 미통과**다. Frame/RT/GPU는 통과했으나 전체 지표 재현성 통과로 표시하지 않는다. 이전60초 시리즈와 실패 세트도 따로 보존했다.

측정 환경 기록을 마쳤다. 전원 어댑터 연결·Windows 전원 모드 Best Performance·외부 모니터의 연결 GPU NVIDIA GeForce RTX4060 Laptop GPU를 사용자가 확인했다. 게임 RHI도RTX4060이며 Windows 전원 계획 조회는Balanced다. Armoury Crate는 미설치이며 제조사 작동 모드는 미확인으로 기록한다. 측정 동안 현재 전원·화면 설정을 유지한다.

1. **배회 분석을 별도 문서로 인계했다.** [배회 최적화 인계](analysis/WanderOptimizationHandoff.md)에 코드 경로·두 진단의 측정 결과·재시도 분산 외의 검토 지점·미확정 비용·복제/안전성 계약·원자료와 검증 절차를 모았다. 최신 진단의 CSV2978개 프레임과 Wander를 연결했으며 기한 만료 상위20개 프레임의 Wander 평균24.2314ms/나머지1.9387ms를 확인했다. 사용자가 다른 세션에서 프로세서 전반을 최적화하기로 했으며 이 세션에서 최적화 코드를 구현하지 않았다.
2. 배회 외3개 항목도 독립 문서로 분리했다. [그래플 앵커](analysis/GrappleAnchorOptimizationHandoff.md)는 Niagara와 월드 후처리 대기 대표 사례, [GT 렌더 동기화](analysis/RenderSyncOptimizationHandoff.md)는 대기9.2741ms/프레임과RT/GPU 예산, [Nav 최근접 노드](analysis/NavNearestNodeOptimizationHandoff.md)는 반복 조회0.2112ms/프레임과 기존 캐시/최근접 전용 경로를 인계한다. 사용자가 각각 다른 세션에서 개선하며 이 세션에서는 개선 코드를 시작하지 않았다.
3. **렌더링 최적화 병행 항목(트랙 O)을 계획에 넣었다(GX-010 초안).** 그래픽스 이전의 선행 렌더링 부하 측정을 [선행 렌더링 부하 측정](analysis/RenderingLoadBaseline_20260925.md)으로 옮겼다. 기존 Medium CPU 진단에도 GPU 가림 질의 대기 14.148ms/프레임이 있어 GT 렌더 동기화 대기가 GPU 처리량 문제일 가능성이 높다. O-1 GPU 패스 분해는 R-C에서, O-2 해상도 스윕은 Phase0 마감 뒤 단위N과 같은 시기에, O-3 적 Lumen 기여는 Phase1 조사·Phase2-3 절제로, O-4 패스별 예산은 Phase2 산출물로 한다([로드맵](Roadmap.md) §4 트랙 O).
4. 현시점의 새 워밍업 연장/재측정은 실행하지 않는다. 개선 완료 후에는 위 재개 절차에서 새 바이너리와부하/품질 조건을 확인한 뒤 측정 단위별로 진행 확인을 받는다. 기존 자료에서3회차 GT가 경로 전반에서 낮았고 활성 NPC 수도 달랐지만 원인은 미확정이다. GT 재현성·영상·전체 보고·깨끗한 트리 빌드 조건이 남아 있으므로 Phase0 완료·단위N 착수로 확대하지 않는다.

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

1. `README.md`·이 문서·`Decisions.md`와 [개선 후 재개 절차](phases/Phase00_PostOptimization.md)를 읽는다. 네 작업의 결과가 모였으면R-A부터 시작한다. 일부만 끝났으면 반환 상태와 남은 항목을 기록하며 완료를 가정하지 않는다.
2. 각 담당 인계 문서와 최종 커밋/소스/에셋·패키지 식별·행동/품질/복제 검증을 받는다. 공통Nav/표면/Niagara 설정의 통합 상태를 확인하고 개별 대조 근거가 없는 절감량은 더하지 않는다. 현재 계측 패키지는143048c의 변경 중인 트리에서 빌드한 것으로, 이전 기준선 준비6171904와 구분한다.
3. R-B의 최종 소스 빌드·별도 패키지·짧은 기능 검증을 거친다. R-C는 반환 근거로 충분한지 먼저 판단하고 부족할 때만 별도 진단 범위를 정한다. `Saved/` 자료가 없는 환경에서 빌드/긴 실행을 자동 시작하지 않는다.
4. R-D는 새 버전 Medium3회부터 시작해 통과 후 Low/High도 같은 바이너리로 각3회 수집한다. 예전 Low/High 통과값과 새 Medium을 묶지 않는다. 진단 계측을 끄고 모든 주요 지표의5% 편차·정상 종료·실효/부하 조건을 확인한다.
5. R-E의 보고/영상·깨끗한 트리 빌드·사용자 확인까지 마친 뒤 Phase0를 마감한다. 그다음은 기존 결정대로 단위N→Phase1이다. 각 분석/구현/측정 단위마다 진행 확인을 받고 수동 PIE·GUI가 빠르면 즉시 요청한다.
