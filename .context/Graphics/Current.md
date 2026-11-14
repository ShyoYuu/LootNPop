# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 1 · 1-A 완료(2026-11-14 사용자 마감), 다음은 1-B 환경 반입 준비
> 마지막 갱신: 2026-11-14

## 0. 확정 기준

- **Phase 0 완료(2026-11-10, 사용자 마감 확인).** Meadow 포함 월드의 과거 기준선으로 보존하며 Phase 1 새 월드와 직접 섞지 않는다. [기준선 보고서](analysis/Phase00_BaselineReport.md), [Phase 0 로그](history/Phase00_Log.md).

| 편성 | 초기 적 | Frame | GT | RT | GPU | 예산 대비 Frame |
|:---|---:|---:|---:|---:|---:|---:|
| Low | 1053 | 20.21 | 4.30 | 20.21 | 19.70 | 121.8% |
| Medium | 2223 | 20.26 | 5.53 | 20.25 | 19.75 | 122.0% |
| High | 3257 | 20.62 | 6.47 | 20.61 | 20.12 | 124.2% |

- 단위 N·O-2도 완료했다. Niagara 대체 중력은 GX-011, 운영 해상도는 GX-012를 따른다. 측정은 네이티브 100%·TSR(Epic)이며 해상도 하향보다 TSR 히스토리/Lumen 후보를 먼저 검토한다. 상세는 [Decisions](Decisions.md)와 [O-2 로그](history/UnitO2_Log.md).

## 1. Phase 1의 현재 상태

- **대상/선정 유지:** 기존 Ridge + Electric Dreams(GX-013), 1차 보행 충돌 보존. 1-B0의 대분지 대표 구역·재질 방침·6종 조건부 팔레트 선정은 완료했다. 실제 환경 반입·PCG/재질 변경은 아직 하지 않았다. 파라곤 종류/수도 미확정이다. [선정 결과](phases/Phase01_RidgeZonePalette.md) §8.
- **경로/하네스 완료:** 시드 1의 Ridge slot 1·7을 실제 확인했고 측정은 slot 1을 쓴다. `RidgeViewpoints.json`의 A=DeepBasin·B=TablelandRim·C=ValleyRise, B만 구면 현지 Up으로 50cm 보정했다. 기존 화면비와 사용자 설정 1280×720(실측 1280×722)에서 관통 없음 확인을 받았다. C++·NonUFS·패키저·런처·검사가 같은 JSON을 참조한다.
- **최종 빌드/검사 완료:** 정규 `LootNPopEditor Win64 Development`, 최종 B 보정 Viewpoints 자동화 **1/1(테스트 오류/경고 0)**, Game Development build/cook/stage/archive가 모두 종료 코드 0이다. 기본 편성·풀·GameState의 디스크 해시는 보존했다.
- **출발 패키지:** `Saved/Graphics/Phase01StartPackage_20261114`, 49파일·약 1.36GiB. 실행 파일·컨테이너 5개·Ridge JSON의 존재/해시와 소스 일치를 확인했다. 경로 SHA256는 `C1E46E0BCEAAB9D9C3FE2F2BCE80BAD37E959663C4775895814350B17E023CE6`이다.
- **짧은 Medium 검증 완료:** 기존 패키지와 소스/보존 에셋 해시가 일치해 재빌드 없이 실행했다. 종료 코드 0·complete·with_editor=false·3065프레임·초기/마지막 적 2002·Pod 120, AllowPreview CSV 집계를 통과했다. 정확 2560×1440·TSR100%·Epic 그룹 3·시드 1/PCG offset 0·Ridge slot 1과 사용자 구도/무관통 확인을 받았다. 짧은 실행은 성능 기준선에서 제외한다.
- **짧은 적 0 검증 완료:** 종료 코드 0·complete·3155프레임, no_enemies=true·performance_eligible=false·초기/마지막/캡처 프레임 적 수 0·Population.csv 헤더만·initial_spawns 적 행 없음을 확인했다. Medium과 실행 파일/컨테이너/경로·렌더 조건·set별 계획 수량·slot별 Pod 초기화 수가 같다. 개별 Pod Transform은 직접 비교하지 못했으며 모든 배치 좌표 일치를 증명한 것은 아니다. [실행 결과](phases/Phase01_BenchmarkContent.md#10-짧은-패키지-검증-결과2026-11-14).
- **정식 측정:** 180초 워밍업·60초 캡처·회차별 냉각으로 Medium 3회+적 0 1회 모두 정상 종료/CSV 통과. Medium 중앙값 Frame/GT/RT/GPU=19.687/5.022/19.681/19.223ms, 반복 편차 1.88/3.59/1.89/1.91%로 5% 기준 통과. Frame은 16.6ms 예산의 118.60%다. [출발 기준선 보고서](analysis/Phase01_StartBaselineReport.md).
- **센서 보완·1-A 마감 완료:** 사용자 승인으로 적 0 한 회차를 추가했다. 정상 종료·CSV 3133프레임·모든 측정 적 수 0·Pod 120·센서 60표본, 센서 양끝 공백 0.461/0.166초·후반 저장 확인을 통과했다. 기존 Medium과 계획/Pod 수량·렌더 조건·해시가 같으며 원래 적 0(센서 부분)도 보존했다. **2026-11-14 사용자 요청으로 1-A를 마감했다. Phase 1 전체는 미완료다.** 모든 Pod Transform 직접 비교는 미검증으로 유지한다.
- **크래시 참고:** Live Coding 뒤 첫 PIE가 애니메이션 디버그 CVar 읽기에서 종료됐으나 풀빌드/재시작 후 세 회차는 정상 종료했다. Live Coding 관련성은 가설이며 애니메이션 소스는 수정하지 않았다. 새 세션에서 같은 증상이면 패치 상태를 먼저 분리한다.

## 2. 바로 다음 행동

1. **[Phase 1 실행서 §13](phases/Phase01_BenchmarkContent.md#13-1-a-마감과-다음-단위-인계2026-11-14)와 [출발 기준선 보고서](analysis/Phase01_StartBaselineReport.md)를 읽는다.** §9~12는 패키지 인계부터 센서 보완까지의 기록이다. Saved는 git 제외이므로 새 세션/체크아웃에서는 원자료 존재부터 확인한다.
2. **다음 착수 확인 대상은 1-B 환경 반입 준비다.** 진행 요청을 받으면 [기존 1-B0 선정 결과](phases/Phase01_RidgeZonePalette.md) §8을 유지하며 git 제외/반입·배치 방식과 작은 반입 시험 범위를 구체화한다. 이번 요청 범위는 1-A 마감·문서·커밋이며 1-B 작업은 시작하지 않았다.
3. 1-B0을 재선정하지 않는다. 환경 반입·PCG·재질 변경·파라곤 선정은 후속 단위에서 진행하고 기존 보행 충돌/Support source를 보존한다.

## 3. 근거와 실행 규칙

- 최종 빌드/소스 식별/패키지 해시: `Saved/Profiling/Graphics/Phase01/Build_20261114/`의 SourceSnapshot.json·BuildStatus.json·PackageVerification.json, EditorBuild.log·Automation/index.json·PackageBuild.log.
- 짧은 실행/교차 검증: `Saved/Profiling/Graphics/Phase01/PackageValidation/`의 Preflight.json·Comparison.json·Postflight.json 및 Medium/NoEnemies 하위 Launch/Exit/Run/Summary/Runtime.log. 첫 샌드박스 실행은 로그 생성 전 실패했고 같은 인자의 권한 확장 재시도는 통과했다. 원인은 미확정이다.
- 정식 원자료: `Saved/Profiling/Graphics/Phase01/StartBaseline_20261114_Retry1/`의 Session.json·RepeatedSummary.json·ValidationStatus.json·GpuTelemetry.csv와 네 회차 자료. 전원 어댑터·Windows Best Performance·외부 모니터는 사용자 확인, 제조사 도구 모드/정확한 외부 패널 연결 GPU는 미확인이다. 최초 세션 스크립트는 빈 인자 오류로 게임 실행 전 실패했으며 실패 폴더도 보존했다.
- 보완 원자료는 `NoEnemyTelemetryRetry_20261114/`, 최종 검증은 `StartBaseline_20261114_FinalValidation/RepeatedSummary.json`·ValidationStatus.json이다(같은 Phase01 상위). 최종 폴더는 기존 Medium 3회+보완 진단을 가리키는 파생 분석 인덱스이며 원자료를 옮기거나 덮어쓰지 않았다.
- 측정 회차들에 VSM pages=4096 우선순위·GameplayCue 경로 폴백·NS_BurnLoop 누락/적 화상 VFX 비활성화 경고가 있다. 관찰 실행은 통과했으며 화상 VFX 검증은 아니다. 상세는 Phase 1 로그를 따른다.
- 카메라·PIE·크래시 원자료: `Saved/Profiling/Graphics/Phase01/RidgePath_20261113/`. 최종 육안 근거는 PathVisualValidation.json, 경과는 [Phase 1 로그](history/Phase01_Log.md)다.
- 정식 측정 종료 뒤 게임/해당 센서 프로세스가 종료됐고 소스/보존 에셋 13파일·패키지 핵심 7파일 해시가 다시 일치했다. 새 실행 전 프로세스 상태를 다시 확인한다.
- 성능 판정은 패키지에서만 한다. 짧은 실행은 `-AllowPreview`로 분리하며 정식 결과로 쓰지 않는다. 냉각·전원/GPU 환경 규약은 [Measurement](design/Measurement.md)를 따른다.
- 빌드/Live Coding/쿡 직전에 사용자 병행 작업을 확인한다. UBT·cook/package는 AGENTS 규칙대로 처음부터 권한 확장으로 실행한다. Live Coding과 정규 빌드는 겹치지 않는다.
- 각 구현/검증 단위가 끝나면 계속 진행할지 확인한다. 수동 PIE/GUI가 더 빠르면 즉시 요청한다.
- 화면 녹화는 전체 모니터를 기록하므로 사용자가 다른 창을 닫고 자리를 비운다고 확인한 때만 실행한다. 이번 세션에서는 녹화하지 않았다.
