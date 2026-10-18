# Phase 2 작업 로그

> 소유 정보: Phase 2(스켈레톤 전환과 GASP Mover ABP 재구성)의 날짜별 수행 내용·검증 결과·시행착오. 현재 유효한 단위 계획·재생성 절차는 `phases/Phase02_GASPMoverABP.md`가 소유한다.
> 원시 결과는 저장소 밖 `Saved/AnimOverhaul2-0a/`·`Saved/AnimRetarget/`(git 제외)에 있다. 이 로그에는 요약과 파일 이름만 둔다.

## 2026-10-18 — 단위 2-0a Lyra 세트 UEFN 리타깃

LootNPop 에디터(8000)만 썼다. GASP 에디터는 쓰지 않았다. 런타임 참조(`ABP_Lyra`·`CHT_Montage`·GA·`BP_LNP*`·`ABP_Retarget`·런타임 리타기터)는 바꾸지 않았다.

### 1. 실사용 목록 재집계

- 0-3 방법 그대로: git 추적 `.uasset` 255개(Maps·외부 액터 제외, 1C-2 경로 이동 반영)를 루트로, 라이선스 폴더(`Lyra_Mannequin`(MotionMatchingData 제외)·`UEFN_Mannequin`·`MoverSample_Mannequin`·`Weapons`(LongSword 제외)·`Retargeted`·GASP 독립 에셋) 안에서만 `get_dependencies`를 따라갔다. 추적 에셋에 닿으면 그 에셋 몫으로 넘긴다. `ProgrammaticToolset` 스크립트 1회, 조회 586회. 존재하지 않는 의존은 `PSD_SM_CMC_Idles` 1개(1C-3 Removed 리다이렉트, 정상). 결과 `Saved/AnimOverhaul2-0a/reach.txt`.
- `Lyra_Mannequin/Animations` 사용 121개 — 0-3과 같다. PSD 경로 30, 서브 ABP 경로 37, 몽타주 경로 55, 겹침 1(`MM_Rifle_Idle_ADS`) → **91개**(MM 59·AM 21·A 9·AO 2).
- 0-3과 다른 점 2가지:
  - `CHT_Montage`가 `Weapons/{Pistol,Rifle,Shotgun}/Animations/AM_MM_*_{Fire,Reload}` 6개를 참조한다. 스켈레톤이 `SK_Mannequin`인 캐릭터 몽타주인데, 0-3은 `Lyra_Mannequin/Animations`만 세어 빠졌다(세그먼트 시퀀스는 55개 안에 있었다).
  - `ABP_Lyra`가 직접 참조하는 `MM_Unarmed_Jump_Fall_Loop`(AN-007)은 `PSD_Jump_Lyra`도 참조해 0-3에서 PSD 경로로 셌다. 2-0b에서 메인 ABP가 UEFN이 되면 UEFN 판이 필요하므로 넣었다.
- 확정 목록 98개: `Scripts/AnimRetarget/LyraToUEFN_RetargetList.txt`. C++·Config에 Lyra 애니 경로 문자열 참조는 없다(grep 0).

### 2. 리타기터

- `RTG_Lyra_to_GASP`(원본 `IK_Mannequin` → 대상 `IK_UEFN_Mannequin`): 대상 체인 30개 모두 매핑, 체인 본은 모두 93본 안에 있다. 리타깃 루트는 양쪽 `pelvis`.
- 시험 배치(3클립) 뒤 대조에서 루트모션 소실이 나왔다. 사본의 `root`는 원점에 고정되고 이동량이 골반에 들어갔다(`MM_Dash_Forward` root 875cm → 0, pelvis가 846cm 이동). 원인: Root Motion op의 `SourceRoot`·`TargetRoot`·`TargetPelvis`가 빈 이름이다. `RTG_Lyra_to_MS`·`RTG_GASP_to_MS`도 같다.
- 발가락: 대상 `LeftToe`·`RightToe` 체인이 원본 `LeftLeg`·`RightLeg`에 매핑돼 있다(원본 IK 리그에는 발가락 체인이 없다). `ball_l`이 원본에서 정지인데 사본은 107° 흔들렸다.
- 전역 설정 `bCopyBasePose`는 5.8 op 스택에서 효과가 없다(결과 동일). 대신 `IKRetargetCopyBasePoseOp`를 스택 맨 앞에 넣었다.
- 런타임 리타기터는 적 `VisualMesh`가 쓰므로 고치지 않고 복제본 `RTG_Lyra_to_UEFN_Offline`(추적)에 루트 본 지정·발가락 매핑 해제·Copy Base Pose를 넣었다(`Scripts/AnimRetarget/SetupOfflineRetargeter.py`).

| 측정(시험 3클립, 로컬 회전 최대 차) | 런타임 리타기터 그대로 | 루트·발가락만 수정 | + Copy Base Pose(채택) |
|:---|:---|:---|:---|
| 대시 root 이동(원본 875cm) | 0 | 850 | 850 |
| `A_SW_Damage_Backward` root(원본 −109cm) | 0 | −106 | −106 |
| `ball_r` 원본 대비 | 최대 80°(원본 범위 32°) | 원본 움직임 소실(0°) | **0°** |
| 트위스트 본(`lowerarm_twist_01_l` 등) 원본 대비 | 최대 121° | 기준 포즈 고정 | **0°** |
| `foot_l`·`hand_r` | IK·비율 차이로 20~38° | 같음 | 같음 |

루트 이동이 약 3% 줄어드는 것은 두 메시 골반 높이 비(75.7/78.0)만큼 스케일되기 때문이다.

### 3. 배치 실행 경로

- MCP 툴셋(`list_toolsets`)에 IK Retargeter 배치 도구가 없고, `ProgrammaticToolset`은 `unreal` 모듈을 가져올 수 없다(허용 모듈 json·copy·math·time·datetime·re).
- **에디터 Python을 MCP로 실행했다.** `SlateInspectorToolset.Snapshot`으로 상태 표시줄 `Cmd` 콤보 옆 `textbox`(이 세션 ref `tb5`)를 찾아 `Click` → `Type("py \"<스크립트>\" <인자>", submit=true)`. 결과는 스크립트가 JSON 파일로 쓰고 Bash로 읽었다. Output Log 탭 안의 입력창(`tb3`)은 배치가 Content Browser 탭을 앞으로 가져오자 사라져 `Type`이 `false`를 돌려줬다.
- API: `unreal.IKRetargetBatchOperation.run_batch_retarget(IKRetargetBatchOperationInputs)`(5.8에서 `duplicate_and_retarget`은 deprecated). `target_path` 하나로 평평하게 출력, 접미어 `_UEFN`, `include_referenced_assets`·`overwrite_existing_files`·`retain_additive_flags` = True.

### 4. 산출

- 본 실행: 요청 98 → 애니 98개 생성(MM 60·AM 27·A 9·AO 2), `Content/Characters/Retargeted/Lyra/`. 원본 ABP 5개를 같은 배치에 넣어 `ABP_Sub_*_UEFN`도 만든 뒤 `/Game/Characters/`로 옮겼다. 저장은 대상 폴더 98개와 ABP 5개만 경로를 지정해 했다.
- 서브 ABP: 부모 `ULNPAnimInstance`, `ALI_WeaponStyles`(스켈레톤 없음), 대상 스켈레톤 `SK_UEFN_Mannequin`, 미리보기 `SKM_UEFN_Mannequin`, 의존은 `Retargeted/Lyra/` 사본만. `compile_blueprint(warnings_as_errors=True)` 5/5 통과.
- 저장 뒤 dirty 패키지 0. `git status`: 새 추적 파일은 ABP 5개·`RTG_Lyra_to_UEFN_Offline`·`Scripts/AnimRetarget/`뿐, `Retargeted/Lyra/`는 `.gitignore`로 빠진다.

### 5. 몽타주 대조

- `Scripts/AnimRetarget/CompareLyraRetarget.py`로 98쌍(몽타주 27·시퀀스 69·AO 2)을 비교했다. 결함 차이 0, 사본 누락 0.
- ANS 5종은 `AM_SW_Attack_01`에만 있고(각 2개, 시간·길이 일치) 다른 몽타주에는 원본에도 없다. 재장전 3개의 `PlaySound` 각 3개도 남았다. 섹션은 `AM_SW_Attack_01`만 3개(`Section_1`·`NoUse`·`Section_2`). 슬롯 `AdditiveHitReact` 11·`DefaultSlot` 6·`FullBody` 4·`FullBodyAdditivePreAim` 3·`UpperBody`+`UpperBodyAdditive` 3.
- `bEnableRootMotion`은 `A_SW_Attack_01/03`·`A_SW_Damage_Backward`·`Fall_Loop`에 켜져 있고 사본도 같다(Attack은 원본도 root 이동 0). 가산 시퀀스 48개의 타입·기준 포즈 유형이 같고, `RefPoseSeq` 41개가 모두 사본을 가리킨다.
- 차이로 남은 것: 몽타주 그룹 14건(피격 11 `AdditiveGroup` → `DefaultGroup`, 재장전 3 `DefaultGroup` → `Partials`). UEFN 스켈레톤 슬롯 그룹이 `DefaultGroup{DefaultSlot}`·`Partials{UpperBody}`뿐이라서다. 2-0b 결정 B로 넘겼다.
- 썸네일(첫 프레임) `A_SW_Damage_Backward` 원본·사본이 같은 자세다.

### 6. 가상 본

`ABP_Lyra`·`ABP_Sub_*` 5개 uasset 이름 테이블에 `VB ` 문자열이 없다. 본 이름은 `ABP_Sub_LongSword`의 `hand_l`·`spine_02`(TwoBoneIK·레이어 블렌드), 무기 3종의 `spine_02`, `ABP_Lyra`의 `spine_01`뿐이다.

### 시행착오·주의

- **엔진 문제 1 — 원본 오염.** `FAnimTrack::ReplaceReferredAnimations`(`AnimCompositeBase.cpp:474`)가 세그먼트의 원본 시퀀스에도 `ReplaceReferredAnimations`를 부른다. 첫 본 실행 뒤 메모리상의 Lyra 원본 `MM_*_Fire`·`MM_HitReact_*`의 `RefPoseSeq`가 사본을 가리켰다. dirty 표시가 없어 dirty 목록·디스크(수정 시각 7월)에는 드러나지 않는다. `EditorLoadingAndSavingUtils.reload_packages`로 133개를 다시 읽어 되돌렸고, 재생성 스크립트가 매번 같은 처리와 확인을 한다.
- **엔진 문제 2 — 가산 기준 포즈.** `retain_additive_flags`의 복원이 참조 교체 전 값을 쓴다. `MM_HitReact_Back_Lgt_01_UEFN`의 `RefPoseSeq`가 Lyra `MM_Rifle_Idle_ADS`를 가리켰다(스켈레톤 불일치). 스크립트가 사본으로 다시 잇는다.
- `run_batch_retarget` 반환값에는 ABP가 없다(엔진 코드가 애니만 담는다). `find_assets`로 확인해야 한다. `overwrite_existing_files`는 ABP에 적용되지 않아 시험 때 만든 ABP 때문에 `ABP_Sub_Pistol_UEFN1`이 생겼다. 시험 산출물을 모두 지우고(서브 ABP 5개·`Retargeted/Lyra/`·시험 폴더 3개) 확정 리타기터로 처음부터 다시 만들었다.
- `AssetTools.move`가 옮긴 ABP 패키지를 즉시 저장했다(참조하던 사본 21개도 함께). 결과에는 영향이 없었지만, 이동은 저장을 동반할 수 있다.
- 비교 스크립트 API 메모: `AnimNotifyEvent.duration`은 protected → `AnimationLibrary.get_anim_notify_event_duration`. 몽타주 `composite_sections`는 Python·`ObjectTools` 모두 못 읽는다 → `get_num_sections`·`get_section_name`만. 몽타주의 루트모션 플래그 프로퍼티는 없다. `get_animation_notify_event_names`는 블렌드 스페이스를 받지 않는다. 루트 이동은 `get_bone_pose_for_frame(anim, "root", frame, False)`로 쟀다.
- 런타임 리타기터의 결함(루트 본 미지정·발가락 매핑)은 지금 `ActorPromoted` 적 `VisualMesh`에 그대로 있다. 2-0b에서 적은 포즈 복사로 바뀌어 빠진다. `RTG_GASP_to_MS`는 2-0b에서 플레이어 경로가 되므로 같은 확인이 필요하다.
