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

### 사용자 결정 (2-0a 보고 뒤, 같은 날)

- 결정 A: 임시 로코모션 Chooser는 `CHT_PSD_Lyra`를 복제해 `Characters/MotionMatchingData/CHT_PSD_UEFN`을 새로 만든다(백업 복원 기각).
- 결정 B: 몽타주 슬롯 그룹은 C++ 런타임 등록으로 맞춘다 → `Decisions.md` AN-020. 소스 스켈레톤 저장·슬롯 이름 변경 기각.
- 두 결정 모두 2-0b에서 실행한다. 이 기록에서 에셋·C++ 변경은 없다.

## 2026-10-18~19 — 단위 2-0b 전환

LootNPop 에디터(8000)만 썼다. 커밋 경계: 사전 작업 `69acbb3`(C++·새 구성 에셋, 런타임 미참조) → 전환·회귀 수정·문서.

### 1. 사전 작업

- 슬롯 그룹(AN-020): `ULNPAnimInstance::NativeInitializeAnimation`에서 Lyra 표를 `USkeleton::SetSlotGroupName`으로 등록. 엔진 확인: `SetSlotGroupName`·`AddSlotGroupName`은 `MarkPackageDirty`를 부르지 않는다. 몽타주 `PostLoad`는 슬롯을 `DefaultGroup`으로 `RegisterSlotNode`만 한다. 그룹 불일치 몽타주는 `Montage_Play`의 `HasValidSlotSetup()`에서 거부된다(경고 + 재생 안 함). 에디터 확인: 피격 `AdditiveGroup`, 재장전 `DefaultGroup`(두 트랙 같음), 대시·공격 `DefaultGroup`, `SK_UEFN_Mannequin` dirty 아님.
- 재장전을 C++가 끊는 경로: `ULNPAbility_Reload::OnDashExecuted` → `CancelAbility`, `ULNPEquipmentComponent` 무기 교체 → `CancelAbilities(Reload)`. 둘 다 `PlayMontageAndWait`가 몽타주를 멈춘다. `Montage_Stop`을 직접 부르는 곳(이동 취소 ANS·랙돌·경직)은 그룹과 무관하게 전부 멈춘다.
- Chooser 결과 행은 MCP·Python으로 쓸 수 없다(`UChooserTable::ResultsStructs` = 편집 플래그 없는 `UPROPERTY()`). `ConsolidateAssets`는 원본을 지운다. 그래서 에디터 명령 `LNP.Editor.ReplaceAssetRefs <대상> <원본>=<사본> ...`(`LootNPopEditor/Tools/LNPReplaceAssetRefsCommand.cpp`)를 만들었다. 첫 구현은 UPackage를 검색 루트로 넘겨 교체 0건이었다(패키지 직렬화에 참조가 없다) → 패키지 안 오브젝트마다 돌리도록 고쳤다(Live Coding). 새 cpp라 에디터 재시작·풀 빌드가 한 번 필요했다(사용자가 수행).
- `CHT_PSD_UEFN` 4쌍 9참조, `CHT_Montage` 26쌍 53참조 교체. 저장 뒤 의존성: 각각 `PSD_*_UEFN` 4개, `Retargeted/Lyra/*_UEFN` 26개만.
- `ABP_Main_UEFN`: `ABP_Lyra` 하나를 배치 리타깃(`RTG_Lyra_to_UEFN_Offline`, 참조 에셋 미포함) → 이동. 남은 참조 3개를 노드 프로퍼티로 교체, `warnings_as_errors` 컴파일 통과. 원본 `ABP_Lyra` dirty 아님. 이동 직후 프리뷰 인스턴스의 `Accessed None ... Predictor` 로그는 기존 노이즈.
- `ABP_VisualCopyPose`: `ABP_Retarget`(템플릿, 부모 `ULNPAnimInstance`) 복제 → 리타깃 노드를 Copy Pose From Mesh(`bUseAttachedParent`·`bCopyCurves`)로 교체, 이벤트 그래프·리타기터 변수 제거.

### 2. 전환

- 두 BP `AnimSourceMesh`: `SKM_UEFN_Mannequin` + `ABP_Main_UEFN` + `bDisablePostProcessBlueprint`. `UnarmedAnimLayerClass` → `ABP_Sub_Unarmed_UEFN`. 플레이어 `VisualMesh` 태그 `RTG_Lyra_to_MS` → `RTG_GASP_to_MS`. 적 `VisualMesh` → `ABP_VisualCopyPose`, 태그 비움.
- `GA_ParrySuccess_LongSword.ReactionMontage` → 사본. `GA_Stagger_LongSword`: 바이너리 이름 테이블에 `StaggerMontage`가 남아 있었다(C++에서 지운 프로퍼티, `ea15a25` 경직 통합 때). 컴파일·재저장으로 제거.
- `VS_*` 4개 `AnimLayerClass` → `_UEFN`. `ABP_Retarget` 리타기터 맵을 `RTG_GASP_to_MS` 하나로 줄여 Lyra 메시·IK 리그 하드 로드를 끊었다.
- 두 Pawn에서 의존성을 따라가 Lyra 참조 0(188개 방문). 자동 PIE(1회) 로그: 몽타주·슬롯·애니 경고 0.

### 3. 사용자 PIE 1차 → 수정

1. **ensure `SkeletonToCompactPose.IsValidIndex`**(`UBlendProfile::FillBoneScalesArray`, `FullBody` 슬롯). 대시 사본 4개의 `BlendProfileOut`이 Lyra `SK_Mannequin:FastFeet`. 2-0a 대조 스크립트가 블렌드 프로필을 보지 않았다. UEFN 스켈레톤에도 `FastFeet`(+`FastFeet_FastRoot` 등 GASP 변형)이 있어 이름으로 다시 이었다(`FixRetargetedMontages.py`).
2. 같은 점검에서 몽타주 사본 22개의 저장본에 원본 Lyra 시퀀스 의존성이 남은 것을 찾았다. 세그먼트는 사본이고, 메모리 참조를 교체 명령으로 훑으면 0건이다. 그냥 다시 저장해도 남는다. 교체 명령의 `PostEditChange`를 거친 뒤 저장하면 사라진다(섹션·노티파이 링크 정보로 추정). 98개 모두 의존 0.

### 4. 사용자 PIE 2차 → 수정

- **달릴 때 다리가 한 프레임씩 불가능한 각도로 튐(플레이어).** `RTG_GASP_to_MS`를 GASP 달리기 클립 2개로 오프라인 배치 리타깃해 본별 프레임 간 로컬 회전 최대치를 쟀다(원본 20~32°).

  | 구성 | 허벅지·종아리·발 | ball | 트위스트 |
  |:---|:---|:---|:---|
  | 디스크 원래 상태 | 100~178° | 150~179° | 0(고정) |
  | 체인 매핑 정리 + Copy Base Pose + 다리 OneToOne | 100~178° | 같음 | 원본과 같음 |
  | op 하나씩 끄기 | Run IK Rig만 끄면 원본과 같음 | 0(고정) | — |
  | 다리 체인만 IK에서 빼고 팔 IK 유지 | 60~118°(Relaxed 루프) | — | — |
  | 대상 리그 `IK_MS_FootGoal`(다리 끝·목표 foot, 다리 목표 솔버 분리) + IK 켬(채택) | **원본과 같음** | **원본과 같음** | 원본과 같음 |

  원인 정리: 자동 매핑이 원본 리그에 없는 체인을 엉뚱하게 이었고(트위스트 ← 손가락 중수골·쇄골, FootIK ← Toe, HandIK ← Index), MS 리그 다리 목표는 `ball`인데 UEFN은 `foot`이라 IK가 다리를 뒤집었다. 전신 솔버라 다리 체인만 빼도 다리 목표가 남아 무릎이 뒤집혔다. op 설정의 `ik_rig_asset`은 `set_settings`로 바뀌지 않는다 — `assign_ik_rig_to_all_ops`를 써야 한다.
- **멈춰도 달리기가 계속 재생되고 제자리 대시·점프 뒤에야 Idle로 돌아감(플레이어·적 공통).** UEFN PSD 4개의 `normalization_set`이 비어 있었다(Lyra PSD는 `PSN_CoreLocomotion_Lyra`). `PSN_CoreLocomotion_UEFN`을 연결했다. 나머지 DB 비용 설정(continuing·looping bias, PCA KD-tree, 이웃 200, 주성분 4)은 Lyra와 같다.

### 5. 사용자 PIE 3차 → 수정

- **무기 왼손 위치 이상**(총기는 오른쪽으로, 롱소드는 몸 쪽으로). 4절 중간 단계에서 IK op를 통째로 꺼 팔 IK도 꺼졌기 때문이다. 최종 구성(팔 IK)에서 조준 아이들의 "오른손 공간의 왼손 위치"를 쟀다: 전환 전 Lyra → MS 결과 라이플 (−31.0, 10.4, 1.3)·권총 (0.9, 7.0, −1.3) / 지금 UEFN 사본 → MS 결과 (−29.9, 9.8, 0.0)·(1.2, 7.1, −1.3), 차이 1.8cm·0.3cm. 2-0a UEFN 사본 자체는 Lyra 원본과 약 8cm 다르지만 팔 IK가 되돌린다.
- **방향과 무관하게 정면 달리기만 재생.** 회귀다(Lyra DB는 Fwd/Bwd/Left/Right). 사용자 결정으로 2-2에서 복구하고 2-0b는 알려진 차이로 기록했다(`phases/Phase02_GASPMoverABP.md` 2-0b 결과).

### 6. 사용자 PIE 4·5차 → 무기 왼손 IK·견착

- **총기 왼손 1~2cm 잔차.** 리타깃 설정만으로 줄일 수 있는지 임시 사본으로 4가지를 쟀다(오른손 기준 왼손 위치, 전환 전 Lyra 대비): 지금 구성 라이플 1.8·권총 0.3cm / 런타임 Blend To Source 3.9·0.5 / 오프라인 Blend To Source 4.3·2.6 / 둘 다 5.0·1.8. 지금이 최선이라 표시 메시 IK로 갔다. `Weapon Goals` op는 `ik_hand_*`가 GASP 로코모션 값이라(서브 ABP가 spine_01 이하만 덮는다) 쓸 수 없다.
- 총기 메시에는 `LeftHandGrip` 소켓이 없다(롱소드 `SK_LongSword` 스켈레톤에만). `DisableLHandIK` 커브는 Lyra 라이플 재장전 두 클립에만 있다 → 슬롯 몽타주 가중치를 함께 쓴다.
- C++: `ULNPWeaponVisualSet`에 `bUseLeftHandGripOffset`·`LeftHandGripOffset`, `ALNPCharacterBase::GetAppliedVisualSet()`, `ULNPAnimInstance::LeftHandIKAlpha`·`ComputeLeftHandOffDemand()`. UPROPERTY 추가라 사용자가 에디터 재시작·풀 빌드.
- 오프셋(무기 공간 cm, 조준 아이들): 라이플 (6.49, 29.70, 6.95)·샷건 (7.30, 27.81, 6.90). 힙파이어와 차이 2.5·2.7cm, 권총 3.2cm(권총은 넣지 않음). 서브 ABP 기본 자세가 조준 아이들이라 조준 값을 쓴다. 사용자 PIE: 왼손 깔끔하게 일치.
- **견착이 몸 쪽으로 당겨짐(개머리판이 어깨 관통, 라이플·샷건).** 오른손의 `spine_05` 기준 위치(Lyra 대비): 2-0a UEFN 사본 2.5cm, 최종 MS 9.5cm → 런타임 홉에서 약 7cm가 생긴다. IK 끄기·팔/척추 FK OneToOne은 효과 없음(8.0~13.0cm). 두 리타기터 모두 리타깃 포즈가 정렬 안 된 `DefaultPose`였다.

  | 변형 | 라이플 | 샷건 | 권총 |
  |:---|:---|:---|:---|
  | 지금 | 9.5 | 10.4 | 8.5 |
  | 런타임 대상 포즈 자동 정렬(채택) | 4.2 | 4.3 | 1.2 |
  | 런타임 원본 포즈 정렬 | 4.5 | 4.5 | 6.8 |
  | 오프라인 1차 대상 정렬 + 런타임 정렬 | 3.6 | 3.8 | 4.5 |
  | 오프라인 1차만 정렬 | 10.9 | 11.8 | 8.3 |

  처음에는 런타임 대상 포즈만 정렬했다(98개 재생성 불필요). 사용자 PIE에서 개머리판이 어깨 위로 떠 머리를 파고들었다. 축별로 보니 총(오른손)은 컴포넌트 공간에서 Lyra와 1cm 이내였고, 상체가 뒤로 3~4cm 밀려 있었다. 척추 체인 모드(OneToOne·MatchChain)·팔 IK 끄기·Pelvis op 끄기는 모두 ≥4cm로 효과가 없었다 → 무기별 견착 보정(`RightHandCorrection`, 오른팔 IK)을 C++로 추가했다(재시작 1회). 수동 값 Z −3.5 → −7.0cm로 맞추던 중 사용자가 "권총이 고개를 숙였다"고 보고했다.
- **총구 27° 하향.** 무기 +Y의 컴포넌트 공간 방향을 Lyra와 비교: 정렬 없음 0°, 자동 정렬 27.2°(세 총 모두, 위팔 13.2°·아래팔 21.8° 오프셋). 손 회전 오프셋만 0으로 되돌리는 시도는 회전값 설정이 틀려 무의미(70~87°). → **정렬을 되돌리고**(대상 포즈 `Default Pose`, `AutoAligned` 삭제) 견착 보정값을 정렬 없는 결과 기준으로 다시 계산했다(오른손의 `spine_05` 기준 위치를 Lyra와 같게 하는 이동량을 무기 공간으로): 라이플 (0.53, 8.70, −3.75)·샷건 (0.09, 9.68, −3.88)·권총 (1.90, 6.66, −4.90). 사용자 PIE: 세 총 견착·총구·왼손 정상.
- **ISKM 총기 적**(지상 원거리 샷건)도 견착·왼손이 맞지 않는다는 보고 → 2-0b 이전부터 있던 구운 클립 문제라 별도 세션으로 기록만 했다.

### 시행착오·주의

- 리타깃 포즈 정렬 여부는 `IKRetargeterController.get_current_retarget_pose_name`과 `get_rotation_offset_for_retarget_pose_bone`(Quat 반환)으로 본다. ⚠️ 자동 정렬은 위치만 보고 판단하면 안 된다 — 손목 방향(총구)이 함께 바뀐다. 기본 포즈 이름은 `Default Pose`(공백 있음)라 `set_current_retarget_pose("DefaultPose")`는 조용히 실패한다.
- `ComputeLeftHandGripOffsets.py`는 왼손 그립 오프셋과 견착 보정을 함께 계산한다. 런타임 리타기터(`RTG_GASP_to_MS`)나 사본을 다시 만들면 `apply`로 다시 돌린다.
- PIE를 끈 직후 Cmd 입력창에 `Type`하면 `true`를 돌려줘도 실행되지 않을 수 있다(로그에 `Cmd: py` 줄이 없다). `Click` → `Ctrl+A` → `Type`으로 다시 넣는다.
- `python3`는 이 PC에서 Windows 스토어 별칭이라 멈춘다. 엔진 번들 파이썬(`Engine/Binaries/ThirdParty/Python3/Win64/python.exe -I`)을 쓴다.
- `IKRetargeterController`로 op를 바꿔도 리타기터가 dirty로 표시되지 않아 `save_assets`가 건너뛴다 → `EditorAssetLibrary.save_loaded_asset(a, only_if_is_dirty=False)`.
- `ProgrammaticToolset` 스크립트는 툴 호출 중 에디터가 남긴 오류 로그(프리뷰 인스턴스 `Accessed None`, 없는 에셋)를 예외로 받아 중단된다. 값은 이미 들어가 있을 수 있으니 되읽어 확인한다.

## 2026-10-19 — 단위 2-1 B0 애니 변수 공급층 + B10 Pose History·DeadBlending

LootNPop(8000)과 GASP(8001, 읽기 전용) 에디터를 함께 썼다. 애니 선택(Chooser·PSD)은 바꾸지 않았다.

### 1. GASP 계산식 확인

- `SandboxCharacter_Mover_ABP`: `Update_Logic`·`Update_Trajectory`·`Update_States`·`IsMoving`·`IsStarting`·`IsPivoting`(MM 조건 합성 노드)·`ShouldTurnInPlace`·`ShouldSpinTransition`·`JustLanded_*`·`JustTeleported`·`Get_TrajectoryTurnAngle`·`Get_TotalFacingDelta`·`Update_AdditiveLean`·`Get_MMBlendTime`·`Get_MMInterruptMode`를 DSL로 읽었다. DSL이 Break 구조체 출력 핀을 숨기는 곳(`Update_EssentialValues` — 데이터 순환으로 DSL 실패, 각속도 Z, `Velocity_LastFrame_Z`, `UnrotateVector().Y`, `Delta().Yaw`)과 `UpdateStateValues` 매크로는 노드 정보로 읽었다. 임계값 기본값(`HeavyLandSpeedThreshold` −500, `TeleportThreshold` 50)과 enum 표시 이름은 CDO `list_properties`로 얻었다.
- 폰 `SandboxCharacter_Mover`: `MovementDirection`은 ABP가 아니라 폰이 입력 단계에서 계산해 InputCmd로 넘긴다(`Get_MovementDirectionAndOffset`·`Thresholds`·`FromThresholds`). 판정은 F·B·좌·우 넷만 돌려준다.
- 두 GASP BP는 DSL 읽기로 dirty가 됐다. 저장하지 않았다.
- 결과 요약 `design/GASPReference.md` §2.8.

### 2. 구현

- `ULNPAnimInstance`(헤더 UPROPERTY 추가): enum 6종, 상태별 현재·`_LastFrame`·`_Recent`·`_Time`, `Trajectory`·`TrajectoryPredictor`, 가속 4종, `Velocity_LastFrame`·`LandVelocity`, 지면 5종, `BasedMovementDelta`, `LeanAmount`, 궤적 파생 12종, 술어 8개. `IsLocomotionSource()`(메인 인스턴스)일 때만 `UpdateLocomotionValues`가 돈다. 링크 레이어·표시 메시 ABP는 계산하지 않는다.
- 각도는 Up 축 부호각(`SignedAngleAroundAxis`), 속도·가속은 접평면 투영. 보행은 질주 → Sprint, Mover `MaxSpeed` < 450 → Walk(걷기 입력이 없다).
- `LootNPop.Build.cs`에 `PoseSearch` 추가.
- `ABP_Main_UEFN`: BP 변수 4개·`Initialize_MoverPredictor`·EventGraph 노드 14개 삭제. 저장 시점(구 C++)에는 `Invalid field 'Trajectory' found in property path for Pose History` 경고가 났고, 새 C++로 재시작한 뒤 바인딩이 C++ `Trajectory`로 풀려 경고 없이 컴파일됐다. DeadBlending을 `Slot 'AdditiveHitReact'`와 `ApplyWeaponPostProcess` 레이어 사이에 넣었다. `PoseHistoryCollector`는 이미 출력 직전이라 설정만 바꿨다.
- PSS `PSS_CoreLocomotion_UEFN`은 `foot_l/r`와 궤적만 쓴다 → GASP 수집 본 8개로 충분.

### 3. 빌드

- 헤더 변경이라 게임 타깃으로 먼저 컴파일을 확인했다(통과).
- LootNPop 에디터를 끈 뒤에도 에디터 타깃 `Build.bat`이 `Unable to build while Live Coding is active`로 거부됐다. UBT는 실행 파일 경로(`UnrealEditor.exe`, 두 프로젝트 공유)로 Live Coding 뮤텍스를 찾아서, GASP 에디터의 `LiveCodingConsole`이 걸린다. `-NoHotReloadFromIDE`로 이 검사만 끄고 빌드했다(LootNPop DLL만 쓴다).

### 4. 검증

- 자동 PIE(단독, 정지): 메인 인스턴스 값 확인(지상·Idle·Strafe, 경사 −0.2°·−0.8°). 새 경고·`Accessed None` 없음. 에디터가 백그라운드면 PIE 뷰포트 렌더가 멈춰 캡처로는 화면 표시를 확인할 수 없었다.
- 사용자 리슨 2인 PIE(적도, `LNP.Anim.DebugLocomotion 2`): 정지·출발·급반전·롱소드/라이플 다방향·낮은 점프·고공 낙하·승격 적 전투. `[AnimB0]` 585줄 — 호스트(서버 Authority·게스트 시점 SimulatedProxy), 게스트(Autonomous·서버 시점), 승격 적 2마리(서버)에서 모두 값이 나왔다. 사용자 체감 회귀 없음. 한 프레임 펄스(`LandH` 등)는 눈으로 보기 어려워 로그로 확인했다.
- 발견: `bJustTeleported` 오탐 — 고속 낙하 착지 프레임(로컬), 게스트가 보는 호스트 74회(Actor 스냅). 메시 위치 + 허용치 `50 + max(|V|,|V_last|)·dt`로 바꿔 cpp만 Live Coding 반영. 재확인 PIE(질주·낙하·사망 리스폰): 착지 오탐 0, 시뮬 프록시 15회/80초(1000~2000cm/s), 리스폰·적 승격은 정상 발생.
- 2-2 참고로 남긴 성질(§2.8 끝): 승격 적 `MovementState` 깜빡임, 고속 급반전은 Spin으로 잡힘, 스폰 직후 facing 90° 1회, 좌우 연타 시 방향 깜빡임.

### 시행착오·주의

- `read_graph_dsl`은 Break 구조체 핀을 `(ToolMenus|Get)`으로 뭉개 보여 준다. 수치 판단에 쓰는 핀은 노드 정보로 다시 읽어야 한다.
- `ObjectTools.set_properties`의 인자는 `values`(JSON 문자열)다. 노드 구조체는 통째로 읽어 고친 뒤 다시 쓴다.
- C++ `NativeUpdateAnimation`은 BP EventGraph보다 먼저 돈다. BP가 만드는 값을 C++가 같은 프레임에 쓰려면 소유권을 C++로 옮겨야 한다.
- 레거시 `ABP_Lyra`(런타임 미참조)도 BP 변수 `Trajectory`를 가진다. 열면 부모 C++ 프로퍼티와 이름이 겹친다(이번 세션에서는 로드되지 않아 동작을 확인하지 못했다. 컴파일러가 BP 변수 이름을 바꾸는 경고가 날 것으로 보인다). 열었다면 저장하지 않는다.

## 2026-10-19 — 단위 2-2 B1 MM 선택 코어 + 스트레이프 복구

LootNPop(8000)과 GASP(8001, 읽기 전용) 에디터를 함께 썼다. GASP에서는 읽기 툴과 Chooser 에디터 열기(`OpenEditorForAsset`, UI 텍스트 스냅샷)만 썼다. 저장 없음.

### 1. GASP 확인

- MCP: 루트 Chooser 컬럼 4개(이동 모드·자세·이동 상태·보행, 8행). UI 스냅샷으로 결과 셀이 "Stand Idles" 같은 중첩 테이블 이름임을 보고, `CHT_PoseSearchDatabases_Relaxed.Stand Walks` 경로로 중첩 테이블 컬럼까지 MCP로 읽었다. 결과 셀은 여전히 안 읽힌다.
- PSD 10개·PSS 2개·PSN의 프로퍼티와 의존성(클립 목록)을 MCP로 읽었다. `E_MovementDirection` 표시 이름은 `.uasset` 문자열(F·B·LR·LL·RL·RR).
- 결과 셀·PSD 항목 설정은 LootNPop 쪽 C++ 명령으로 덤프했다(아래 3). 그래서 Chooser 스크린샷을 요청하지 않았다.

### 2. 반입·빌드

- LootNPop 에디터를 끈 상태에서 시퀀스 26개를 복사(해시 일치, git 미추적 확인). `Idle/M_Relaxed_Stand_Idle_Loop`은 1C-3 본과 해시가 같아 그대로 뒀다.
- 에디터 타깃 `Build.bat ... -NoHotReloadFromIDE`(GASP 에디터는 켠 채). 첫 빌드는 `FMotionMatchingBlueprintBlendSettings` 생성자 링크 오류(PoseSearch가 내보내지 않음) → `FInstancedStruct`로 리플렉션 생성. 같은 이유로 `FAssetChooser::StaticStruct()`도 못 써서 `FindObject<UScriptStruct>` + 프로퍼티 설정으로 채웠다.

### 3. GASP 덤프·복제(`LNP.Editor.GASPRef`)

- 스펙 `Saved/AnimOverhaul/GASPRef_2-2_spec.json`. GASP `Content/`를 `/GASPRef/`로 마운트하고, 불러오는 동안만 `/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/`·`/Game/Blueprints/Data/`를 `/GASPRef/`로 패키지 리다이렉트(LootNPop에는 두 경로 모두 없다).
- 덤프 `GASPRef_2-2.json`(101KB): Chooser 9개 오브젝트(루트 + 중첩 8), PSD 10개, PSS 2개. 결과 행을 문자열로 나누니 Walk 25·Run 26행(비활성 행은 `Asset=None`)으로 컬럼 행 수와 맞았다. 처음에 정규식으로 `Asset=None` 행을 빠뜨려 21행으로 셌다가 다시 셌다.
- 복제 13개: PSS 2, PSN 1(DB 10개만 남김), PSD 10. 참조 재연결 로그 정상, `/GASPRef/` 잔여 0. 저장 뒤 `get_dependencies`로 GASP 경로 0 확인. GASP PSD를 불러올 때 반입 안 된 클립 때문에 `Database None entry N has no associated animation asset` 경고가 났다(무해).

### 4. Chooser·ABP

- `CHT_PSD_UEFN`을 `CHT_PoseSearchDatabases_UEFN`으로 복제 → `SetChooserResults`로 11행 → MCP로 컨텍스트(첫 원소만 남김)·컬럼 4개. 시행착오: ① `set_properties`는 배열 크기와 기존 원소를 한 번에 바꾸면 거부한다(`elements changed alongside the size change`). ② 기존 원소의 `_structType`은 바뀌지 않는다(Bool 컬럼에 enum 바인딩이 들어감) → 빈 배열을 먼저 쓰고 새로 넣었다. ③ 복제 원본의 폴백 결과 `PSD_Idle_UEFN`이 남아 `ReplaceAssetRefs`로 새 Idle DB로 바꿨다.
- `ABP_Main_UEFN`: `OnUpdate_MotionMatching`을 `(LNP|MotionMatching|UpdateMotionMatchingNode self Node)`로(DSL에서 self 대상 C++ 함수는 `self`를 첫 인자로 준다). MM 노드 GASP 값, CDO `LocomotionDatabaseChooser`. 경고 없이 컴파일·저장.
- 자동 PIE(단독): `[AnimB1] ... DBs [PSD_Stand_Idles_UEFN]`, 새 경고 없음.

### 5. 사용자 PIE와 수정

- 1차(리슨 2인, 적도): 적도에 가까울수록 앞으로 걸어도 옆걸음, 옆으로 가도 뒷걸음 클립. 앞으로 걷는데 Idle이 유지되는 경우(대시로 풀림). 정지 → Idle은 정상.
  - 로그상 DB 선택 순서는 상태대로였다. 방향 판정 입력 `Direction`이 엔진 `UKismetAnimationLibrary::CalculateDirection`인데, 이 함수가 속도를 `GetSafeNormal2D`로 정규화한다(월드 Z 제거). 적도에서는 이동 속도가 대부분 월드 Z라 방향이 틀린다. 2-1까지는 이 값이 DB 선택에 쓰이지 않아 드러나지 않았다 → 캐릭터 Up 둘레 부호각으로 직접 계산.
  - Idle 고착: `DoNotInterrupt`에서 진행 포즈는 자기 DB 스키마로 평가된다(엔진 `PopulateContinuingPoseSearches`). `PSS_Relaxed_Idle`은 궤적 가중치 0이라 움직여도 Idle 포즈 비용이 오르지 않는다. 멈출 때는 루프 스키마가 궤적을 봐서 정상이다(비대칭). → Idle→Moving 프레임에 `InterruptOnDatabaseChange`.
  - 둘 다 cpp만이라 Live Coding으로 반영.
- 2차(1·2인, 적도): 걷기·달리기·정지·8방향·ADS 이동·총기 상체·가드 이동·재장전·근접 콤보·승격 적 전투·가드·패링 모두 자연스럽다(사용자). 승격 적 `MovementState` 깜빡임에 따른 떨림은 보고되지 않아 Chooser는 현재 값(`_Recent` 아님)을 유지했다.
- 사용자 질문: 스트레이프가 Lyra와 비슷해 보인다 → 로코모션 DB가 참조하는 시퀀스는 모두 GASP `UEFN_Mannequin/Animations`다. 무기 서브 ABP가 `spine_01` 위를 Lyra 리타깃 파지 포즈·AO로 덮어서 상체는 Lyra로 보인다.

### 시행착오·주의

- 엔진 BP 라이브러리 함수에도 월드 XY 전제가 숨어 있다(`CalculateDirection`). 구면 월드에서 각도·방향을 내는 엔진 함수는 구현을 확인하고 쓴다.
- MCP `get_dependencies`는 디스크 레지스트리를 읽어 저장 전 새 에셋에는 `None`을 돌려준다.
- 상태 표시줄 `Cmd` 입력창(SlateInspector `Type`, submit)으로 콘솔 명령을 실행하고 결과는 `Saved/Logs/LootNPop.log`에서 읽었다.

### 6. 커밋 뒤 빌드 재확인

- 커밋 `0f7e55a` 뒤 깨끗한 트리(변경 0)에서 **게임 타깃** `Build.bat LootNPop Win64 Development` 통과. 에디터 타깃은 LootNPop 에디터가 켜져 있어 Live Coding 검사에 막히므로 돌리지 않았다. 에디터 타깃은 커밋 전 같은 소스(이후 `LNPAnimInstance.cpp`의 방향·인터럽트 수정만 Live Coding으로 반영)로 경고 없이 빌드됐다. 다음에 LootNPop 에디터를 끈 김에 에디터 타깃을 한 번 더 돌린다.

## 2026-10-20 — 단위 2-3 B3 Offset Root Bone + B2 블렌드 스택 절차 보정

LootNPop(8000)과 GASP(8001, 읽기 전용) 에디터를 함께 썼다. GASP에서는 `list_graphs`·`find_nodes`·`get_node_infos`·`get_properties`만 썼다(DSL 안 씀, 저장 없음). 바인딩은 사용자가 Ctrl+C로 준 텍스트 두 개(`Saved/AnimOverhaul/gasp_2-3_blendstack.txt`·`gasp_2-3_offsetroot.txt`)로 확인했다.

### 1. GASP 확인

- 블렌드 스택 그래프 체인·노드 값, `EnableSteering`·`Get_DesiredFacing`·`Get_ProceduralTargetTime`·`Get_OffsetRoot*`·`OnUpdate_OffsetRootBone`·`Get_OrientationWarpingWarpingSpace`를 노드 정보로 읽었다. 요약은 `design/GASPReference.md` §2.10.
- Ctrl+C 텍스트에서: OffsetRoot 노드의 모드·반감기·오차·클램프는 **프로퍼티 바인딩**(함수 이름 경로)이고, CopyBone의 복사 핀은 바인딩 없이 모두 false(사실상 무동작), Steering `AnimatedTargetTime`은 핀 값 10이 실제 값(노드 구조체 2는 낡은 값)이다. GASP의 `TranslationRadius` CVar는 ini 기본 0이고 위젯이 런타임에 바꾸는지는 확인하지 않았다.

### 2. 엔진 구현 확인(구면)

- `AnimNode_OffsetRootBone.cpp`: ① `GetOnGround()`면 가상 루트 위치를 `GroundNormal`(기본 월드 (0,0,1)) 평면에 투영 ② `bClampToTranslationVelocity`면 보정 방향을 `GetSafeNormal2D()` ③ Accumulate·Interpolate는 `ShouldCounterComponentDelta`가 거짓이라 캡슐 회전 변화를 가상 루트에 더하지 않는다 — 구면을 걸어가면 캡슐 Up만 돈다. 모두 월드 Z 가정이거나 구면에서 누적되는 오차라 C++로 막았다(지면 법선 = 컴포넌트 Up, 클램프 끔, 매 프레임 Up 스윙 정렬).
- `AnimNode_Steering.cpp`: 목표 = `RootBoneRotation⁻¹ × TargetOrientation` 전체를 루트모션 회전에 스프링으로 더한다. 루트는 Offset Root가 보내는 가상 루트다. 목표 facing의 Up이 다르면 기울기까지 보정하려 들므로 C++ 목표 함수가 Up을 맞춘다.
- `AnimNode_OrientationWarping.cpp`: 방향을 `WarpingSpaceTransform`(가상 루트) 로컬로 바꾼다 → 가상 루트 Up이 중력 Up이면 안전.
- `MoverPoseSearchTrajectoryPredictor.cpp`: facing은 표시 컴포넌트 회전과 Mover 예측 회전이다(월드 Z 가정 없음).
- `AnimInstanceProxy.cpp`: `NativeThreadSafeUpdateAnimation`은 그래프 갱신 직전(워커)에 돈다. `FAnimSubsystem_Tag::FindNodeByTag`로 태그 노드를 찾을 수 있어 base 이동량·Up 정렬을 ABP 노드 함수 없이 C++에서 넣었다.

### 3. 빌드

- 헤더 변경(새 UFUNCTION·UPROPERTY, 의존성 `AnimationWarpingRuntime`·`BlendStack`). 에디터를 켠 채 게임 타깃으로 먼저 컴파일을 확인하고(성공, 468초), 사용자가 LootNPop 에디터를 끈 뒤 에디터 타깃 `-NoHotReloadFromIDE`로 빌드했다(성공, 410초, GASP 에디터는 켠 채). 이것으로 2-2에서 미룬 에디터 타깃 확인도 했다. 경고 1건은 기존 `LNPReplaceAssetRefsCommand.cpp`의 지원 중단 API다.

### 4. ABP

- MCP `create_node`는 블렌드 스택 그래프(외곽이 AnimGraph 노드)에서 `Cannot cast type 'AnimGraphNode_MotionMatching' to 'Blueprint'`로 실패한다. `connect_pins`·`break_pins`·`set_properties`는 된다. → GASP Ctrl+C 텍스트에서 함수 참조를 C++로 바꾸고(같은 핀 이름·형식), 태그 배열 비교 2노드를 `IsTurnInPlaceDatabaseActive` 하나로, 곡선 읽기 노드를 `GetOrientationWarpingAlpha`로 바꾸고, 주석·외부 링크를 뺀 텍스트(`lnp_2-3_blendstack_paste.txt`)를 사용자가 붙여넣었다. 내부 연결은 그대로 살았고, 입력·출력 연결과 입력 노드 태그 `MMBlendStackInput`은 MCP로 했다.
- AnimGraph `OffsetRootBone`은 MCP로 만들었다(`showPinForProperties` 수정으로 핀 노출 → C++ 함수 노드 4개 연결). `DeadBlending` → `OffsetRootBone` → `ApplyWeaponPostProcess`.
- 컴파일·저장. 무인 PIE 시작·정지에 새 경고 없음.

### 5. 사용자 PIE와 수정(적도, 1·2인, 승격 적)

- 캡슐이 먼저 가고 메시가 30cm 이내에서 부드럽게 따라붙는다. 총기·락온 스트레이프, ADS·가드 8방향은 "매우 자연스럽다". 조준 중 급회전에서 총구 이탈 없음. 재장전 중 복잡한 이동, 근접 콤보·락온·근접 보정 모두 회귀 없음. 2P에서 본 1P도 자연스럽다.
- 로그 `[AnimB3]` 약 3,600줄: 가상 루트 기울기 최대 0.98°(95%가 0.1° 미만). 궤적 facing 기울기는 이동 중 1° 미만, 스폰·승격 첫 로그만 82~94°(빈 궤적 이력). 30cm를 넘는 `dist`는 측정 방식 탓(직전 평가 가상 루트 vs 이번 프레임 메시, 800cm/s에서 한 프레임 13~27cm가 더해짐)이고 표시 차이는 노드가 30cm에서 자른다.
- **정지 중 룩만 돌리면 허리가 180°까지 꺾인다**(로그 Yaw 최대 179.7°). 메시 회전이 애니(+Steering)를 따르게 됐는데 제자리 회전 DB가 없어 정지 중에는 메시가 돌 수 없고, AO가 가상 루트 기준이라 허리로 다 돌린다. → 노드 `MaxRotationError` 90°(그 이상은 하체가 캡슐을 따라 미끄러지며 돈다). 사용자 확인: "아까보다 낫다". 발을 디디는 회전은 2-4 TurnInPlace DB + 두 번째 Steering.
- 움직이는 발판: 직접 `LNP.DynamicTerrain.PlaceRider 0`으로 올려 Live Coding 임시 진단(`[AnimB3T]`, 커밋 전 제거)으로 쟀다. 정지 시 프레임마다 발판 이동량(약 15cm)이 정확히 상쇄됐다(반영 뒤 지연 0.00cm). 게임 스레드 `[AnimB3]` 거리에 보였던 24cm는 측정 시점 차이였다. 사용자가 발판 위에서 움직여 봤고 문제 없었다.
- 사용자 관찰: 질주 옆·뒷걸음이 어색하다(질주 클립은 정면뿐, GASP도 질주는 F 고정). 사용자는 정면에서만 질주 최고속도를 내는 이동 변경을 원한다 → 이 단위 밖, 별도 작업 제안. `OrientationWarping` 끄고 켜기 차이는 거의 못 느꼈다(방향별 DB가 각도 차이를 이미 줄여 남는 몫이 대각 약 45°뿐).

### 시행착오·주의

- 처음 시험한 `create_node`·`delete_node`·`break/connect`로 `ABP_Main_UEFN`이 dirty가 됐다. 에디터를 끌 때 저장하지 말아 달라고 사용자에게 알렸다.
- Ctrl+C 텍스트의 `PropertyBindings`는 함수 이름 경로(`PathAsText`)로 묶인다. 같은 이름의 C++ 함수가 있으면 붙여넣기만으로 풀린다.
- `get_properties`로 노드의 `bHasCompilerMessage`·`errorType`은 읽히지 않는다. 컴파일 결과는 출력 로그로 본다.

### 6. 커밋 뒤 빌드 재확인

- 커밋 aee2997 뒤 깨끗한 트리에서 게임 타깃 `Build.bat`(LootNPop 에디터가 켜져 있어 게임 타깃) 성공, 53초. 에디터 타깃은 같은 소스(임시 진단 로그를 넣기 전)로 3절에서 성공했다. 차이는 cpp 주석 한 줄과 Live Coding으로 넣었다 뺀 임시 로그뿐이다.

## 2026-10-20 — 단위 2-4 B6 정지·회전 전환 + B7 착지 검토

LootNPop(8000)과 GASP(8001, 읽기 전용) 에디터를 함께 썼다. GASP에서는 `ProgrammaticToolset`으로 `AssetTools.get_dependencies`만 돌렸다(DSL 안 씀, 저장 없음). 2-2 덤프에 중첩 테이블 행 조건과 결과가 이미 있어 MCP 컬럼 조회는 필요 없었다.

### 1. GASP 확인

- 2-2 덤프 `GASPRef_2-2.json`의 `Stand Idles`(8)·`Stand Walks`(25)·`Stand Runs`(26)·`Stand Sprint`(6) 행을 풀었다(파서는 세션 스크래치). 필요한 DB 41개 = Stops 3, TurnInPlace 1, Idle/Walk/Run/Sprint Lands 8, Walk·Run Starts 8 + Sprint 1, Turns 3, Pivots 8, Spins 8, `Run_F_GaitTransitions` 1. 슬라이드·Traversal 행은 범위 밖.
- `MovementDirection_Recent`·`Gait_LastFrame`은 MultiEnum 비트마스크 컬럼이었다. 마스크를 enum 값으로 풀어 보니 GASP 방향 enum은 왼쪽 계열(값 2·3)·오른쪽 계열(4·5)로 묶여 있다(요약 `design/GASPReference.md` §2.11).
- 새 DB·스키마 덤프 `GASPRef_2-4.json`(53개 오브젝트 패키지). TurnInPlace DB 태그 `TurnInPlace` 확인. 착지 스키마 `PSS_Relaxed_Jump`는 3D 위치·속도 방향(XY 아님, 루트 공간)이라 구면에서도 기준이 캐릭터 Up이다.
- PSD 항목의 시퀀스는 LootNPop에 반입 전이라 덤프에 `None`으로 나왔다 → GASP MCP `get_dependencies`로 DB별 시퀀스 272개와 시퀀스 외부 의존(Foley Walk/Run·EarlyTransition·커브 압축·`AM_BakePhaseCurve…`, 모두 기존 리다이렉트·반입분)을 얻었다.
- 엔진 `PoseSearchLibrary.cpp` 확인: `DoNotInterrupt`면 진행 포즈를 **자기 DB의 스키마로** 평가해 후보에 넣는다(`SearchContinuingPose`, DB가 목록에 없어도). `InterruptOnDatabaseChange`는 현재 DB가 새 목록에 없을 때만 끊는다.

### 2. 반입과 빌드

- 272개 중 LootNPop에 없던 239개를 GASP 원본 경로 그대로 복사(LootNPop 에디터 끈 상태, GASP 에디터는 켠 채). 있던 33개(1C-3분)는 GASP 5.8과 해시 동일. 목록·해시 `Saved/AssetRefresh/2026-10-20/import_list_2-4{,_sha256}.txt`, 스크립트 `copy_2-4.py`·`gen_list.py`(같은 폴더). `git status`에 반입 파일 없음.
- C++: 편집기 명령에 복제 `relink`(기존 LootNPop 복제본으로 잇기, 새 PSD를 기존 정규화 세트에 추가)와 `SetChooserResults @목록파일`. 런타임은 `[AnimB6]` 선택 DB 로그(헤더에 private 멤버 하나). 에디터를 켠 채 게임 타깃 빌드 성공(118초), 에디터를 끈 뒤 에디터 타깃 `-NoHotReloadFromIDE` 빌드 성공(105초). 경고 1건은 기존 `LNPReplaceAssetRefsCommand.cpp`.
- 재시작한 에디터 로드 경고 없음.

### 3. 구성 에셋·Chooser

- `LNP.Editor.GASPRef` 복제 48개(스키마 7 + PSD 41), `/GASPRef/` 잔여 경고 0, 저장 뒤 디스크에 GASP 경로 문자열 0. `PSN_Relaxed_UEFN` DB 10 → 51.
- 행 정의를 `Scripts/AnimOverhaul/BuildLocomotionChooser_2-4.py`로 만들고(결과 목록 + MCP 컬럼 JSON), 컬럼은 `ProgrammaticToolset`에서 `read_file` → `set_properties`(빈 배열 먼저), 결과는 `SetChooserResults @목록`으로 넣었다. 되읽은 컬럼 14개 × 52행이 파일과 차이 0. 컴파일·저장(50개 에셋).
- 무인 PIE: 스폰 낙하 착지에서 `PSD_Stand_Idle_Lands_Heavy_UEFN` 선택, 새 경고 없음. 에디터가 백그라운드라 PIE가 느리고 입력이 없어 출발·정지는 사용자 PIE로 넘겼다.

### 4. 사용자 PIE와 수정(적도, 1·2인, 승격 적)

- 첫 PIE(로그 `Saved/AnimOverhaul/pie_2-4_a.log`, git 제외): 고른 DB — Run Starts 89·Run Stops 63·Sprint Starts 15·Sprint Stops 13·Walk Stops 10·Idle Lands 30·Run Lands 3 등 전환 DB가 실제로 뽑혔다. Pivots·Spins는 각 1회, GaitTransitions 0회.
- **출발 예외 재검토**: 임시 CVar로 끄자 사용자가 "정면 이동 때 Idle 포즈로 미끄러지며 이동"을 종종, "멈췄는데 걷는 모션 유지"를 1회 봤다. 켜면 없음 → 유지 확정, CVar 삭제.
- **제자리 회전 실패**: "멈춰서 카메라만 돌리면 한순간 이상한 포즈로 깜박이고 뚝 끊기듯 발이 돌아가 있다". 로그: 술어 `+TurnInPlace` 96회에 회전 DB 선택 4회. DB 목록이 `[TurnInPlace]`로 바뀐 프레임이 `DoNotInterrupt`라 Idle 진행 포즈(궤적 가중치 0 스키마)가 계속 이겼고, 가상 루트만 `MaxRotationError` 90°까지 벌어졌다가 Idle 재선택 때 뚝 돌았다. → `GetMMInterruptMode`에 "Idle·`bShouldTurnInPlace`·현재 DB에 `TurnInPlace` 태그 없음" 추가(Live Coding).
- 재검증(`pie_2-4_b.log`): 술어 59회·회전 DB 선택 33회(나머지는 회전 클립 재생 중에 다시 켜진 술어). 사용자: "깜박임·뚝 끊김 완전히 없어졌다, 발을 디디며 돈다, 약간 미끄러지지만 훨씬 자연스럽다". 출발·정지·근접 콤보 회귀 없음. 정지 중 가상 루트 Yaw p50 30°·p95 73°·최대 100° → `MaxRotationError` 90° 안전망 유지.
- 보행 전환: 질주 방향 게이트로 Sprint→Run 35회·Run→Sprint 36회, GaitTransitions 선택 0회 — 튀는 문제 없음(한 프레임 조건 + `DoNotInterrupt`라 진행 루프가 이긴다).
- 2인·적도·승격 적: 사용자 "어색한 부분 없음". 승격 적 3마리 DB 변경 13분간 78회(Idle 34·Walk 루프 18·Run Starts 13…), Starts·Stops 반복 깜빡임 없음. 새 경고 없음(Mover "Simulation start location disagrees"는 기존 네트워크 보정 노이즈).

### 시행착오·주의

- 덤프 파서 첫 판은 셸 경로(`/Game/...`이 Git Bash에서 `C:/Program Files/Git/Game/...`로 바뀜) 탓에 실패 → PowerShell로 돌렸다. 긴 heredoc도 Git Bash에서 따옴표 짝 오류가 나 파일로 썼다.
- 반입 전 덤프는 시퀀스를 `None`으로 보여 준다(리다이렉트하지 않는 경로라 LootNPop 쪽으로 풀린다). DB 항목 목록은 GASP `get_dependencies`가 빠르다.
- 원인 분석에 `[AnimB1]`(검색 DB 목록)과 `[AnimB6]`(선택 DB)을 함께 보는 것이 결정적이었다. 목록에 전환 DB가 있어도 선택되지 않으면 진행 포즈가 이기는 것이다.

### 5. 커밋 뒤 빌드 재확인

- 커밋 c7c0571 뒤 깨끗한 트리에서 게임 타깃 `Build.bat`(LootNPop 에디터가 켜져 있어 게임 타깃) 성공, 65초, 경고 0. 에디터 타깃은 2절에서 성공했다. 그 뒤 바뀐 것은 cpp 본문(제자리 회전 인터럽트, CVar 삭제)과 헤더 주석뿐이고 Live Coding으로도 컴파일됐다.

## 2026-10-20 — 단위 2-5 B4 발 배치 + B5 경사

### 1. GASP 읽기

- 두 에디터(8000·8001) MCP 확인. MCP `call_tool`은 `toolset_name`과 짧은 `tool_name`을 따로 넣어야 한다(풀네임은 not found).
- `CR_Biped_FootPlacement`: `ControlRigTools`(`list_nodes`·`list_pins`·`get_connected_pins`·`get_pin_value`)를 묶음 스크립트로 돌려 그래프 42개를 덤프(약 125KB, 스크래치). 구조체 핀은 최상위가 연결돼 있지 않을 때만 서브 핀을 따로 물어야 연결이 보인다 — 양쪽이 서브 핀인 연결(`Subtract.Result.X → ClampLength.Value.X`)은 최상위 덤프에 안 나온다. 서브 핀 스캔으로 월드 축 성분을 직접 쓰는 곳 세 군데를 찾았다(`design/GASPReference.md` §2.12).
- `Procedural_Feet`: 0-2 Ctrl+C 텍스트(`Saved/GASPRef/feet.txt`)를 스크립트로 요약. `AllowFootPinning`·`AllowSlopeWarping`·`JustTeleported`·`Biped_FootPlacement_OnBecomeRelevant`는 `find_nodes`·`get_node_infos`(DSL 안 씀). `AllowFootPinning`의 `IsMoving` AND는 결과에 안 이어져 있다.
- 경사 BS를 참조하는 것은 SM 경로 Chooser뿐(GASP `get_referencers`) → 마스터 세션에 범위 질문, 사용자 결정 A(AN-021): 반입·Chooser 행 없이 CR slope warping만.

### 2. CR 구성 에셋

- `LNP.Editor.GASPRef` 복제(스펙 `Saved/AnimOverhaul/GASPRef_2-5_clone_spec.json`) → `Content/Characters/ControlRigs/CR_Biped_FootPlacement_UEFN`. 복제는 됐지만 함수 참조 44개가 GASP 원래 경로(`/Game/Blueprints/ControlRigs/CR_Biped_FootPlacement`) 라이브러리를 가리켜 컴파일이 깨졌다(함수 식별자는 소프트 경로라 하드 참조 재연결에 안 걸린다).
- 에디터 Python(상태 표시줄 Cmd 입력창, SlateInspector `Type`)으로 `Scripts/AnimOverhaul/FixFootPlacementCR_2-5.py`: `RigVMController.find_graph_function_identifier(<CR>_C, 이름)` → `swap_function_reference`로 재연결, 구면 수정, 컴파일·저장. `RigVMGraphFunctionIdentifier.library_node_path`는 읽기 전용이라 직접 만들 수 없다.

### 3. C++·ABP

- C++: CR 알파 `ShouldUseFootPlacementRig`(지상 & CVar), 착지 프레임 `ShouldResetFootPlacement`, `IsFootPlacementDebugEnabled`. 게임 타깃 빌드(에디터 켠 채) → 에디터 종료 → 에디터 타깃 `-NoHotReloadFromIDE` 빌드.
- ABP: GASP CR 노드 Ctrl+C 텍스트를 바꿔(클래스 → 복제본, 알파 Bool·블렌드 0.3/0.2, `UseIKBoneTargets` 핀 기본 false, 바인딩 제거) 사용자가 붙여넣고, K2 값·함수 노드 9개 생성과 연결은 MCP. 핀 순서는 CR 변수 순서다(`get_node_infos`의 `name`으로 확인). `OffsetRootBone` → CR → 무기 후처리. 경고 없이 컴파일.

### 4. PIE에서 고친 것

1. **이동 패널 위 CR 매 프레임 리셋**(서버 플레이어 `speed 0 +Teleported` 연속): Mover 속도는 base 기준이라 0인데 메시는 패널과 함께 프레임당 50cm 넘게 움직였다(2인 PIE 12fps). 텔레포트 판정에서 base 몫(`PendingBasedMovementDelta`)을 뺀다.
2. **아주 느리게 걷다 서면 걷기 유지**(2-4부터 있던 것): 정지 프레임에 DB가 `Idles`/`Walk_Stops`로 바뀌어도 `DoNotInterrupt`라 진행 중 출발·루프 포즈가 계속 이겼다(`[AnimB6]` 선택 변화 없음). 정지 프레임도 인터럽트.
3. **패널 위 다리 끊김**: 임시 진단(`[AnimB4diag]`, CR 인스턴스 변수를 리플렉션으로 읽음)에서 핀이 애니 발끝에서 20~30cm 벌어지고 골반이 수평으로 ±15~24cm 움직였다. CVar A/B로 핀 고정이 원인임을 확인 → 움직이는 base 위에서 핀 고정 끔.
4. **패널 위 전신 움찔**: 루트(감쇠 e)가 패널 상하 속도가 바뀌는 프레임에만 그 변화량(최대 12cm)만큼 튀었다. CR 평가 시점의 컴포넌트 이동에는 이번 프레임 base 이동이 있고 공급 base 이동량은 한 프레임 전 값이다(정속에서는 상쇄). 감쇠 식을 "자기 이동량의 법선 성분"으로 바꿔도 남아 → 움직이는 base 위에서 루트 감쇠도 끔(`IsOnStaticGround`로 합침). 패널 위 루트 표준편차 1.3 → 0.02cm.
5. **적도에서 제자리 빠른 회전 시 공중 튐**(사용자: 반시계 → 아래로 튀었다 솟음, 시계 → 위로 튀었다 떨어짐, 플레이어 스타트 근처는 안 남): 진단에서 루트·바닥 기울기는 정상인데 골반·두 발만 한 프레임에 −2200~+1500cm로 갔다가 골반 평활(0.2s)로 돌아왔다. CR `AdjustPositionToPlane`이 바닥 법선을 `ik_foot_root`(루트 자식) 기준으로 바꿔 아래(−Z)로 교차하는데, 엔진 `OffsetRootBone`의 `MaxRotationError` 클램프가 로컬 오차를 월드 쪽에 곱해 적도에서 가상 루트를 최대 90° 기울였다(엔진 구면 함정 4번째). 노드 값 −1, C++가 Up 정렬 직후 컴포넌트 Up 둘레로 90° 제한.

### 5. 검증

- 사용자 1·2인 PIE(적도): 평지·여러 각도 경사, 제자리 회전 발 미끄러짐 크게 줄어듦, 점프·낙하 착지, 패널 위 정지·이동·빠른 회전, 정지 전환, 근접 콤보·대시·재장전·가드·패링·랙돌 회귀 없음. 적 스폰 0(`LNP.Spawn.EnemyDensity 0`, 에디터 콘솔)으로 측정한 뒤 1로 되돌렸다.
- 마지막 수정 뒤 사용자 리슨 2인 PIE(적도, 승격 적과 경사로 왕복, 서로 화면 관찰): 어색함 없음. 텔레포트 판정 80회 중 대부분은 적 승격·강등 스냅(적마다 서버·게스트 2회씩).
- 임시 진단 코드는 커밋 전에 지웠다.
- 커밋 0265f91 뒤 깨끗한 트리에서 게임 타깃 `Build.bat`(LootNPop 에디터가 켜져 있어 게임 타깃) 성공, 94초, 경고 0. 에디터 타깃은 진단 코드를 지우기 전 같은 소스로 성공했다(47초). 그 뒤 바뀐 것은 cpp의 임시 진단 블록 삭제뿐이다.

## 2026-10-21 — 단위 2-6 B8 Additive Leans + ActorPromoted 적 전체 회귀 (Phase 2 종료)

### 1. GASP 확인

- 두 에디터(8000·8001) MCP 확인. GASP 쪽은 `AssetTools.get_dependencies`만 썼다(DSL 안 씀, 저장 없음).
- `Saved/GASPRef/leans.txt`를 스크립트로 요약: 노드 21개(캐시 3, BS 플레이어 3, Mesh Space Additive 2, Enum 블렌드 2, Bool 1, TwoWayBlend 2). 바인딩은 `MovementMode`·`MovementState`·`LeanAmount.X/Y`·`Speed2D`(승격 바인딩)·`EnableAO`, 곡선은 `Disable_AdditiveLeans`. BS 축 보간 시간은 0이다(LootNPop 사본 MCP 읽기).
- BS 의존 = 시퀀스 13개 + `SK_UEFN_Mannequin`. 16개 모두 LootNPop에 이미 있었다. 시퀀스 13개는 1C-3 `import_list_1C3.txt`에 들어 있어 해시가 같았다. BS 3개는 `identical_except_path_49.txt`(1C 분류 HEADER_ONLY — 1C-2 경로 이동 때 다시 저장돼 헤더만 다름)라 1C-3이 덮어쓰지 않았다.

### 2. 위치 결정

- 무기 서브 ABP 4종의 `ApplyWeaponStyleOverlay`를 MCP로 읽었다: `LayeredBoneBlend`(`spine_02` 깊이 2, 가중치 1, `bMeshSpaceRotationBlend` 참). 맨손은 통과다. 재장전 레이어드 블렌드(메인 ABP)도 `spine_01` 메시 공간이다.
- 오버레이 앞에 두면 메시 공간 회전 블렌드가 `spine_03` 위를 무기 자세의 컴포넌트 공간 회전으로 덮어, 무기를 든 상태에서 상체 기울기가 지워진다 → 오버레이 뒤, 슬롯 앞으로 정했다. GASP처럼 몽타주 슬롯이 기울기를 덮는다.
- 원거리 발사는 총구 소켓 위치에서 조준점으로 수렴하므로(`TechDesign_HitDetection.md` §7.7) 기울기가 AO 뒤에 얹혀도 명중은 그대로다.

### 3. 반입·C++·빌드

- 사용자가 LootNPop 에디터를 끈 뒤 BS 3개를 GASP 원본으로 덮어썼다(이전 사본 `Saved/AssetRefresh/2026-10-21/backup_pre2-6/`). 16개 해시 일치 `Saved/AssetRefresh/2026-10-21/import_list_2-6_sha256.txt`. git 제외 확인(`.gitignore` `Content/Characters/UEFN_Mannequin/`).
- C++: `ShouldApplyAdditiveLeans`·`ShouldApplyHeadLean`(UFUNCTION, 스레드 세이프), `bIsLockOn`(InputCmd `FLNPModifierInputs`, 원격·시뮬 프록시 포함), CVar `LNP.Anim.AdditiveLeans`, 디버그 표시 `lean x,y on/off +head`·`LockOn` 플래그. 에디터 타깃 `-NoHotReloadFromIDE` 빌드 68초, 경고 0(GASP 에디터는 켜 둔 채).
- 그 뒤 `[AnimB3]` 1초 로그에 Lean 값·적용 여부를 더했다(함수 본문만, Live Coding).

### 4. ABP

- `Scripts/AnimOverhaul/BuildLeansPaste_2-6.py`가 `leans.txt`를 바꿔 `Saved/AnimOverhaul/lnp_2-6_leans_paste.txt`를 만든다. 레이어 입·출력과 이동 모드 Enum을 빼고, 이동 상태 Enum을 머리 Bool의 사본(새 핀 GUID)으로 바꿔 `ShouldApplyAdditiveLeans`에 바인딩하고, 머리 Bool은 `ShouldApplyHeadLean`에 바인딩하면서 참·거짓 캐시 참조를 맞바꿨다. `Speed2D`(double, 승격) → `GroundSpeed`(float). 노드 이름에 `_Lean`을 붙여 ABP의 기존 `SaveCachedPose_0`(`PreUpperBody`)과 섞이지 않게 했다. 생성 뒤 링크 양방향 일관성 검사(불일치 0).
- 클립보드에 넣어 사용자가 붙여넣고, MCP로 `ApplyWeaponStyleOverlay` → `PreLeanPose` 캐시, 곡선 TwoWayBlend → `Slot 'FullBodyAdditivePreAim'`을 이었다. 캐시 참조는 `get_node_infos`의 노드 제목으로 확인했다(`UseCachedPose.SaveCachedPoseNode`·`AnimGraphNodeBinding_Base.PropertyBindings`는 MCP `get_properties`로 읽히지 않는다).
- 경고를 오류로 다루는 컴파일 통과(바인딩 경로가 틀리면 2-1처럼 `Invalid field` 경고가 난다) → 저장.

### 5. 검증

- 자체 PIE(MCP `StartPIE`, 상태 표시줄 Cmd에 `LNP.Anim.DebugLocomotion 2`): 권총(FreeAim) 정지 상태 `lean 0,0 off`(머리 기울기 꺼짐), 새 경고 없음. 플레이어가 가만히 있으면 적이 다가오지 않아 승격 적 로그는 두 번 시도해도 못 모았다.
- 사용자 1인 PIE(맨손·롱소드·총기, 락온 스트레이프, ADS·이동 재장전, 콤보·대시·가드, 점프·활공·그래플, 랙돌, 적도)와 리슨 2인 PIE(게스트 화면에서 호스트·승격 적 이동·선회·출발·정지·제자리 회전·발 배치): "전체적으로 어색한 부분 없음". 그 구간 로그에는 디버그 CVar 명령이 없어(게임패드 `DebugManager` 명령뿐) Lean 수치 대조는 없다.
- 사용자 PIE 구간 경고: Mover `UpDirection` 0 벡터(적 3기, 44회), 초기화 전 궤적 예측(스폰 순간), NetworkPrediction 롤백 5회 — 모두 10-20 로그에도 있던 기존 노이즈다.

### 6. Phase 2 종료

- 체크리스트 2-0a~2-6 완료. 최종 포즈 파이프라인은 `phases/Phase02_GASPMoverABP.md` "Phase 2 종료", 공통 설계는 `TechDesign_CombatAnimation.md` §1·§7.4에 반영했다.
- ActorPromoted 적 회귀: 단위마다 2인 PIE로 확인했고(2-1 값 로그, 2-2·2-4 DB 변경 횟수, 2-5 텔레포트 판정·경사로 왕복), 2-6에서 최종 상태를 게스트 화면으로 다시 봤다.
