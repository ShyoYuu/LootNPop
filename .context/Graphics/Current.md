# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 1 · 1-A 경로·패키지 준비 완료, 패키지 실행 검증부터 새 세션으로 인계
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
- **아직 미검증:** 패키지 실제 실행, 정확 2560×1440/TSR100%·Epic 실효값/구도, 실제 적 0 실행·Pod 계획/배치 일치, 출발 기준선 Medium 3회+적 0 1회. **1-A 전체와 Phase 1은 미완료다.**
- **크래시 참고:** Live Coding 뒤 첫 PIE가 애니메이션 디버그 CVar 읽기에서 종료됐으나 풀빌드/재시작 후 세 회차는 정상 종료했다. Live Coding 관련성은 가설이며 애니메이션 소스는 수정하지 않았다. 새 세션에서 같은 증상이면 패치 상태를 먼저 분리한다.

## 2. 바로 다음 행동

1. **[Phase 1 실행서 §9](phases/Phase01_BenchmarkContent.md#9-새-세션-인수인계2026-11-14)를 현재 인계 원본으로 읽는다.** §7은 착수 전 과거 기록이다. 패키지/원자료가 있는지, PackageVerification.json의 해시가 맞는지 확인한다. Saved는 git 제외이므로 다른 체크아웃에는 없을 수 있다.
2. 다음 단위는 **짧은 Medium·적 0 패키지 실행 검증**이다. 이번 세션에서는 시작하지 않았다. 새 세션에서 진행을 요청받으면 §9의 명령과 검증 항목을 사용하고 2560×1440 구도를 사용자에게 확인받는다. 로컬 패키지가 그대로면 문서 커밋만을 이유로 재빌드하지 않는다.
3. 짧은 검증을 마치면 계속 진행할지 확인한 뒤 정식 기준선(워밍업 180초·캡처 60초·회차별 냉각)으로 간다. **1-A 마감 전 환경 반입/장식을 시작하지 않는다.**

## 3. 근거와 실행 규칙

- 최종 빌드/소스 식별/패키지 해시: `Saved/Profiling/Graphics/Phase01/Build_20261114/`의 SourceSnapshot.json·BuildStatus.json·PackageVerification.json, EditorBuild.log·Automation/index.json·PackageBuild.log.
- 카메라·PIE·크래시 원자료: `Saved/Profiling/Graphics/Phase01/RidgePath_20261113/`. 최종 육안 근거는 PathVisualValidation.json, 경과는 [Phase 1 로그](history/Phase01_Log.md)다.
- 세션 종료 시 LootNPop 에디터는 닫혀 있고 Electric Dreams 에디터는 유지했다. 프로세스 상태는 새 세션에서 재확인한다. 이전 메모리 CVar 설정은 LootNPop 종료로 소실됐다.
- 성능 판정은 패키지에서만 한다. 짧은 실행은 `-AllowPreview`로 분리하며 정식 결과로 쓰지 않는다. 냉각·전원/GPU 환경 규약은 [Measurement](design/Measurement.md)를 따른다.
- 빌드/Live Coding/쿡 직전에 사용자 병행 작업을 확인한다. UBT·cook/package는 AGENTS 규칙대로 처음부터 권한 확장으로 실행한다. Live Coding과 정규 빌드는 겹치지 않는다.
- 각 구현/검증 단위가 끝나면 계속 진행할지 확인한다. 수동 PIE/GUI가 더 빠르면 즉시 요청한다.
- 화면 녹화는 전체 모니터를 기록하므로 사용자가 다른 창을 닫고 자리를 비운다고 확인한 때만 실행한다. 이번 세션에서는 녹화하지 않았다.
