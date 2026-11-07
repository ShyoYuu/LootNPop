# 그래픽스 작업 현재 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 0의 0-C 수집·경로 검증 통과, 다음 단위는 새 세션으로 인계
> 마지막 갱신: 2026-11-07

## 1. 다음 세션의 첫 작업

사용자는 다음 단위를 새 세션으로 넘기고 이번 변경을 한 번에 커밋하도록 요청했다. **0-D 정식 반복 측정은 미착수이며 진행 확인도 아직 받지 않았다.**

1. `README.md`·이 문서·`Decisions.md`를 읽고 [Phase 0 실행서](phases/Phase00_MeasurementBaseline.md)와 [측정 규약](design/Measurement.md)을 확인한다.
2. 먼저 **Low/Medium/High의 3번 구도 주변에 Pod·NPC가 충분한지 확인하는 방법과 범위**를 제시하고 진행 확인을 받는다. 실제 화면을 사용자가 보는 편이 빠르면 바로 요청한다. 3번 주변의 편성별 Pod 유무·근거리 NPC 밀도는 아직 확인하지 않았다.
3. 해당 조건이 적절하면 전원 어댑터·고정 성능 모드·화면 연결 GPU·클럭/온도 기록 가능 여부를 사용자와 확인하고 0-D를 진행한다. 기본은 편성별60초 워밍업+60초 측정·3회다. 새 회차를 현재 예비 실행과 묶지 않는다.

### 3번 주변 Pod에 관한 주의점

같은 시드·편성·베이크 입력의 생성 계획은 결정론적이다. 그러나 구도를 잡은 PIE는 시드3962·밀도0, 벤치마크는 시드1·밀도1이다. 두 실행의 Pod 위치가 같다고 볼 수 없다. Low/Medium/High 사이에도 적 후보 소비·편성 전량 배치 조건 때문에 랜덤 Pod 위치가 달라질 수 있다. 고정 시드만으로 세 편성의 동일 Pod 배치를 보장하지 않는다.

기본 스폰 유지·실제 생성 수 표기라는 사용자 선택을 유지한다. Pod 강제 추가나 부족 적 수 충원은 승인된 범위가 아니다. 3번 주변 조건이 부족하면 근거를 보고하고 구도 조정 여부를 확인한다.

## 2. 이번 세션에서 완료한 것

- **0-A/0-B:** Acropolis·기존8-slot 월드·시드1·PCG0·slot6 촬영을 선택했다. 별도 편성3개를 저장했고 기본 편성·풀·GameState BP 파일 해시를 보존했다.
- **0-C:** 기본 비활성 `ULNPGraphicsBenchmarkSubsystem`, 관찰 카메라, CSV·초기/1Hz 개체 계수, 패키지 생성·실행·집계 스크립트를 구현했다. 선택 편성·밀도1·에픽·TSR100%·1440p를 적용하고 실효값을 기록한다. 관찰자는 인지 대상에서 제외되며 AI 활동 거리는 카메라를 따른다. 전투 부하가 아닌 배회 관찰이다.
- **실제 초기 적 수:** Low1053, Medium2223, High3257, Pod는 모두120이다. High는 계획요청3393 중136미배치·스폰 초기화 중 추가 거부0이다. 일반 편성 산술합1080/3240/5400을 실제 수로 쓰지 않는다. 종류별 상세는 [Phase 0 로그](history/Phase00_Log.md)에 있다.
- **검증:** 정규 Editor 빌드·Game Development 패키지·세 편성의 짧은 패키지 예비 실행·CSV 집계를 통과했다. 최초 경로/기존 스폰 자동화3건, 최종 경로 수정 뒤 경로 자동화1건을 통과했다. 일반 패키지 실행에서 벤치마크0·하네스 비활성을 확인했다. 깨끗한 트리의 최종 전체 빌드는 아직 확인하지 않았다.
- **크래시 수정:** CSV 신규 카테고리를 제거하고 기본 카테고리의 `LNPGraphics_` 열을 사용한다. 서브시스템은 TestMap03에만 생성해 시작용 임시 월드 폐기 때 조기 종료하지 않는다. 수정 뒤 패키지 실행은 정상 종료했다. 상세 경위·실패 자료는 로그에 보존했다.
- **경로 최종본:** 1(A 하부 상향)→2(B 상부 조망)→3(C 근거리 섬)→1이다. 1번은 최초 자세, 2번 위치는 **캐릭터 위치+캐릭터 전방50cm**·회전은 기록 카메라, 3번은 새로 고른 카메라 위치·회전 그대로다. 사용자가 최종 재생의 세 구간 모두 관통 없이 정상이라고 확인했다.

## 3. 파일·실행·증거 위치

- 구현: `Source/LootNPop/Graphics/LNPGraphicsBenchmark.{h,cpp}`, `Source/LootNPop/Tests/LNPGraphicsBenchmarkTest.cpp`와 스폰·GameMode·관찰자 관련 최소 연결 변경.
- 편성: `Content/SpawnData/Graphics/DA_MassSpawnConfig_Graphics_{Low,Medium,High}.uasset`. 구성·cook root는 `Scripts/Profiling/Graphics/SpawnProfiles.json`에 있다.
- 경로: `Scripts/Profiling/Graphics/AcropolisViewpoints.json`. 촬영용slot6의 옥탄트 로컬 위치·쿼터니언이다. 최종 SHA256은 `E07430EA141E3543457C2030E2B7E1040297448191EE6EE8DC71B40C34F766CB`다.
- 도구: `Scripts/Profiling/Graphics/PackageGraphicsBenchmark.ps1`, `RunGraphicsBenchmark.ps1`, `AnalyzeGraphicsBenchmark.ps1`. 정확한 명령·지표 의미는 [측정 규약](design/Measurement.md)에 있다.
- 로컬 패키지: `Saved/Graphics/Phase00Package/Windows/LootNPop/Binaries/Win64/LootNPop.exe`. 마지막 수정은 NonUFS 경로 JSON만 복사했다. 게임 실행 파일·cook 컨테이너는 그대로이며 기록은 `Saved/Profiling/Graphics/Phase00/PackageBuild/ViewpointsRevision2.json`이다.
- 세 편성 예비 실행 요약: `Saved/Profiling/Graphics/Phase00/PackagedPreflightSummary.json`. **이 세 실행은 이전 경로**이며 최신 경로의 Medium/High 화면 검증을 대신하지 않는다.
- 최종 경로 자동화: `Saved/Profiling/Graphics/Phase00/CameraRevision2_Automation.json`. 최신 사용자 확인 실행: `CameraRevision2Preview/Low_20261107_041440_5ac018/Low_14E75E44474BC52B9ABBBB9060BFF636/`(같은 Phase00 원자료 폴더 아래).
- 빌드/cook 로그·해시: 같은 폴더의 `PackageBuild/`. 기본 에셋 해시: `Recovery/PreservedHashes.json`. `Saved/`의 빌드·원자료는 로컬에만 있고 git에 포함하지 않는다. 다른 환경에서는 커밋한 코드·편성·경로로 패키지를 다시 만든다.

모든 예비 실행은 `performance_eligible=false`다. 프러스텀 후보는 가림을 포함하므로 실제 가시 NPC 수가 아니며 ISM/ISKM은 합산된다. 성능 기준선·전투 부하·새 경로의 세 편성 조건 확인으로 확대 해석하지 않는다.

## 4. 환경과 협업 규칙

- 마지막 조작에서 에디터는 실행 중이고 PIE는 종료했다. 마지막 조회는 Benchmark0·EnemyDensity0이며 BP GameState CDO 시드는0으로 복구했다. 새 세션에서 실제 상태를 다시 확인한다. 사용자 일반 플레이 설정을 임의로 변경하지 않는다.
- 앱에 Unreal 도구가 보이지 않아도 프로젝트 `.mcp.json`의 로컬 `http://127.0.0.1:8000/mcp`는 읽기 호출로 연결을 확인했다. 재접속 시 스키마부터 조회하고 프로젝트·PIE 상태를 확인한다.
- 각 구현 단위가 끝나면 계속 진행할지 확인한다. 수동 PIE·GUI 조작이 더 빠르면 자동화 구현 전에 요청한다.
- 성능 판정은 패키지 빌드에서만 한다. 전체 빌드/cook/package는 처음부터 권한 확장으로 실행한다. 에디터가 열려 있으면 전체 빌드와 Live Coding을 동시에 사용하지 않는다.
- 전체 순서는 기준선→콘텐츠→군중 렌더링→렌더 패스→지형 흔적이며, 단위N은 Phase0 직후다. 고밀도 아트는 Phase1이다. JustDodge 이관 범위는 [현재 인계](../JustDodge/Current.md)를 따른다.
