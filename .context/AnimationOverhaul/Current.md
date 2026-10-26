# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: **Phase 5, 5-0 완료·5-1 진행 중. 발 배치 Control Rig 최소 연결·검증 단위 완료(Phase05 §15). 새 세션 인수인계 완료·다음 단위 진행 확인 대기.**
> 마지막 갱신: 2026-10-27

Phase 0·1A·1C·2·3은 완료, 1B는 대기다. Phase 5의 최종 일곱 게이트는 아직 통과하지 않았고 **5-1 미완료·5-2 미착수·Go/No-Go 보류**다. 이번 단위 완료를 UAF 마이그레이션 승인으로 해석하지 않는다.

## 바로 다음 행동과 승인 경계

1. 세션 시작 시 CLAUDE.md·README·이 문서·Decisions를 읽고 `phases/Phase05_UAFSpike.md` §15의 종료 판정부터 확인한다.
2. **다음 제안은 ResetRoot/CopyBone의 남은 계약 대조다. 사용자 진행 확인 전 조사·구현에 착수하지 않는다.** 이후에도 구현 단위마다 계속 진행할지 확인한다. 수동 조작·배선·플레이가 빠르면 즉시 요청한다.
3. 완료한 발 배치 단위의 빌드·녹화를 인계만을 위해 반복하지 않는다. 새 코드나 재현 문제가 생길 때만 필요한 검증을 추가한다.
4. 2026-10-27 사용자 요청은 인수인계 정리와 이번 단위의 단일 커밋까지다. ResetRoot/CopyBone 조사·구현은 승인되지 않았다. 새 세션 시작 메시지는 `@.context/AnimationOverhaul/README.md @.context/AnimationOverhaul/Current.md 문서를 확인하고 다음 단위 진행 여부부터 확인해줘.`로 전달한다.

## 이번 단위의 결과

상세 구현·기록·한계는 `phases/Phase05_UAFSpike.md` §15가 소유한다.

- 기존 구면 보정 CR `/Game/Characters/ControlRigs/CR_Biped_FootPlacement_UEFN`을 수정 없이 참조한다. 신규 **Control Rig(v2) + Passthrough Blend** 스택이 최종 구면 루트 보정 뒤에 있다. Bool 알파는 지상에서 켜고 진입0.3초·해제0.2초다. `UseIKBoneTargets=false`, `DoRaycast=true`다.
- `LNPUAFSpikeAnimInstance.h/.cpp`에 FootPlacement 공유 입력을 추가하고 `LootNPop.cpp`에 RigVM 타입을 등록했다(C++ 3파일). 접지 법선·활성·핀 고정·경사·디버그는 기존 공급층 함수를 재사용하며 최초 활성/착지/CVar 재활성화 때 ForceReset을 공급한다. base 이동/텔레포트는 기존 OffsetRoot 입력을 재사용한다.
- v2의 변수 노출 UI 등록 누락을 로컬 엔진에서 확인했다. 임시 Legacy 전환으로 UI를 확인했으나 사용자 우려로 최종 채택하지 않았고 **Legacy 스택은 제거했다.** 기존 엔진 Export/Import API로 v2 `AnimNextTraitStack_3`의 공개 변수10개를 생성하고 포즈 Input 레이아웃도 복구했다. 엔진 수정·추가 런타임 trait·MCP 도구는 만들지 않았다.
- 사용자 배선 후 실제 링크11개(입력9·포즈2)를 읽어 대조했고 AG/S Compile·Save를 확인했다. Has Teleported는 OffsetRoot.bReset이다.
- **전체 빌드 성공:** 사용자 에디터 종료 후 권한 확장 Editor Development, 8 actions·43.72초·종료 코드0. 그 뒤 C++ 변경은 없으며 Live Coding은 사용하지 않았다.
- **수동 관찰:** 북극 기본 동작 정상. 북극 주변 경사0/1에서 튐 없고 1의 발 접지가 훨씬 자연스럽다는 사용자 평가. 적도에서 여러 방향 걷기·달리기·점프·정지도 어색함 없음. 움직이는 발판도 어색함 없음 보고.
- **실제 Rewind 평가:** 경사/적도 녹화35초의 안정 공중에는 작업10개·CR task 없음. 40초 지상에는 작업12개 중 마지막에 Control Rig task가 있어 최종 구면 보정 뒤 실행과 공중 해제/지상 복귀를 확인했다.
- **발판 입력:** 첫 발판 녹화5초 Native Properties에서 OnGround/OnMovingBase/Enabled=true, AllowFootPinning=false, AllowSlopeWarping=true. 연결에 따라 핀 고정·루트 감쇠를 끄고 경사 보정은 유지한다.

## 검증 한계와 남은 게이트

- `[UAFRoot]`는 **CR 앞**의 진단이다. 경사/적도 기록의 거리≤30cm·Yaw≤90°·Tilt0.000°를 발 배치 뒤 최종 본 포즈 수치로 확대하지 않는다. 전체 본 배열 대조와 기존 ABP의 샘플별 보정 완전 동등성은 미판정이다.
- 착지/재활성화 리셋은 코드·배선·튐 없는 관찰과 task 복귀로 확인했다. `ForceReset=true`인 정확한 한 프레임은 직접 조회하지 않았다.
- 짧은 발판 녹화 두 개는 전부 지상이다. 정지·이동 기록은 있으나 발판 위 점프/착지의 직접 녹화 근거는 없다. base 회전 최대0.0026°라 큰 회전 base 검증도 남았다.
- ResetRoot/CopyBone 잔여 계약, 추가 전환 표본(Pivot·Sprint Turns·GaitTransitions 등), 적/네트워크·무기 레이어/몽타주/ANS·루트모션 전체 게이트가 남았다.
- 기존 Foley 이벤트 미처리 경고는 남는다. 경사/적도 기록에서 LogAnimation Error0·검색한 CR/RigVM/Assertion/Ensure 오류0과 구분한다. 경고 전체 억제나 Foley 구현은 하지 않았다.
- 숨김 ABP와 UAF가 함께 평가되는 중간 구성이다. 이번 결과로 성능 비교를 하지 않는다. 최신 v2 구성의 별도 에디터 재시작 후 재로드 시험은 이번 단위에서 반복하지 않았다.

## 현재 구성·에디터 상태

- 숨김 `ABP_SourceUAFSpike`가 Chooser 다중 PSD·Mover 궤적·상태를 공급하고 UAF가 VisualMesh의 UEFN 포즈를 평가한다. 시스템 locator는 `Current Actor → VisualMesh`, UAF PrePhysics는 숨김 메시 뒤에 실행된다.
- AG 흐름: MM/Fall_Loop Bool + Pose History → Passthrough/Strafe/입력 어댑터 → Offset Root/최종 구면 보정 → **Control Rig(v2)/Passthrough Blend → Output**. 기존 B0·공중 고정·Strafe/Steering 계약은 유지한다.
- 현재 스파이크 C++는 `Source/LootNPop/Animation/UAFSpike/`, 테스트 에셋5개는 `Content/Characters/UAFSpike/`다. 원래 ABP/표시 ABP/무기 레이어/CR·Chooser·PSD·GASP 소스는 이번 단위에서 수정하지 않았다.
- 마지막 사용자 조작은 PIE 종료 후 저장 녹화 조회다. 마지막 조회 대상은 발판 `20261026_202321824.utrace`의 약5초 AG Native Properties다. 10월26일 종료 확인에서는 PIE/녹화가 종료됐고 AG/S 저장이 성공했으며 추가 Slate observer는 해제했다. 10월27일 인계에서는 에디터를 조작하지 않았다. 새 세션에서는 PIE/CVar/재생 헤드를 다시 확인하고 과거 ref를 재사용하지 않는다.
- 마지막 확인 CVar는 FootPlacement=1·FootPlacementOnMovingBase=0·Steering=1·StrafeWarping=1·DebugLocomotion=2다. FootPlacement 비교에는 `LNP.Anim.FootPlacement`를 사용한다.
- TestMap03의 GameMode Override는 `BP_GameModeUAFSpike` 임시 설정이다. 기존 맵 변경과 섞여 있으므로 맵 전체를 되돌리지 않는다. 다른 체크아웃에서는 다시 지정한다. 5-1 종료 시 원래 값으로 복원하되 구체 원래 값이 기록돼 있지 않아 사용자 기준값을 먼저 확인한다.

## 로컬 기록과 조회 경로

| 기록 | 위치/의미 |
|:---|:---|
| 경사0/1·적도 | `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261026_201850667.utrace`(363,654,323바이트) |
| 발판 정지·이동 | `...20261026_202321824.utrace`(28,886,782바이트, 약10.223초) |
| 발판 정지 | `...20261026_202342105.utrace`(11,019,442바이트, 약3.934초) |
| 로그·집계·조회 근거 | `Saved/AnimationOverhaul/2026-10-26_UAFFootPlacement/`의 SlopeEquator/Panel 로그·Summary JSON과 Rewind35AirTasks/Rewind40GroundTasks/RewindPanel5Inputs JSON |
| 편집 전·중간 백업 | `Saved/AnimationOverhaul/2026-10-26_PreFootPlacement/`: 최초 AG/S, AG_BeforeLegacy, AG_BeforeV2Pins, NorthSmoke.log, 일회성 스크립트 |

Saved는 git 제외라 다른 체크아웃에 따라가지 않는다. 과거 Strafe 녹화·로그는 Phase05 §14, 공중/대표 전환 녹화 `20261025_133940695.utrace`는 §12~§13을 따른다.

- 실제 작업은 `UAFLocomotion → S_LocomotionSpike → UAFAnimGraph_* → AG_LocomotionSpike → EvaluationProgram → Tasks`에서 읽는다.
- 공유 입력은 상위 AG의 Native Properties다. 빈 Native Properties를 입력 없음으로 해석하지 않는다. SequenceInfo가 없어 Tasks를 읽은 경위는 §12에 있다.
- 이후 새 PIE로 Rewind 화면이 비어도 이미 저장된 파일의 부재/실패로 단정하지 않는다. 이번에는 과거 파일을 다시 열어 task와 입력을 직접 확인했다.

## 보존 범위와 빌드 주의

- 이번 단일 커밋 범위: C++ 3파일, AG_LocomotionSpike, Current·Roadmap·Phase05 실행서/로그의 총8파일. 2026-10-27 사용자 요청에 따라 구현·검증·인수인계 문서를 함께 기록한다. 커밋 식별자는 `git log -1`로 확인한다. 푸시는 요청되지 않았다.
- 기존 사용자 변경4개는 보존·제외한다: `Content/Maps/TestMap03.umap`, `Content/Gameplay/DT_PlayerBaseStats.uasset`, `Content/ItemData/Weapons/Levels/DT_LongSword_Levels.uasset`, `Content/SpawnData/DA_MassSpawnConfig_TestOnePerSpecies.uasset`.
- 구조체/trait 레이아웃을 바꾸는 다음 C++ 변경은 에디터 종료 후 전체 빌드를 우선한다. 앞선 Strafe Live Coding은 완료 알림 Slate/mimalloc 경로에서 크래시했고 원인은 미확정이다(§14). Build.bat은 처음부터 권한 확장, `LootNPopEditor Win64 Development <uproject> -WaitMutex -NoHotReloadFromIDE`. 전체 빌드와 Live Coding을 동시에 돌리지 않는다.
- GASP 참조 에디터는 불필요하며 쓰기 금지다. GASP 소스 에셋은 저장하지 않는다(AN-018). 소스 확보·리타깃 재생성은 README §4와 Phase01C/Phase02를 따른다.

## 다른 트랙

- ISKM 총기 적 재베이크: `TechDesign_EnemyNPC_LowLOD.md`의 미해결 절, Phase 5와 독립이다.
- 1B 마커리스 모캡: 사용자 플러그인 설치·영상 선택 후 별도 착수한다(AN-010).
- Locomotor 사족 드론은 최후순위 별도 트랙이다(AN-008).
- **한 번에 한 세션만 진행한다.** 각 트랙의 구체 절차·자산 소유권은 README/Decisions와 해당 Phase 문서가 소유한다.
