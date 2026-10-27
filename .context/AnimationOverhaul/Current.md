# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: **Phase 5, 5-0 완료·5-1 진행 중. §16~§18 검증 단위와 임시 Actor 정리 완료. 새 세션으로 인계하며 다음 단위는 미착수.**
> 마지막 갱신: 2026-10-27

Phase 0·1A·1C·2·3은 완료, 1B는 대기다. **5-1 미완료·5-2 미착수·Go/No-Go 보류**이며 이번 결과를 UAF 마이그레이션 승인으로 해석하지 않는다.

## 새 세션의 시작점과 승인 경계

1. `CLAUDE.md`·이 폴더의 `README.md`·`Current.md`·`Decisions.md`를 읽고 `phases/Phase05_UAFSpike.md` §2·§18 최종 판정을 확인한다. 전체 게이트를 대응할 때 `Roadmap.md` Phase 5를 추가로 읽는다.
2. 다음 제안은 **5-1 종료 판정·남은 비교 범위 정리**다. 기존 완료 근거를 최초 조건에 대응하고, 미판정 항목이 5-1 종료에 필요한지 구분해 실제로 필요한 부족 표본/비교만 정한다. 현재 세션에서는 이 단위의 조사·구현을 시작하지 않았다. 새 세션에서 사용자 진행 요청을 받은 뒤 착수한다.
3. 각 구현 단위가 끝나면 계속 진행할지 확인한다. PIE·Rewind·GUI 수동 조작이 더 빠르면 자동화·시험 장치 추가 전에 즉시 요청한다. 이미 완료한 빌드·녹화를 인계만을 위해 반복하지 않는다.
4. 사용자가 이번 세션의 인수인계 작성과 단일 커밋을 승인했다. 이후 구현·추가 커밋·푸시는 자동 승인 범위가 아니다. 새 세션/채팅 생성이나 푸시는 하지 않았다.

## 이번 세션 완료 결과

상세 근거·분모·한계는 실행서의 해당 절이 소유한다.

| 단위 | 확인한 결과 | 남은 한계 |
|:---|:---|:---|
| §16 ResetRoot/CopyBone | 기본/숨김 ABP CopyBone 두 개의 복사 플래그 모두 false. Chooser PSD52개의 GASP 시퀀스319개와 Fall_Loop의 RefPose 루트 잠금, UAF Preserve Original Root Rotation 확인. 현재 Offset Root 전 루트 복원만을 위한 별도 노드 불필요 | 정적 계약 판정이며 전체 본 포즈 동등성은 미판정 |
| §17 추가 전환 | Sprint Turns·GaitTransitions·Run Pivot의 PSD 배열/정책/블렌드 전달과 실제 후보 평가 확인. 선택 결과는 Sprint Start·Sprint Loop 유지·Run RL Start | 목표 전환 클립 재생·지속 고속 품질을 확인한 것은 아님. 최신 발 배치 전의 과거 녹화 |
| §18 기존 이동 패널 | 사용자 관찰 이상 없음. 점프/동일 발판 착지17쌍, 공중26.3초 task10개/CR 없음·재착지27.3초 task12개/마지막 CR 복귀. 실제 패널 라이브 회전 차이10.896426° | 경로상 기울기 변화이며 큰 Yaw와 구분. ForceReset 정확한 한 프레임·17쌍 전체 task 조회는 안 함 |
| §18 큰 Yaw 발판 | 신규 BP·로컬 Yaw30°/초. 라이브 자세 사이91.274880°·Up 차이0°. 롱소드15초 정지→10초 짧은 걷기/멈춤→5초 정지 관찰 이상 없음. 녹화 약10초 task12개·최종 구면 보정/진단 뒤 CR 실행 | 남극 부근(위도 약−85.46°)·현재 롱소드 기준. 라이브 회전은 녹화 프레임과 동기화하지 않음. 적도 큰 Yaw·총기 카메라 유지 분기는 미검증 |

- 기존 패널 녹화 Root 창68개/2,661프레임 중 지상/base2,147·공중514, CR 앞 거리≤30cm·Yaw≤90°·Tilt0°. 첫 창 제외 안정 탑승30창/1,167프레임 최대 거리0.143cm다.
- 큰 Yaw 녹화 Root 창41개/1,846프레임은 모두 지상/base이며 CR 앞 거리≤30cm·Yaw≤88°·Tilt0°다. 마지막 정지6창은 최대 거리0cm·Yaw0.16°다. 창 합계를 정확한 녹화 전체 프레임 수로 해석하지 않는다.
- `maxBaseDeg`는 Mover가 Actor에 적용한 based 델타이며 발판 자체의 전체 회전각이 아니다. 모든 `[UAFRoot]` 수치는 **CR 앞** 진단이다. CR 뒤 전체 본 포즈 수치나 완전 동등성으로 확대하지 않는다.
- 큰 Yaw 시험의 총기/롱소드 차이는 사용자 관찰이다. 총기는 카메라 방향을 유지하려고 제자리 회전을 반복한다고 보고했으며 이를 결함으로 판정하지 않았다. 무기 레이어/몽타주/ANS 계약 통과 근거도 아니다.

## 유지한 구현과 현재 에디터 상태

- 숨김 `ABP_SourceUAFSpike`가 Chooser 다중 PSD·Mover 궤적·상태를 공급하고 UAF가 VisualMesh의 UEFN 포즈를 평가한다. locator는 `Current Actor → VisualMesh`, UAF PrePhysics는 숨김 메시 뒤에 실행된다.
- AG 흐름: MM/Fall_Loop Bool + Pose History → Passthrough/Strafe/입력 어댑터 → Offset Root/최종 구면 보정 → **Control Rig(v2)/Passthrough Blend → Output**. 기존 B0·공중 고정·Strafe/Steering 계약을 유지한다.
- 발 배치는 기존 `/Game/Characters/ControlRigs/CR_Biped_FootPlacement_UEFN`을 수정 없이 참조한다. 지상 Bool 알파·진입0.3/해제0.2초, DoRaycast=true·UseIKBoneTargets=false, 최초 활성/착지/재활성 ForceReset 공급, moving base의 핀 고정·루트 감쇠 억제를 유지한다. §15와 직전 커밋이 구현/전체 빌드 근거를 소유한다.
- 사용자가 에디터 재시작 후 `AG_LocomotionSpike`와 임시 발판의 Unsaved 항목을 저장했다. 현재 AG를 읽기 전용 Export해 발 배치 연결11개(입력9/포즈2)·공개핀10개·v2/Legacy 없음·Bool 알파·0.3/0.2초·레이캐스트/IK/루트 회전 보존 설정이 기존 계약과 일치함을 확인했다. **재저장된 AG를 원복하지 않고 이번 커밋에 포함한다.** HEAD와의 완전한 의미 동등성이나 재시작 후 새 PIE 포즈 시험을 주장하지 않는다.
- 신규 `/Game/Characters/UAFSpike/BP_UAFYawTestPlatform`은 StaticMeshActor·Cube1600×1600×30cm·Movable·LNPDynamicTerrain 충돌·물리/중력 시뮬레이션 끔·기본 RotatingMovement 로컬 Yaw30°/초다. 재사용할 BP만 유지한다. PIE Python 스폰은 API 미노출로 생성 전에 실패해 중단했고 `Scripts/AnimOverhaul/SpawnUAFYawTestPlatform.py`는 제거했다. 재시도하지 않는다.
- **정리 완료:** `EditorPlacement.json`의 정확한 `UAFYawTestPlatform_Temporary` Actor를 제거했고 조회 결과 빈 배열이다. 저장됐던 외부 Actor 파일도 해당 빈 패키지만 save_packages로 처리해 삭제 확인했다. 재배치 없이 과거 Teleport 명령을 실행하지 않는다. 맵 전체를 저장/원복하지 않았다.
- 마지막 조회는 **PIE=false·Slate observer 해제**다. Rewind에는 `20261027_163122363.utrace` 약10초의 UAF EvaluationProgram이 표시돼 있었으며 사용자도 파일/시점을 확인했다. 다음 세션은 과거 widget/object ref를 재사용하지 않는다.
- 재시작 후 CVar는 FootPlacement1·FootPlacementOnMovingBase0·Steering1·StrafeWarping1·DebugLocomotion0이다. FootPlacement 비교는 `LNP.Anim.FootPlacement`, Steering/Strafe는 `LNP.UAFSpike.*`다.
- TestMap03의 GameMode Override는 `BP_GameModeUAFSpike` 임시 설정이다. 기존 사용자 맵 변경과 섞여 있으므로 전체 원복하지 않는다. 다른 체크아웃에서는 다시 지정한다. 5-1 종료 시 원래 값은 기록돼 있지 않으므로 사용자 기준값을 먼저 확인한다.

## 미판정 항목과 다음 판정의 주의점

- 본별 포즈/샘플별 보정의 완전 동등성, 목표 전환 클립 실제 재생·지속 고속 품질, 적/네트워크·무기 레이어/몽타주/ANS·루트모션 및 최종 일곱 게이트가 남는다. 이 목록 전부를 새 시험의 필수 교차 조합으로 자동 확대하지 않고 다음 단위에서 최초 조건과 대응한다.
- 공중 해제·착지 복귀는 코드/배선/관찰과 task로 확인했지만 ForceReset=true 정확한 한 프레임은 직접 조회하지 않았다.
- 기존 Foley 이벤트 미처리 경고가 남는다. 큰 Yaw 구간 LogAnimation Error0·assertion/ensure/fatal0과 구분한다. 녹화 종료 직후 LogTrace error64는 실제 저장된 과거 task 조회가 성공한 사실과 함께 기록했고 억제하지 않았다.
- 숨김 ABP와 UAF가 함께 평가되는 중간 구성이다. 이번 기록으로 성능을 비교하지 않는다.

## 로컬 근거와 조회 경로

`Saved`는 git 제외이므로 다른 체크아웃에 따라가지 않는다. 같은 로컬 프로젝트에서 아래 파일을 읽고 필요할 때만 녹화를 다시 연다.

| 기록 | 위치/용도 |
|:---|:---|
| ResetRoot/CopyBone | `Saved/AnimationOverhaul/2026-10-27_ResetRootCopyBone/`의 ContractInspection·MainNodePins·FallLoopAndGraph JSON |
| 추가 전환 | `Saved/AnimationOverhaul/2026-10-27_AdditionalTransitions/`의 Targets·SprintTurns·GaitTransitions·RunPivot JSON. 녹화는 실행서 §17 |
| 패널 점프/착지 | `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261027_153613941.utrace`(148,240,307바이트·약68.452초), `Saved/AnimationOverhaul/2026-10-27_BaseJumpRotation/`의 로그/집계·라이브 자세·공중/지상 task JSON |
| 큰 Yaw 롱소드 | `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261027_163122363.utrace`(109,352,237바이트·약41.469초). `Saved/AnimationOverhaul/2026-10-27_YawPlatform/`의 Setup·LiveYawRotation·YawRecordSummary·RewindYawTasks·Cleanup JSON, YawRecord.log |
| AG 재시작 검사·백업 | 위 YawPlatform의 `AfterRestart/`: 현재 AG·커밋 전 HEAD·외부 Actor 백업, AG_CurrentExport.txt·AGContractInspection·AGExposedPinsInspection JSON |
| 직전 발 배치 근거 | `Saved/AnimationOverhaul/2026-10-26_UAFFootPlacement/`, `Saved/AnimationOverhaul/2026-10-26_PreFootPlacement/`. 경사/적도 녹화 `20261026_201850667.utrace`·35초 공중/40초 지상. 과거 발판 입력 녹화 `20261026_202321824.utrace`·5초 |

- 실제 task는 `UAFLocomotion → S_LocomotionSpike → UAFAnimGraph_* → AG_LocomotionSpike → EvaluationProgram → Tasks`, 공유 입력은 상위 AG의 Native Properties에서 읽는다. 빈 Details나 새 PIE 뒤 빈 트랙을 기록 부재로 해석하지 않는다.
- 이번 사용자 AG 재저장 전 기준 커밋은 `31d40d6`이다. 위 백업과 읽기 전용 검사 자료를 사용하며 필요 없이 파일을 덮어쓰지 않는다.

## 커밋 범위·보존 대상·빌드 주의

- 이번 인계 커밋 범위는 문서4개(Current·Roadmap·Phase05 실행서·이력), 검사 후 유지한 AG, 신규 Yaw 시험 BP다. 이번 세션에서 C++/엔진/기존 CR·Chooser·PSD·GASP 소스는 변경하지 않았다.
- **기존 사용자 변경4개는 보존하고 커밋에서 제외한다:** `Content/Maps/TestMap03.umap`, `Content/Gameplay/DT_PlayerBaseStats.uasset`, `Content/ItemData/Weapons/Levels/DT_LongSword_Levels.uasset`, `Content/SpawnData/DA_MassSpawnConfig_TestOnePerSpecies.uasset`. 정리 후 해시가 저장 전과 모두 일치한다(`YawPlatform/Cleanup.json`). 다음 세션에서 이 파일들이 수정 상태로 남아 있어도 실패나 누락으로 해석하지 않는다.
- 직전 §15의 Development 전체 빌드는8 actions·43.72초·종료 코드0이었다. 이후 C++ 변경이 없어 재빌드하지 않았다. 이번 BP는 컴파일·설정 재조회·수동 실행 검증을 완료했고 문서 diff 검사를 수행했다.
- 구조체/trait 레이아웃을 바꾸면 에디터 종료 후 전체 빌드를 우선한다. 앞선 Strafe Live Coding은 완료 알림 Slate/mimalloc 경로에서 크래시했고 원인은 미확정이다(§14). Build.bat은 처음부터 권한 확장, `LootNPopEditor Win64 Development <uproject> -WaitMutex -NoHotReloadFromIDE`. 전체 빌드와 Live Coding은 동시에 돌리지 않는다.
- GASP 참조 에디터는 불필요하며 쓰기 금지다(AN-009·AN-018). 소스 확보/리타깃 재생성은 README §4와 Phase01C/Phase02를 따른다.

## 다른 트랙

- ISKM 총기 적 재베이크는 `TechDesign_EnemyNPC_LowLOD.md`의 미해결 절이며 Phase 5와 독립이다.
- 1B 마커리스 모캡은 플러그인 설치·영상 선택 후 별도 착수(AN-010), Locomotor 사족 드론은 최후순위 별도 트랙(AN-008)이다.
- 한 번에 한 세션만 진행한다. 새 세션은 위 다음 단위부터 이어가고 이 세션에서 추가 구현하지 않는다.
