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
