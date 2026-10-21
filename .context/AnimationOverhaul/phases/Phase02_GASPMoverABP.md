# Phase 2 — 스켈레톤 전환과 GASP Mover ABP 재구성

> 상태: 완료(2026-10-21. 2-0a 2026-10-18, 2-0b·2-1·2-2 2026-10-19, 2-3·2-4·2-5 2026-10-20, 2-6 2026-10-21)
> 선행: Phase 1C 완료(GASP 5.8 소스, 93본 `SK_UEFN_Mannequin`), AN-011(UEFN 전환)·AN-015(MM 노드 방식)·AN-016(폴더·git)·AN-018(소스 저장 금지)
> 순서: 2-0a → 2-0b → 2-1 → … → 2-6. 단위 하나가 세션 하나이고, 단위마다 커밋하고 사용자 확인을 받는다.
> 단위 목록·블록 정의는 `Roadmap.md` §4 Phase 2와 `design/GASPReference.md` §2.2·§2.7이 소유한다. 이 문서는 착수한 단위의 실행 계획·결과·재생성 절차를 소유한다.

## 단위 2-0a — Lyra 세트 UEFN 리타깃

목표: 2-0b 전환에 필요한 Lyra 애니(무기 레이어·몽타주)의 UEFN 사본과 UEFN 스켈레톤용 무기 서브 ABP 5개를 미리 만든다. **런타임 참조는 바꾸지 않는다**(`ABP_Lyra`·`CHT_Montage`·GA·`AnimSourceMesh`·`ABP_Retarget`은 2-0b). 에디터는 LootNPop(8000)만 쓴다.

계획:

1. 실사용 목록을 0-3 방법(git 추적 `.uasset`을 루트로 라이선스 폴더 안에서만 `get_dependencies`를 따라감)으로 다시 뽑아 파일 단위로 확정한다.
2. 리타기터를 고르고 93본 스켈레톤에서 체인 매핑이 유효한지 확인한다.
3. 소수 클립으로 배치 리타깃이 되는지 시험한 뒤 전체를 `Content/Characters/Retargeted/Lyra/`(git 제외)에 만든다.
4. 몽타주 사본을 원본과 대조한다(ANS 5종·섹션·루트모션·노티파이·슬롯). 가상 본 `VB IK_Hand_*` 사용 여부를 확인한다.
5. 무기 서브 ABP 5개를 UEFN 스켈레톤용 새 구성 에셋으로 만든다(레이어 인터페이스·AO 경로는 원본과 같게).
6. 재생성 절차를 남긴다(README §4 ③).

### 2-0a 결과 (2026-10-18, 상세 `history/Phase02_Log.md`)

| 항목 | 결과 |
|:---|:---|
| 실사용 목록 | `Lyra_Mannequin/Animations` 121개 사용(0-3과 같음). PSD 경로 30을 빼면 **91개**(서브 ABP 경로 37 + 몽타주 경로 55, 겹침 `MM_Rifle_Idle_ADS` 1). 여기에 **0-3이 놓친 7개를 더해 98개**를 리타깃했다: `Weapons/{Pistol,Rifle,Shotgun}/Animations/AM_MM_*_{Fire,Reload}` 6개(`CHT_Montage`가 쓰는 캐릭터 몽타주, 스켈레톤 `SK_Mannequin`. 0-3은 Lyra 폴더만 셌다), `MM_Unarmed_Jump_Fall_Loop` 1개(`ABP_Lyra` 직접 참조, AN-007 공중 루프. 0-3에서는 `PSD_Jump_Lyra`도 참조해 PSD 경로로 셌다). 분류 MM 60·AM 27·A 9·AO 2. 목록 원본 `Scripts/AnimRetarget/LyraToUEFN_RetargetList.txt` |
| 리타기터 | 런타임 `RTG_Lyra_to_GASP`를 복제한 **`RTG_Lyra_to_UEFN_Offline`**(추적)을 쓴다. 체인 30개는 모두 매핑돼 있고 본은 93본에 다 있다. 하지만 런타임 리타기터 그대로는 결함 2건이 있었다. ① Root Motion op의 `SourceRoot`·`TargetRoot`·`TargetPelvis`가 비어 op가 아무 일도 하지 않는다 → 루트 이동량이 골반에 남고 `root`가 원점에 고정(대시 875cm, `A_SW_Damage_Backward` −109cm가 0). ② 대상 `LeftToe`·`RightToe` 체인이 원본 다리 체인에 매핑돼 `ball_*`가 최대 107° 흔들린다(원본은 정지). 복제본에 루트 본 지정, 발가락 매핑 해제, 스택 맨 앞 **Copy Base Pose op**를 넣었다. 결과: 루트 이동 유지(대시 875→850cm, 피격 −109→−106cm, 두 메시 골반 높이 비 0.97), 발가락·트위스트 본 로컬 회전이 원본과 0° 차이. `_noRootMotion`은 ISKM 제자리 클립 전용이라 쓰지 않았다 |
| 배치 실행 | MCP 툴셋에는 IK 리타깃 배치 도구가 없다. **에디터 Python을 MCP로 실행**했다 — `SlateInspectorToolset`으로 하단 상태 표시줄 Cmd 입력창에 `py "<스크립트>"`를 입력. `IKRetargetBatchOperation.run_batch_retarget` 1회로 98개 생성 |
| 몽타주 대조 | 98쌍(몽타주 27·시퀀스 69·AO 2) 결함 차이 0. ANS 5종(`AM_SW_Attack_01`에 각 2개)·노티파이 트랙·시간·길이, `PlaySound`(재장전 3개 각 3개), 섹션 이름(`Section_1`·`NoUse`·`Section_2`), 슬롯·세그먼트, 블렌드, `bEnableRootMotion`·루트 잠금, 가산 타입·기준 포즈, 커브 3개, AO 샘플·축이 같다. 섹션 시작 시간·다음 섹션 연결은 Python에서 읽을 수 없어 이름만 비교했다(배치가 몽타주 객체를 그대로 복제한다). 남은 차이는 **몽타주 그룹 14건**뿐이고 사본이 아니라 스켈레톤 슬롯 그룹 문제다(아래 2-0b 결정 B) |
| 가상 본 | `VB IK_Hand_*`는 `ABP_Lyra`·서브 ABP 5개 어디에도 없다. 왼손 IK는 `ABP_Sub_LongSword`의 TwoBoneIK(`hand_l`)뿐이다 → UEFN 전환에 영향 없음 |
| 서브 ABP 5개 | `/Game/Characters/ABP_Sub_{LongSword,Pistol,Rifle,Shotgun,Unarmed}_UEFN`(추적). 배치에 원본 ABP를 함께 넣어 복제·리타깃한 뒤 옮겼다. 부모 `ULNPAnimInstance`, 인터페이스 `ALI_WeaponStyles`(스켈레톤 없음, 그대로 공유), 대상 스켈레톤 `SK_UEFN_Mannequin`, 참조 애니는 사본만(AO 경로 원본과 같음 — `Shotgun`은 원래대로 Rifle AO). 경고를 오류로 취급한 컴파일 5/5 통과 |
| 소스 보호 | `UEFN_Mannequin/`·Lyra 원본·런타임 리타기터 저장 없음(dirty 0, `git status`에 소스 없음). 아래 엔진 문제 2건 때문에 원본이 메모리에서 바뀌는 것은 스크립트가 디스크에서 다시 읽어 되돌린다 |

**엔진 문제(UE 5.8) — 재생성 스크립트가 보정한다.**

- `FAnimTrack::ReplaceReferredAnimations`(`AnimCompositeBase.cpp:474`)가 몽타주 세그먼트의 **원본** 시퀀스에도 참조 교체를 호출한다. 그 결과 메모리상의 Lyra 원본 `RefPoseSeq`가 사본을 가리킨다(dirty 표시 없음). 그 세션에서 원본을 저장하거나 PIE를 돌리면 오염된다. 스크립트가 끝에서 원본 패키지를 `reload_packages`로 되돌리고 확인한다.
- `retain_additive_flags`는 사본의 가산 설정을 참조 교체 **전** 값으로 되돌린다. 다른 시퀀스를 기준 포즈로 쓰는 `MM_HitReact_Back_Lgt_01`(기준 `MM_Rifle_Idle_ADS`) 사본이 Lyra 원본을 가리켰다. 스크립트가 같은 이름의 사본으로 다시 잇는다. `RefPoseSeq`가 있는 나머지 40개(자기 자신이 기준이거나 AO 샘플이 `*_AO_CC`를 기준으로 쓰는 경우)는 배치 결과가 이미 사본을 가리켰다(대조로 확인, 왜 이 1건만 다른지는 확인하지 못했다).
- `run_batch_retarget` 반환 목록에는 ABP가 빠진다(엔진 코드상 애니만 넣는다). 대상 폴더에는 생긴다.
- `overwrite_existing_files`는 애니만 덮어쓰고 ABP는 이름 뒤에 `1`을 붙여 새로 만든다. ABP를 다시 만들 때는 기존 것을 먼저 지운다.

### 재생성 절차 (새로 클론한 환경 또는 스켈레톤·Lyra 원본 교체 뒤, README §4 ③)

전제: `Content/Characters/Lyra_Mannequin/`·`Content/Weapons/{Pistol,Rifle,Shotgun}/`(Lyra 라이선스 원본, git 제외)와 GASP 5.8 `UEFN_Mannequin/`(README §4 ②)이 있다. 추적 에셋 `RTG_Lyra_to_UEFN_Offline`·`ABP_Sub_*_UEFN`은 저장소에 있다.

1. LootNPop 에디터를 켜고 Output Log 하단 Cmd 입력창(Python이 아니라 `Cmd` 모드)에서 실행한다.
   ```
   py "D:/UnrealProjects/LootNPop/Scripts/AnimRetarget/RetargetLyraToUEFN.py"
   ```
   MCP로 할 때는 `SlateInspectorToolset.Snapshot`에서 상태 표시줄 `Cmd` 콤보 옆 `textbox`를 찾아 `Click` → `Type(submit=true)`한다(Output Log 탭 안의 입력창은 탭이 바뀌면 ref가 사라진다).
2. `Saved/AnimRetarget/LyraToUEFN_RetargetList_report.json`을 본다: `created_count` 98, `missing`·`ref_pose_unresolved`·`source_refs_into_target_after_reload`·`newly_dirty_outside_target`가 모두 비어 있어야 한다. `ref_pose_fixed`는 `MM_HitReact_Back_Lgt_01` 1건이 정상이다.
3. `/Game/Characters/Retargeted/Lyra/`만 저장한다(MCP `AssetTools.save_assets`에 폴더 안 경로를 넘긴다. 전체 저장 금지 — 소스가 섞이면 안 된다, AN-018).
4. 대조: `py ".../Scripts/AnimRetarget/CompareLyraRetarget.py"` → `Saved/AnimRetarget/LyraToUEFN_RetargetList_compare.json`의 `compared` 98, `mismatch`·`missing_copy` 비어 있음. `slot_group_diff` 14건은 2-0b 결정 B가 풀릴 때까지 정상이다.
5. 추적 서브 ABP 5개는 같은 경로의 사본을 참조하므로 다시 만들 필요가 없다. 에디터를 다시 켠 뒤 컴파일 경고가 없는지만 본다.

리타기터를 처음부터 다시 만들 때(런타임 리타기터가 바뀐 경우)는 `Scripts/AnimRetarget/SetupOfflineRetargeter.py`를 먼저 실행하고 `RTG_Lyra_to_UEFN_Offline`만 저장한다. 서브 ABP를 다시 만들 때는 기존 `ABP_Sub_*_UEFN`을 지우고, 목록에 원본 `/Game/Characters/ABP_Sub_*` 5줄을 임시로 더해 1을 실행한 뒤 `Retargeted/Lyra/`에 생긴 ABP를 `/Game/Characters/`로 옮기고(`AssetTools.move`) 컴파일한다.

## 단위 2-0b — 전환 (결정 A·B 확정 2026-10-18, 결과는 아래 "2-0b 결과")

`Roadmap.md` §4 2-0b 행이 범위를 소유한다. 2-0a에서 확인한 것 중 2-0b가 처리해야 할 것:

### 결정 A — 임시 로코모션 Chooser (확정: A1, 2026-10-18 사용자 결정)

Roadmap 초안은 임시 로코모션으로 `CHT_PSD_UEFN`·`PSD_*_UEFN`을 쓴다고 했다. 그런데 `CHT_PSD_UEFN`은 1C-2에서 사용자 결정으로 삭제됐고(옛 실험 `ABP_UEFN`과 함께), 남은 것은 `Characters/MotionMatchingData/PSD_{Idle,Jump,Run,Walk}_UEFN`·`PSN_`·`PSS_CoreLocomotion_UEFN`뿐이다.

| 안 | 내용 | 평가 |
|:---|:---|:---|
| **A1 새로 만든다(채택)** | `Lyra_Mannequin/MotionMatchingData/CHT_PSD_Lyra`(지금 `ABP_Lyra`가 쓰는 Chooser, 컨텍스트가 LootNPop C++)를 `Characters/MotionMatchingData/CHT_PSD_UEFN`으로 복제하고 PSD 행 4개를 `_UEFN`으로 바꾼다(Jog → Run). 추적 구성 에셋 | 행 조건·컨텍스트가 지금 `ABP_Lyra`와 같아 전환 회귀 원인이 하나 줄어든다. 2-2에서 상태별 PSD 묶음 Chooser로 교체될 임시물이라 이 정도면 충분하다 |
| A2 백업에서 되살린다 | `Saved/AssetRefresh/2026-10-18/backup/GASP_Mannequin/MotionMatchingData/CHT_PSD_UEFN.uasset` | 백업의 내부 참조가 모두 끊겨 있다. 컨텍스트가 삭제된 `ABP_UEFN`이고 PSD 4개가 1C-2 이전 경로(`GASP_Mannequin/MotionMatchingData/`, 리다이렉터 정리됨)를 가리킨다. 되살려도 컨텍스트와 행 4개를 다시 이어야 해서 A1보다 손이 더 간다 |

### 결정 B — 몽타주 슬롯 그룹 (확정: B1, 2026-10-18 사용자 결정, AN-020)

몽타주 그룹은 스켈레톤에 등록된 슬롯 그룹에서 정해진다. 두 스켈레톤이 다르다.

| 스켈레톤 | 슬롯 그룹(몽타주 그룹으로 확인한 소속) |
|:---|:---|
| Lyra `SK_Mannequin` | `DefaultGroup` ← `DefaultSlot`·`FullBody`·`FullBodyAdditivePreAim`·`UpperBody`·`UpperBodyAdditive`, `AdditiveGroup` ← `AdditiveHitReact`. 이름 테이블에 `UpperBodyDynAdditive*`도 있다 |
| GASP `SK_UEFN_Mannequin` | `DefaultGroup` ← `DefaultSlot`, `Partials` ← `UpperBody`뿐. 등록되지 않은 슬롯은 `DefaultGroup`으로 본다 |

2-0b에서 `AnimSourceMesh`가 UEFN이 되면 그룹이 바뀐다. ① 피격 몽타주 11개(`AdditiveHitReact`)가 `AdditiveGroup` → `DefaultGroup`이 되어 공격·대시 몽타주와 같은 그룹이 된다. 같은 그룹의 몽타주는 새 몽타주가 재생될 때 자동으로 멈추므로, 피격 리액트가 진행 중인 공격 몽타주를 끊게 된다. ② 재장전 3개는 첫 트랙 `UpperBody`가 `Partials`라 그룹이 `Partials`가 되고, `UpperBodyAdditive` 트랙은 `DefaultGroup`이라 로드 때 "All slots must belong to the same group" 경고가 난다. 재장전과 대시·공격이 서로 자동으로 끊지 않게 된다(C++가 명시적으로 멈추는 경로가 있는지는 2-0b에서 확인한다). `ABP_Lyra`의 슬롯 노드는 `DefaultSlot`·`FullBody`·`UpperBody`·`AdditiveHitReact`·`FullBodyAdditivePreAim`이다.

| 안 | 내용 | 평가 |
|:---|:---|:---|
| **B1 런타임 등록(채택)** | C++(`ULNPAnimInstance` 초기화 등)에서 Lyra와 같은 슬롯 → 그룹 표를 `USkeleton`에 등록한다. 에셋은 저장하지 않는다 | AN-018과 맞고 GASP 덮어쓰기에도 살아남는다. 에디터에서 스켈레톤이 dirty로 표시되지 않는지 확인이 필요하다 |
| B2 소스 스켈레톤에 등록·저장 | `SK_UEFN_Mannequin` 슬롯 그룹 편집 | AN-018 위반. GASP 갱신 때 사라진다 |
| B3 몽타주 슬롯 이름 변경 | 사본 몽타주를 UEFN에 있는 슬롯으로 옮긴다 | 그룹 의미(가산 피격이 공격을 안 끊음)를 재현할 수 없다 |

### 그 밖의 인계

- `ABP_Lyra` 공중 루프(AN-007)는 `Retargeted/Lyra/MM_Unarmed_Jump_Fall_Loop_UEFN`으로 바꿀 수 있다(GASP 낙하 루프로 바꾸는 것은 AN-007 재논의).
- `CHT_Montage`·`GA_Stagger_LongSword`·`GA_ParrySuccess_LongSword`가 가리킬 사본은 `Retargeted/Lyra/AM_*_UEFN`이다. 무기 폴더 몽타주 6개도 `Retargeted/Lyra/AM_MM_*_UEFN`에 있다.
- 무기 레이어 참조를 `ABP_Sub_*_UEFN`으로 바꾼다. 지금 원본을 참조하는 곳: `ItemData/WeaponVisualSets/VS_{LongSword,Pistol,Rifle,Shotgun}`, `ABP_Sub_Unarmed`는 `BP_LNPPlayer`·`ABP_Lyra`.
- 런타임 `RTG_Lyra_to_GASP`의 결함 2건(루트 본 미지정, 발가락 매핑)은 지금 `ActorPromoted` 적 `VisualMesh`에 그대로 있다. 2-0b에서 적 `VisualMesh`가 포즈 복사로 바뀌면 이 리타기터는 런타임에서 빠진다. `RTG_GASP_to_MS`(플레이어 UEFN → MoverSample)도 Root Motion op 본 이름이 비어 있다. 2-0b에서 같은 확인을 한다.
- 이 단위의 엔진 문제 때문에, 같은 에디터 세션에서 배치를 돌린 뒤에는 원본을 저장하지 말고 재로드 결과(`source_refs_into_target_after_reload` 빈 목록)를 확인한다.
- 필요한 에디터: LootNPop(8000)만. GASP 구성을 읽지 않는다.

### 2-0b 결과 (2026-10-18~19, 상세 `history/Phase02_Log.md`)

| 항목 | 결과 |
|:---|:---|
| 메인 ABP | **`ABP_Lyra`를 리타깃 복제**해 `/Game/Characters/ABP_Main_UEFN`(추적)을 만들었다. 새로 만들지 않은 근거: 2-0b는 "지금과 동등한 동작"이 목표라 그래프·변수·함수(`OnUpdate_MotionMatching`·`Initialize_MoverPredictor`)·슬롯 순서를 그대로 옮기는 편이 회귀 원인을 줄인다. 2-0a 서브 ABP 5개도 같은 방식으로 문제가 없었다. 이름에서 Lyra를 뺀 것은 2-1부터 GASP식으로 재구성될 본체이기 때문이다. `IKRetargetBatchOperation`에 ABP 하나만 넣고(`include_referenced_assets=False`) 남은 참조 3개(공중 루프 `MM_Unarmed_Jump_Fall_Loop_UEFN`, 링크 레이어 기본값 `ABP_Sub_Unarmed_UEFN` 2곳, Chooser `CHT_PSD_UEFN`)는 노드 프로퍼티로 바꿨다. 경고를 오류로 취급한 컴파일 통과. `ABP_Lyra`·`ABP_Sub_*`(Lyra)는 되돌림용으로 남겼다(런타임 미참조) |
| 결정 A | `CHT_PSD_UEFN`: `CHT_PSD_Lyra` 복제 → PSD 4개를 `_UEFN`으로(Jog → Run). Chooser 결과 행은 MCP·Python으로 쓸 수 없어(`ResultsStructs`가 편집 플래그 없는 `UPROPERTY()`) **에디터 명령 `LNP.Editor.ReplaceAssetRefs`**(`LootNPopEditor`, 새 cpp)를 만들었다. 패키지 안 모든 오브젝트에 `FArchiveReplaceObjectRef`를 돌린다(UPackage를 검색 루트로 주면 아무것도 순회하지 않는다). 스크립트 `Scripts/AnimRetarget/SwitchChoosersToUEFN.py` |
| 결정 B | `ULNPAnimInstance::NativeInitializeAnimation`이 Lyra 슬롯 → 그룹 표를 `USkeleton`에 등록(AN-020). 확인: 피격 `AdditiveGroup`, 재장전 두 슬롯 모두 `DefaultGroup`, `SK_UEFN_Mannequin` dirty 아님. 이게 없으면 재장전 몽타주는 경고에 그치지 않고 `HasValidSlotSetup()`에 걸려 재생되지 않는다. 재장전을 C++가 끊는 경로: 대시(`OnDashExecuted` → 어빌리티 취소)·무기 교체(`CancelAbilities`) → `PlayMontageAndWait`가 몽타주를 멈춘다 |
| 몽타주·GA·레이어 | `CHT_Montage` 26개 → 사본. `GA_ParrySuccess_LongSword.ReactionMontage` → 사본. `GA_Stagger_LongSword`는 C++에서 지워진 `StaggerMontage` 프로퍼티의 저장값이 Lyra 몽타주를 끌고 와서, 다시 저장해 털어냈다(경직 몽타주는 Chooser가 정한다). `VS_*` 4개와 두 BP의 `UnarmedAnimLayerClass` → `ABP_Sub_*_UEFN` |
| 메시 | 두 BP `AnimSourceMesh` = `SKM_UEFN_Mannequin` + `ABP_Main_UEFN`, `bDisablePostProcessBlueprint = true`. 플레이어 `VisualMesh` = 태그 `RTG_GASP_to_MS`, 적 `VisualMesh` = `ABP_VisualCopyPose`(Copy Pose From Mesh, Leader Pose 아님 — RBWC 자리). `ABP_Retarget` 리타기터 맵은 `RTG_GASP_to_MS` 하나만 남겼다. 두 Pawn 의존성 순회에서 Lyra 참조 0 |
| `RTG_GASP_to_MS` | 플레이어 경로가 되면서 처음 런타임에 쓰였고 결함이 4가지 있었다. ① Root Motion op 본 이름 빈칸 → root·pelvis. ② 원본 UEFN 리그에 없는 대상 체인 23개가 엉뚱하게 자동 매핑(허벅지·종아리 트위스트 ← 손가락 중수골·쇄골 등) → 해제 + Copy Base Pose op. ③ Run IK Rig의 다리 목표 불일치(MS `ball`, UEFN `foot`)로 달릴 때 다리가 프레임마다 100~178° 뒤집힘(원본 20~30°) → MS 리그를 **`Retargeters/IK_MS_FootGoal`(추적)**로 복제해 다리 체인 끝·목표를 `foot`으로 옮기고 다리 목표를 솔버에서 뗐다(다리 FK, 팔만 IK). op는 대상 리그를 따로 들고 있어 `assign_ik_rig_to_all_ops`로 바꿔야 한다. ④ IK를 통째로 끄면 팔 IK도 사라져 무기 왼손이 그립에서 벗어났다(사용자 PIE) → ③으로 팔 IK 복구. ⑤ 총기 견착이 몸 쪽으로 약 9cm 당겨졌다(개머리판이 어깨 관통, 사용자 PIE). 리타깃 설정(IK 끄기·FK 체인 모드·원본/오프라인 포즈 정렬·Blend To Source)으로는 줄지 않았고, 대상 포즈 자동 정렬은 위치를 4cm로 줄이는 대신 총구를 27° 숙였다(권총에서 발견) → 정렬하지 않고 **무기별 견착 보정**(아래 "무기 왼손 IK·견착" 행)으로 맞췄다. 결과: 달리기 클립 2개의 다리·트위스트·팔 프레임 간 회전과 총구 방향이 원본과 같다. 스크립트 `Scripts/AnimRetarget/FixGASPtoMSRetargeter.py [verify]` |
| 무기 왼손 IK·견착 | 리타깃만으로는 라이플·샷건 왼손이 1~2cm 어긋났다(엄지가 총을 파고듦, 사용자 PIE). 표시 메시 ABP 두 개(`ABP_Retarget`·`ABP_VisualCopyPose`)의 포즈 노드 뒤에 Two Bone IK(`hand_l` → 그립)를 넣었다. 그립 목표는 무기 소켓(롱소드)이 우선이고, 소켓이 없는 Lyra 총기는 새 `ULNPWeaponVisualSet::LeftHandGripOffset`(무기 공간, Lyra 조준 아이들에서 계산, `Scripts/AnimRetarget/ComputeLeftHandGripOffsets.py`) — `VS_Rifle`·`VS_Shotgun`. 새 `ULNPAnimInstance::LeftHandIKAlpha`가 총기 IK를 재장전·대시·경직 몽타주 가중치와 `DisableLHandIK` 커브만큼 끈다. 견착은 새 `ULNPWeaponVisualSet::RightHandCorrection`(무기 공간, 같은 스크립트가 Lyra 대비 상체-손 위치 차이로 계산: 라이플 (0.5, 8.7, −3.8)·샷건 (0.1, 9.7, −3.9)·권총 (1.9, 6.7, −4.9))을 `ABP_Retarget`의 오른팔 IK로 적용한다(플레이어만, 손 회전 유지). 사용자 PIE: 세 총 견착·총구·왼손 정상 |
| PSD 정규화 | UEFN PSD 4개에 정규화 세트가 비어 있었다(`PSN_CoreLocomotion_UEFN`은 있고 4개를 담고 있다. 1C-2 경로 이동 때 끊긴 것으로 보인다). DB마다 자기 분산으로 정규화되어 `DoNotInterrupt`로 이어지는 달리기 포즈가 항상 이겨, 멈춰도 달리기가 계속 재생됐다 → 연결. Lyra PSD는 `PSN_CoreLocomotion_Lyra`를 쓴다 |
| 리타깃 몽타주 사본 | ① 대시 4개 `BlendProfileOut`이 Lyra 스켈레톤의 `FastFeet`을 가리켜 UEFN에서 `SkeletonToCompactPose.IsValidIndex` ensure(사용자 PIE) → 같은 이름 UEFN `FastFeet`으로. 내용이 같은지는 MCP로 못 읽어 확인하지 못했다. ② 몽타주 22개 저장본에 원본 Lyra 시퀀스 의존성이 남아 있었다(메모리 참조는 사본, `PostEditChange`가 링크 정보를 다시 만들면 사라진다). 스크립트 `Scripts/AnimRetarget/FixRetargetedMontages.py`. 98개 모두 Lyra·`Weapons/` 의존 0 |
| 회귀(사용자 리슨 2인 PIE, 적도 근처) | 통과: 롱소드 콤보·휘두를 때 이동/공격 차단·적 타격·근접 보정·가드 자세·가드/패링 성공 모션, 총기 조준 자세·사격·재장전, 4방향 대시, 사망 랙돌·리스폰, 피격 움찔이 공격을 끊지 않음, 달리기·정지 후 Idle 전환(수정 뒤). 자동 PIE 로그에 몽타주·슬롯·애니 경고 없음(Mover out-of-band·Mass `CharacterMovementComponent` 오류는 이전부터 있던 노이즈) |
| 소스 보호 | `UEFN_Mannequin/`·Lyra 원본·MoverSample 저장 없음(dirty 0). `Retargeted/`는 git 제외 확인 |

**알려진 차이 — 스트레이프 방향 클립(2-2에서 복구, 2026-10-19 사용자 결정).** 전환 전 Lyra DB는 걷기·조깅마다 Fwd/Bwd/Left/Right 클립이 있어 총기(FreeAim)·락온 이동이 방향별로 재생됐다. 지금 `PSD_Walk_UEFN`·`PSD_Run_UEFN`에는 정면·호·회전 클립뿐이라 어느 방향으로 움직여도 정면 달리기가 나온다. GASP 5.8 원본에는 방향별 루프(`M_Neutral_Run_Loop_{F,B,LL,LR,RL,RR,FL,FR,BL,BR}` 등, Walk도 같은 구성)가 있으나 LootNPop에는 일부만 반입됐고 Walk 루프는 없다. 2-2가 상태별 PSD 묶음 Chooser를 만들 때 스트레이프 DB와 함께 반입한다. 허리를 이동 방향으로 돌리는 처리(Orientation Warping)는 2-3(B2) 범위다.

**범위 밖으로 기록 — ISKM 총기 적(2026-10-19 사용자 PIE).** 지상 원거리 샷건 NPC(ISKM)도 견착이 몸 쪽으로 당겨지고 왼손이 맞지 않는다. 2-0b가 건드리지 않은 `Retargeted/Enemy/` 구운 클립(정렬 안 된 `RTG_Lyra_to_GASP_noRootMotion`)의 문제이고, ISKM은 런타임 IK가 없어 클립을 다시 구워야 한다. 별도 세션에서 처리한다(해결 방향은 [TechDesign_EnemyNPC_LowLOD.md](../../TechDesign_EnemyNPC_LowLOD.md) "미해결 — ISKM 총기 적").

**재생성 절차 추가(2-0a 절차 뒤).** 2-0a 1번(`RetargetLyraToUEFN.py`) 다음에 `py ".../Scripts/AnimRetarget/FixRetargetedMontages.py"`를 실행하고 `Saved/AnimRetarget/FixRetargetedMontages.json`에서 `cleared`·`source_no_copy`가 비었는지 본 뒤 `Retargeted/Lyra/`만 저장한다. 추적 에셋(`CHT_*`·`RTG_GASP_to_MS`·`IK_MS_FootGoal`)은 저장소에 있으므로 다시 만들 필요가 없다. 런타임 리타기터를 처음부터 다시 만들 때만 `FixGASPtoMSRetargeter.py`를 돌리고, 이어서 `ComputeLeftHandGripOffsets.py apply`로 무기 그립 오프셋·견착 보정을 다시 계산해 `VS_*`를 저장한다(리타깃 결과가 바뀌면 견착 보정값도 바뀐다).

## 단위 2-1 — B0 애니 변수 공급층 + B10 Pose History·DeadBlending

`Roadmap.md` §4 2-1 행이 범위를 소유한다. 애니 선택(Chooser·PSD)은 바꾸지 않는다(2-2). 에디터는 LootNPop(8000)과 GASP(8001, 읽기 전용) 둘 다 쓴다.

계획:

1. GASP `SandboxCharacter_Mover_ABP`(+ 폰 `SandboxCharacter_Mover`)에서 `design/GASPReference.md` §2.3 값의 계산식·임계값을 읽는다. 결과 요약은 같은 문서 §2.8.
2. `ULNPAnimInstance`에 공급값을 C++로 추가한다. 상태 enum 6종(이동 모드·이동 상태·보행·자세·회전 모드·이동 방향)과 이력(`_LastFrame`·`_Recent`·`_Time`), 궤적 파생값, 가속, 지면 법선·경사, `BasedMovementDelta`, 상태 술어 bool. 속도·가속은 캐릭터 Up 접평면에 투영하고, 각도는 Up 축 기준 부호각으로 잰다(§2.4).
3. **궤적 소유권을 C++로 옮긴다.** 지금은 `ABP_Main_UEFN` EventGraph가 BP 변수 `Trajectory`를 만들고 `PoseHistoryCollector`의 핀 바인딩이 그 변수를 읽는다. C++ `NativeUpdateAnimation`은 BP EventGraph보다 먼저 돌아 BP 궤적을 그 프레임에 읽을 수 없다. 그래서 예측기 생성·궤적 갱신을 C++(`ULNPAnimInstance::Trajectory`)로 옮기고 BP 변수·함수를 지운다. 바인딩은 이름 경로(`Trajectory`)로 풀리므로 BP 변수를 지우면 같은 이름의 C++ 프로퍼티로 이어진다(엔진 `AnimGraphNodeBinding_Base`). 궤적 샘플링은 GASP 값(이력 0.033s×15, 예측 0.1s×15)으로 맞춘다.
4. 메인 인스턴스에서만 계산한다. 서브 ABP(링크 레이어)·표시 메시 ABP도 같은 C++ 부모를 쓰므로 `AnimSourceMesh`의 메인 인스턴스가 아니면 새 계산을 건너뛴다.
5. `ABP_Main_UEFN`: `PoseHistoryCollector`를 출력 직전에 두고 GASP 설정(포즈 2개, 수집 본 8개, 곡선 `Phase`)을 맞춘다. 마지막 슬롯 뒤, `ApplyWeaponPostProcess` 레이어 앞에 DeadBlending(0.25s HermiteCubic, GASP 값)을 넣는다. 무기 레이어·슬롯·AO·공중 고정·근접 보정 어댑터는 그대로 둔다.
6. 디버그 출력 CVar `LNP.Anim.DebugLocomotion`(1 화면 표시, 2 + 전이 로그)으로 값을 검증한다.

완료 조건:

- 정지·출발·회전·피벗·착지에서 상태·술어가 기대대로 바뀐다(디버그 표시·전이 로그).
- 적도 근처에서 접평면 투영 값(속도·가속·경사·회전각)이 정상이다.
- `ActorPromoted` 적과 리슨 2인 게스트에서 같은 값이 나온다.
- 근접 콤보·재장전·가드·랙돌·몽타주 슬롯 회귀가 없다.
- 커밋 뒤 깨끗한 트리에서 `Build.bat` 재확인.

### 2-1 결과 (2026-10-19, 상세 `history/Phase02_Log.md`)

| 항목 | 결과 |
|:---|:---|
| GASP 확인 | `Update_*`·술어 함수와 폰의 방향 함수를 읽어 계산식·임계값을 `design/GASPReference.md` §2.8에 정리했다. GASP 두 BP는 DSL 읽기로 dirty, 저장 안 함 |
| C++ 공급층 | `ULNPAnimInstance`: 상태 enum 6종(`ELNPMovementMode`·`ELNPMovementState`·`ELNPGait`·`ELNPStance`·`ELNPRotationMode`·`ELNPMovementDirection`)과 `_LastFrame`·`_Recent`·`_Time`, 궤적 파생값, 가속, 지면 법선·경사, `BasedMovementDelta`(Mover `OnBasedMovementApplied` 누적), `LeanAmount`, 술어 8개. 메인 인스턴스(`AnimSourceMesh`의 `GetAnimInstance() == this`)에서만 계산한다. 빌드 의존성에 `PoseSearch` 추가 |
| 궤적 소유 | 예측기·궤적을 C++로 옮기고 `ABP_Main_UEFN`의 BP 변수 4개(`Trajectory`·`Predictor`·`PreviousDesiredControllerYaw`·`Mover`)·`Initialize_MoverPredictor`·EventGraph 노드를 지웠다. 이름 경로 바인딩이 C++ `Trajectory`로 이어졌다(에디터 재시작 뒤 경고를 오류로 취급한 컴파일 통과). 샘플링은 GASP 값 0.033s×15 / 0.1s×15 |
| B10 | `PoseHistoryCollector`는 이미 출력 직전(`ApplyWeaponPostProcess` 뒤)이었다. 설정만 GASP 값으로(샘플 간격 0, 수집 본 8개, 곡선 `Phase`, 루트 이동 복구 비율 0). DeadBlending을 `AdditiveHitReact` 슬롯 뒤·`ApplyWeaponPostProcess` 앞에 넣었다(엔진 기본값 = GASP 값, 래그돌 필터 곡선만 뺐다) |
| 디버그 | CVar `LNP.Anim.DebugLocomotion` 1 = 캐릭터 위 문자열·속도/예측 속도/지면 법선 선, 2 = + 상태 변화·술어 상승 에지 로그(`[AnimB0]`, 넷 모드·역할 포함) |
| 검증(사용자 리슨 2인 PIE, 적도) | 호스트·게스트(자신·상대)·승격 적 2마리 모두 값이 나왔다. 로그 585줄에서 정지·출발(`Starting`)·착지 Light/Heavy(Heavy는 Up 속도 −690~−3914)·Spin·Pivot·TurnInPlace·방향 F·B·L·R·Strafe/Aim 확인. 회귀(근접 콤보·재장전·가드/패링·대시·피격·랙돌·리스폰) 없음 |
| PIE에서 고친 것 | `bJustTeleported` 오탐: 고속 낙하 착지 프레임, 게스트가 보는 호스트의 Actor 스냅(74회). 메시 위치 + 속력 허용치로 바꿔 착지 오탐 0, 시뮬 프록시 15회/80초(1000cm/s 이상에서 메시가 실제로 튀는 경우)로 줄였다. 남은 것은 2-5에서 정한다 |

## 단위 2-2 — B1 MM 선택 코어 + 스트레이프 복구

`Roadmap.md` §4 2-2 행이 범위를 소유한다. 시작·정지·피벗·회전 DB(B6)는 2-4, Orientation Warping·Steering(B2)·Offset Root(B3)는 2-3이다. 이번에는 **루프만** 고른다. 에디터는 LootNPop(8000)과 GASP(8001, 읽기 전용) 둘 다 쓴다.

착수 시 확인한 GASP 구성(상세 `design/GASPReference.md` §2.9, 결과 셀은 아래 2번 명령으로 덤프):

- `CHT_PoseSearchDatabases_Relaxed` 루트 8행(이동 모드·자세·이동 상태·보행)의 결과는 모두 **중첩 Chooser**(`Stand Idles`·`Stand Walks`·`Stand Runs`·`Stand Sprint`…)다. 중첩 테이블은 `<Chooser>.<표시 이름>` 경로로 컬럼까지 읽힌다. Walk·Run 테이블은 `MovementDirection`(F·B·LL·RL)·`_Recent`·술어로 나뉜 25~26행이고, ABP는 여러 결과를 한꺼번에 받아 검색한다(멀티 결과).
- 방향별 루프 DB는 `PSD_Relaxed_Stand_{Walk,Run}_{F,B,LL,RL}_Loops`, 루프 스키마는 `PSS_Relaxed_Loops`(궤적 위치·속도 XY 5샘플 + `Phase` 곡선 2개 + 골반 Heading, 포즈 채널 없음), Idle은 `PSS_Relaxed_Idle`, 정규화는 `PSN_Relaxed_All` 하나다. `LR`·`RR` 루프 DB는 Chooser가 고르지 않는다.
- 결과 셀(`ResultsStructs`)과 PSD 항목(`DatabaseAnimationAssets`)은 MCP·Python으로 읽히지 않는다. 둘 다 C++에서는 접근할 수 있다.

계획:

1. **반입.** 루프 DB 10개가 쓰는 시퀀스 27개 중 LootNPop에 없는 26개를 GASP 원본 경로 그대로 복사한다(LootNPop 에디터를 끈 상태). 외부 의존(Foley Walk/Run·`PSD_SM_CMC_Idles`·커브 압축 설정)은 이미 `Removed` 리다이렉트·반입 목록에 있다.
2. **GASP 참조 편집기 명령**(`LootNPopEditor`): GASP `Content/`를 임시 마운트 포인트로 올려 LootNPop 에디터에서 GASP 구성 에셋을 **읽기만** 한다. ① 덤프 — Chooser 결과 행·중첩 테이블, PSD 항목 설정을 JSON으로(스크린샷 대신). ② 복제 — GASP PSD·PSS를 LootNPop 경로(`Characters/MotionMatchingData/`)에 새 구성 에셋으로 복제하고, 복제본끼리 참조를 다시 잇는다. GASP 패키지는 저장하지 않는다(AN-009·AN-016 ②).
3. **LootNPop 구성 에셋**: `PSS_Relaxed_{Loops,Idle}_UEFN`, `PSN_Relaxed_UEFN`, PSD 10개(`PSD_Stand_Idles_UEFN`, `PSD_Stand_{Walk,Run}_{F,B,LL,RL}_Loops_UEFN`, `PSD_Stand_Sprint_Loops_UEFN`), 평면 멀티 결과 Chooser `CHT_PoseSearchDatabases_UEFN`(행 입력 `ELNPMovementMode`·`ELNPMovementState`·`ELNPGait`·`ELNPMovementDirection`). 스트레이프는 `MovementDirection`이 이미 회전 모드를 반영한다(OrientToMovement·Sprint면 F). 공중 행은 기존 `PSD_Jump_UEFN`(MM 노드는 공중에서 꺼져 있다, AN-007).
4. **C++**(`ULNPAnimInstance`): `GetMMInterruptMode`·`GetMMBlendTime`(GASP 식, §2.8)과 MM 노드 갱신 함수 하나(Chooser 멀티 평가 → `SetDatabasesToSearch` → 블렌드 시간 덮어쓰기). `ABP_Main_UEFN` `OnUpdate_MotionMatching`은 이 함수를 호출만 한다. 디버그 CVar 2에서 고른 DB가 바뀔 때 로그(`[AnimB1]`).
5. 승격 적 `MovementState` 깜빡임 대응: Chooser는 현재 값을 쓴다(GASP와 같다). 근거 — 관찰된 깜빡임 주기(0.1~0.5초)가 `_Recent` 유지 시간(0.1초)보다 길어 `_Recent`로는 거의 걸러지지 않고 모든 정지·출발에 지연만 더한다. 이동 상태 변화는 DB 인터럽트 조건이 아니어서(`DoNotInterrupt`) 진행 중인 포즈가 유지된다. PIE에서 떨림이 보이면 다시 정한다.
6. 유지: 공중 `Fall_Loop` 고정(AN-007), 무기 레이어·슬롯 3종·AO·근접 보정 어댑터, 슬롯 그룹 런타임 등록(AN-020). 옛 `CHT_PSD_UEFN`·`PSD_{Idle,Walk,Run}_UEFN`은 되돌림용으로 남긴다(런타임 미참조).

완료 조건:

- Idle·Walk·Run·Sprint 루프 전환, 정지 뒤 Idle 복귀, 총기·락온 스트레이프 방향별 클립(F·B·좌·우 DB. 대각 입력은 히스테리시스 임계로 가장 가까운 DB가 고르고, 허리를 이동 방향으로 돌리는 보정은 2-3 Orientation Warping), 무기별 상체 레이어·AO 유지
- 근접 콤보·재장전·가드·랙돌 회귀 없음, 적도·`ActorPromoted`(게스트 재시뮬 포함)·리슨 2인
- 커밋 뒤 깨끗한 트리에서 `Build.bat` 재확인

### 2-2 결과 (2026-10-19, 상세 `history/Phase02_Log.md`)

| 항목 | 결과 |
|:---|:---|
| GASP 확인 | 루트 8행 → 중첩 Chooser 8개, Walk 25행·Run 26행·Idle 8행·Sprint 6행의 결과를 C++ 덤프로 얻었다(스크린샷 불필요, 요약 `design/GASPReference.md` §2.9) |
| 반입 | 시퀀스 26개(목록 `phases/Phase01C_GASPAssetRefresh.md` "2-2 반입분", 해시 `Saved/AssetRefresh/2026-10-19/import_list_2-2_sha256.txt`). 새 노티파이·외부 의존 없음(기존 `Removed` 리다이렉트로 충분). git 제외 확인 |
| 편집기 명령 | `LNP.Editor.GASPRef`(GASP Content 임시 마운트 + 불러오는 동안만 패키지 리다이렉트 → 덤프·복제), `LNP.Editor.SetChooserResults`(Chooser 결과 행 = 에셋, 컴파일). `LootNPopEditor`에 `AssetTools`·`Chooser`·`Json`·`PoseSearch` 의존 추가 |
| 구성 에셋 | `PSS_Relaxed_{Loops,Idle}_UEFN`·`PSN_Relaxed_UEFN`·PSD 10개(GASP 복제, 내부 참조 재연결, `/GASPRef/` 잔여 0), `CHT_PoseSearchDatabases_UEFN`(11행, `CHT_PSD_UEFN` 복제 후 결과·컬럼 교체 — 복제 원본의 폴백 결과 `PSD_Idle_UEFN`은 `ReplaceAssetRefs`로 새 Idle DB로) |
| C++ | `ULNPAnimInstance::UpdateMotionMatchingNode`(Chooser 멀티 평가 → `SetDatabasesToSearch` → 블렌드 시간 덮어쓰기)·`GetMMInterruptMode`·`GetMMBlendTime`, 프로퍼티 `LocomotionDatabaseChooser`(ABP 기본값), `[AnimB1]` 로그. `FMotionMatchingBlueprintBlendSettings` 생성자가 PoseSearch 밖으로 내보내지지 않아 리플렉션으로 인스턴스를 만든다 |
| ABP | `OnUpdate_MotionMatching` = `UpdateMotionMatchingNode(Node)` 한 줄. MM 노드 GASP 값(0.5s HermiteCubic, `FastFeet_InstantRoot`, 재생 속도 0.85~1.15, 노티파이 필터). 경고 없이 컴파일 |
| PIE에서 고친 것 | ① 적도에서 앞으로 걸어도 옆·뒤 클립: 엔진 `CalculateDirection`이 속도를 `GetSafeNormal2D`로 정규화(월드 Z 제거) → `Direction`을 캐릭터 Up 둘레 부호각으로. ② 걷는데 Idle 유지(대시로 풀림): Idle 스키마는 궤적 가중치 0이라 `DoNotInterrupt`에서 진행 중인 Idle 포즈가 계속 이김 → 출발 프레임에도 인터럽트(2-4 Starts DB 때 재검토) |
| 검증(사용자 1·2인 PIE, 적도) | 걷기·달리기·질주·정지, 8방향 이동, ADS 이동, 총기 상체 자세, 가드 이동·가드·패링, 재장전, 근접 콤보, 승격 적 전투 — 어색함 없음. 자동 PIE 경고 0 |
| 남긴 것 | 옛 `CHT_PSD_UEFN`·`PSD_{Idle,Walk,Run}_UEFN`(되돌림용, 런타임 미참조). 감속 구간은 Stops DB가 없어 `MovementState`가 Idle이 되는 순간(약 330cm/s) Idle DB로 넘어간다 — 2-4 |

## 단위 2-3 — B3 Offset Root Bone + B2 블렌드 스택 절차 보정

`Roadmap.md` §4 2-3 행이 범위를 소유한다. 감속(Stops)·출발 인터럽트 예외·전환 DB는 2-4, 발 배치는 2-5다. 에디터는 LootNPop(8000)과 GASP(8001, 읽기 전용) 둘 다 쓴다.

착수 시 확인한 GASP 구성(요약 `design/GASPReference.md` §2.10):

- MM 노드 블렌드 스택 그래프: 입력 → 컴포넌트 공간 → CopyBone `foot_l→ik_foot_l`·`foot_r→ik_foot_r` → Steering(활성 = `EnableSteering`, 목표 = `Get_DesiredFacing`, 절차 목표 시간 = `Get_ProceduralTargetTime`) → Steering(제자리 회전, 활성 = 현재 DB 태그에 `TurnInPlace`, 목표 같음) → OrientationWarping(α = 곡선 `Enable_Warping`, 방향 = `LastNonZeroVelocity`, 공간 `RootBoneTransform`) → ResetRoot → 로컬 공간. 함수 입력은 태그 `MMBlendStackInput` 노드 참조다(블렌드 샘플마다 자기 애니의 곡선을 읽는다). MM 경로에 Stride Warping은 없다.
- `OffsetRootBone`(태그 `OffsetRoot`, Graph 모드): 회전 반감기 0.1, `MaxTranslationError` ← CVar(GASP 기본 0 — 위젯이 바꾸는지는 미확인), 모드·반감기 ← `Get_OffsetRoot*`, `OnUpdate`가 base 이동량을 `ApplyDeltaToOffsetRootBone`으로 더한다.
- **엔진 구현에서 찾은 구면 함정 3건**(`AnimNode_OffsetRootBone.cpp`): ① `GroundNormal` 기본값이 월드 (0,0,1)이고 지상이면 가상 루트 위치를 그 평면에 투영한다. ② `bClampToTranslationVelocity`가 켜지면 보정량 방향을 `GetSafeNormal2D()`(월드 Z 제거)로 만든다. ③ Accumulate·Interpolate 모드는 캡슐 회전 변화를 가상 루트에 더하지 않는다 — 구면을 걸어가면 캡슐 Up은 돌아가는데 가상 루트 Up은 남아 메시가 점점 기운다. Steering·OrientationWarping은 가상 루트 기준 상대 회전·로컬 변환이라 루트 Up이 중력 Up이면 안전하다.

계획:

1. **C++(`ULNPAnimInstance`, 헤더 변경 → 에디터 재시작 필요).**
   - Offset Root 모드·반감기 함수: 전신 몽타주 슬롯(`DefaultSlot`·`FullBody`) 재생 중 Release(이동·회전), 지상 이동 Interpolate·정지 Release, 공중 Release, 회전은 그 밖에 Accumulate. 반감기 Idle 0.1·Moving 0.3. 지면 법선 함수 = 캐릭터 Up(함정 ①). `bClampToTranslationVelocity`는 끈다(함정 ②).
   - `NativeThreadSafeUpdateAnimation`(그래프 갱신 직전, 워커)에서 태그 `OffsetRoot` 노드를 찾아 base 이동량(2-1 `BasedMovementDelta`)을 더하고, 가상 루트 Up을 컴포넌트 Up으로 맞춘다(함정 ③, 스윙만 보정하고 Yaw는 그대로). 가상 루트 트랜스폼(`OffsetRootTransform`)을 보관한다.
   - Steering 함수(블렌드 스택 입력 노드 참조를 받는다): 활성 여부(현재 애니 활성 & 움직임, 또는 공중), 목표 facing(곡선 `SteeringTargetTime` 0~1 → 궤적 0.1~1.5s facing, 컴포넌트 Up으로 정렬), 절차 목표 시간(같은 곡선 → 0.1~0.3), 제자리 회전 DB 여부(MM 검색 결과 DB 태그), OrientationWarping α(곡선 `Enable_Warping`).
   - 가상 루트가 캡슐과 다르게 돌므로 Aim Offset 각도와 `FutureFacingDelta` 첫 기준을 가상 루트 기준으로 바꾼다(GASP도 `RootTransform` = 오프셋 루트 기준).
2. **`ABP_Main_UEFN`.** 블렌드 스택 그래프에 GASP와 같은 체인을 만들고 입력 노드에 태그 `MMBlendStackInput`을 단다. AnimGraph `DeadBlending` 뒤·`ApplyWeaponPostProcess` 앞에 `OffsetRootBone`(태그 `OffsetRoot`)을 넣고 핀을 C++ 함수에 잇는다. 분기는 C++ 함수 안에만 둔다(AN-003).
3. **구면 확인.** 디버그 표시(`LNP.Anim.DebugLocomotion`)에 궤적 facing Up과 캐릭터 Up의 각도, 가상 루트 오프셋(거리·Yaw·기울기)을 더해 적도에서 잰다.
4. 유지: 무기 레이어·슬롯 3종·AO 경로·공중 `Fall_Loop` 고정(AN-007)·근접 보정 Motion Warping 어댑터(전신 몽타주 중 Release)·슬롯 그룹 런타임 등록(AN-020). Stride Warping은 미끄러짐이 보일 때만.

완료 조건:

- 대각·스트레이프에서 허리가 진행 방향을 따라 돌고 발 미끄러짐이 줄어든다. 방향 전환 시 루트가 부드럽게 따라온다(캡슐-메시 벌어짐 ≤ `MaxTranslationError`).
- 근접 콤보·대시·재장전 중 Release, 근접 보정 워핑 유지, 움직이는 base, 적도·`ActorPromoted`(게스트 재시뮬)·리슨 2인에서 회귀 없음.
- 커밋 뒤 깨끗한 트리에서 `Build.bat` 재확인.

### 2-3 결과 (2026-10-20, 상세 `history/Phase02_Log.md`)

| 항목 | 결과 |
|:---|:---|
| GASP 확인 | 블렌드 스택 체인·노드 값·`Get_OffsetRoot*`·`OnUpdate_OffsetRootBone`은 MCP로, 바인딩은 사용자 Ctrl+C 텍스트(`Saved/AnimOverhaul/gasp_2-3_*.txt`, git 제외)로 확인(요약 `design/GASPReference.md` §2.10). CopyBone은 이동·회전·스케일 복사가 모두 꺼져 사실상 무동작이고, Steering `AnimatedTargetTime`의 실제 값은 핀 10이다(노드 구조체 2는 낡은 값). OrientationWarping `WarpingSpace` 바인딩은 Offset Root가 켜져 있으면 `RootBoneTransform`인 상수다 |
| C++ | `ULNPAnimInstance`: B2 함수 5개(`ShouldEnableSteering`·`GetSteeringTargetFacing`·`GetSteeringProceduralTargetTime`·`IsTurnInPlaceDatabaseActive`·`GetOrientationWarpingAlpha`), B3 함수 4개(이동·회전 모드, 이동 반감기, 지면 법선 = 컴포넌트 Up), `NativeThreadSafeUpdateAnimation`이 태그 `OffsetRoot` 노드에 base 이동량과 Up 정렬을 넣는다. AO·`FutureFacingDelta`는 가상 루트 기준. 빌드 의존성 `AnimationWarpingRuntime`·`BlendStack`. 디버그 표시·`[AnimB3]` 로그 |
| ABP | 블렌드 스택 그래프: GASP 노드를 C++ 함수 참조로 바꾼 텍스트를 사용자가 붙여넣었다(MCP `create_node`는 중첩 그래프에서 막힌다), 입력·출력 연결과 입력 노드 태그 `MMBlendStackInput`은 MCP. AnimGraph: `DeadBlending` → `OffsetRootBone`(Graph, 태그 `OffsetRoot`, 회전 반감기 0.1, `MaxTranslationError` 30, `MaxRotationError` 90, 속도 클램프 끔) → `ApplyWeaponPostProcess`, 핀 4개를 C++ 함수 노드에 연결(MCP). 컴파일·저장, 무인 PIE 새 경고 0 |
| 구면 확인(로그 약 3,600줄, 적도·리슨 2인·승격 적) | 가상 루트 기울기 최대 0.98°(95%가 0.1° 미만) — Up 정렬 동작. 궤적 facing 기울기는 이동 중 1° 미만, 스폰·승격 첫 프레임만 약 90°(빈 이력) → `GetSteeringTargetFacing`의 Up 정렬이 거른다. 움직이는 발판(`LNP.DynamicTerrain.PlaceRider`) 위 정지 시 base 이동량을 프레임마다 정확히 상쇄(반영 뒤 지연 0.00cm) |
| 사용자 PIE | 캡슐이 먼저 가고 메시가 30cm 이내에서 부드럽게 따라붙음, 총기·락온 스트레이프·ADS·가드 8방향 "매우 자연스러움", 조준 중 급회전 총구 이탈 없음, 재장전·근접 콤보·락온·근접 보정 회귀 없음, 2P에서 본 1P도 자연스러움, 발판 이동 문제 없음 |
| PIE에서 고친 것 | 정지 중 룩만 돌리면 허리가 180°까지 꺾임(제자리 회전 DB가 없어 메시가 돌 방법이 없다) → `MaxRotationError` 90°로 그 이상은 하체가 캡슐을 따라 미끄러지며 돈다. 발을 디디는 회전은 2-4 TurnInPlace |
| 남긴 것 | 질주 옆·뒷걸음 어색함(질주 클립이 정면뿐, GASP도 질주는 F 고정) → 정면에서만 질주 최고속도를 내는 이동 변경은 별도 작업으로 제안. Stride Warping은 미끄러짐 보고가 없어 넣지 않았다. `OrientationWarping` 끄고 켜기 차이는 사용자가 거의 못 느꼈다(방향별 DB가 이미 각도를 줄여 남는 보정이 대각 약 45° 몫뿐) |

## 단위 2-4 — B6 정지·회전 전환 + B7 착지 검토

`Roadmap.md` §4 2-4 행이 범위를 소유한다. 발 배치·경사(2-5)와 Leans(2-6)는 범위 밖이다. 에디터는 LootNPop(8000)과 GASP(8001, 읽기 전용) 둘 다 쓴다.

착수 시 확인한 GASP 구성(2-2 덤프 `Saved/AnimOverhaul/GASPRef_2-2.json`의 중첩 테이블, 요약 `design/GASPReference.md` §2.9·§2.11):

- 중첩 테이블 행은 **멀티 결과의 합집합**이다. 예: Walk 출발 프레임은 `Walk_F_Starts`와 `Walk_F_Loops`가 함께 나오고 MM이 비용으로 고른다. 전환 행은 술어(`IsStarting`·`IsPivoting`·`ShouldSpinTransition`·`ShouldTurnInPlace`·`JustLanded_*`)로 켜지고, 루프 행은 술어와 무관하게 "직전 프레임 OnGround"면 늘 나온다.
- 피벗·스핀 행은 `MovementDirection_Recent`를 **비트마스크**(MultiEnum)로 본다 — "지금 방향 = X이고 최근 방향 ≠ X 계열". `F_Turns`는 최근 방향도 F(방향 회전 모드의 급선회)다. Run의 `F_GaitTransitions`는 직전 보행 Walk·Sprint(마스크)일 때만.
- Stops는 Idle 테이블에서 `Speed2D` 구간(20~170 Walk·170~400 Run·400~ Sprint)으로, TurnInPlace는 `ShouldTurnInPlace`, 착지는 `JustLanded_Light/Heavy`(이번 프레임만)로 고른다.

계획:

1. **덤프·복제.** 전환 DB 41개(Stops 3, TurnInPlace 1, Starts 9, Turns 3, Pivots 8, Spins 8, GaitTransitions 1, Lands 8)와 스키마를 `LNP.Editor.GASPRef`로 덤프해 태그·스키마·시퀀스를 확인한다. 복제는 기존 `PSS_*_UEFN`·`PSN_Relaxed_UEFN`에 다시 잇는다(편집기 명령에 "기존 복제본으로 잇기" 추가).
2. **반입.** 새 DB가 참조하는 시퀀스 중 LootNPop에 없는 것을 GASP 원본 경로 그대로 복사한다(LootNPop 에디터를 끈 상태). 목록·해시는 `Saved/AssetRefresh/2026-10-20/`, `README.md` §4와 `phases/Phase01C_GASPAssetRefresh.md` 반입 표를 갱신한다.
3. **Chooser.** `CHT_PoseSearchDatabases_UEFN`(평면 멀티 결과)에 GASP 중첩 테이블과 같은 행을 펼쳐 넣는다. 입력은 2-1 술어·`_Recent`·`_LastFrame`. 루프 행에 "직전 OnGround"를 넣는다(착지 프레임은 Lands 행이 받는다).
4. **C++ 재검토.** 출발 프레임 인터럽트 예외(Starts가 생기면 필요한지), 선택 DB 로그, 질주 방향 게이트(§4.3)로 Sprint↔Run이 자주 바뀔 때 GaitTransitions가 튀는지.
5. **TurnInPlace·착지.** TurnInPlace DB 태그, 정지 회전 발 디딤, `MaxRotationError` 90° 유지 여부. 착지는 AN-007(공중 `Fall_Loop` 고정) 안에서 지상 쪽 Lands DB만 넣을지 판단한다.
6. **PIE.** 출발·정지·피벗·제자리 회전·보행 전환·착지, 회귀(근접 콤보·대시·재장전·가드), 적도·승격 적·리슨 2인.

### 2-4 결과 (2026-10-20, 상세 `history/Phase02_Log.md`)

| 항목 | 결과 |
|:---|:---|
| GASP 확인 | 중첩 테이블 행 조건·결과는 2-2 덤프에 이미 있었다. 새 DB 41개·스키마 7개는 `LNP.Editor.GASPRef`로 덤프했다(`GASPRef_2-4.json`). 참조 시퀀스는 반입 전이라 덤프에 `None`으로 나와 GASP MCP `get_dependencies`로 얻었다. TurnInPlace DB는 태그 `TurnInPlace`를 갖고, 착지 스키마 `PSS_Relaxed_Jump`는 3D 위치·속도 방향(루트 공간)이라 구면에서도 쓸 수 있다 |
| 반입 | 시퀀스 272개 중 없던 **239개**(있던 33개는 GASP 5.8과 해시 동일). 새 외부 의존 없음(Foley Walk/Run·EarlyTransition·커브 압축·`AM_BakePhaseCurve…`, 모두 기존 리다이렉트·반입분). 목록·해시 `Saved/AssetRefresh/2026-10-20/import_list_2-4{,_sha256}.txt`. git 제외 확인 |
| 구성 에셋 | 스키마 7개(`PSS_Relaxed_{Stops,StandTurn,Jump,Starts,Pivots,WalkSpins,RunSpins}_UEFN`), PSD 41개(`PSD_Stand_*_UEFN`), `PSN_Relaxed_UEFN` 51개 DB. 기존 `PSS_Relaxed_{Loops,Idle}_UEFN`·`PSN_Relaxed_UEFN`에 다시 이었다(편집기 명령 `relink`). GASP 경로 참조 0 |
| Chooser | `CHT_PoseSearchDatabases_UEFN` 11 → **52행**, 컬럼 14개(이동 모드·상태·보행·방향, 최근 방향·직전 보행 마스크, 직전 이동 모드, 속력 구간, 술어 6개). 루프 행에 "직전 OnGround"를 넣었다. 행 정의 원본은 `Scripts/AnimOverhaul/BuildLocomotionChooser_2-4.py`(결과 목록·컬럼 JSON 생성), 요약은 `design/GASPReference.md` §2.11 |
| C++ | 인터럽트 예외 두 가지: 출발 프레임(2-2 유지 확정) + **정지 중 제자리 회전 요청인데 아직 회전 DB가 아님**(새로). `[AnimB6]` 선택 DB 로그. 편집기 명령: 복제 `relink`, `SetChooserResults @목록파일` |
| 출발 예외 재검토 | CVar로 끄고 PIE: 정면 출발 때 Idle 포즈로 미끄러지는 일이 종종, 정지 뒤 걷기 유지 1회 → **유지**. Starts DB가 있어도 진행 중 Idle 포즈(궤적 가중치 0 스키마)가 이긴다. CVar는 지웠다 |
| 제자리 회전 | 첫 PIE: 술어 96회에 회전 DB 선택 4회, 깜박이며 뚝 돎(Idle 진행 포즈가 이김, 가상 루트만 90°까지 벌어짐) → 위 인터럽트 추가 뒤 술어 59회·선택 33회(나머지는 회전 클립 재생 중), 사용자 "깜박임·뚝 끊김 없음, 발을 디디며 돈다, 약간 미끄러짐은 남음". 정지 중 가상 루트 Yaw p95 73°·최대 100°. `MaxRotationError` 90°는 **안전망으로 유지** |
| 착지 | Lands 8개 DB를 넣었다 — 공중은 그대로 `Fall_Loop` 고정(AN-007)이고, 착지 DB는 지상 첫 프레임(`JustLanded_*`)에만 Chooser에 오른다. 스폰·점프·낙하에서 `Idle_Lands_Light/Heavy`·`Run_Lands_*`가 뽑혔고 사용자 PIE에서 어색함 없음 |
| 보행 전환 | 질주 방향 게이트로 Sprint↔Run 35회 전환 동안 `Run_F_GaitTransitions`는 한 번도 뽑히지 않았다(직전 보행 조건이 한 프레임뿐이고 `DoNotInterrupt`). 튀는 문제는 없다 |
| 검증(사용자 1·2인 PIE, 적도) | 출발·정지·피벗·제자리 회전·보행 전환·착지, 근접 콤보·대시·재장전·가드 회귀 없음, 2P 적도·승격 적 어색함 없음. 승격 적 DB 변경 13분간 78회(깜빡임 없음). 새 경고 0(Mover 보정 경고는 기존 노이즈) |
| 남긴 것 | 피벗·스핀은 거의 안 뽑힌다(선택 1회씩) — LootNPop은 고속에서 큰 호를 그려 `Trj_TurnAngle` 75°에 잘 못 닿는다(2-1 관찰과 같다). 제자리 회전 발 미끄러짐 소량(발 배치 2-5에서 다시 본다) |

## 단위 2-5 — B4 발 배치 + B5 경사

`Roadmap.md` §4 2-5 행이 범위를 소유한다. Leans(2-6)·물리 히트리액트(Phase 3)는 범위 밖이고, 표시 ABP의 RBWC 자리는 건드리지 않는다. 에디터는 LootNPop(8000)과 GASP(8001, 읽기 전용) 둘 다 쓴다.

착수 시 확인한 GASP 구성(상세 `design/GASPReference.md` §2.12):

- `Procedural_Feet` 레이어: 입력 → 캐시 `PreProceduralPose` → 이동 모드 Blend(기본 0.3s, InAir 0.2s, Ragdoll 0) — OnGround는 Bool(CVar `a.AnimNode.FootPlacement.Enable`) 참이면 `CR_Biped_FootPlacement`, 거짓이면 `LegIK`(ik_foot 목표), Sliding·Traversing·InAir는 `LegIK`, Ragdoll은 IK 없음. 몽타주 예외는 없다.
- CR 핀: `Ground Normal` ← 캐릭터 구조체 `GroundNormal`(월드), `GroundMovementWorldDelta` ← `BasedMovementDelta`, `DoRaycast` true, `Enable Foot Pinning` ← `AllowFootPinning`(= OnGround, `IsMoving`과의 AND는 결과에 안 이어져 있다), `Enable Slope Warping`·`WorldZDamperEnabled` ← `AllowSlopeWarping`(OnGround·Sliding), `Has Teleported` ← `JustTeleported`, `ForceReset` ← `OnBecomeRelevant`에서 켜는 변수.
- CR 구면 점검(그래프 전체를 서브 핀 단위로 덤프): 접지 트레이스·평면 보정·골반·다리 IK는 리그 공간 Z(= 컴포넌트 Up)를 쓰고, 트레이스는 소유 Actor를 무시한다(엔진 `RigUnit_WorldCollision.cpp`). **월드 축 성분을 직접 쓰는 곳은 세 군데**다 — `UpdateRootDamper`·`Reset`(월드 Z 높이 감쇠, 적도에서는 루트가 행성 반지름만큼 튄다), `Update Foot Pinning`(핀 = 월드 XY는 핀, 월드 Z는 애니), `ComputePinnedToeTransform`(같은 섞기 + 월드 XY 평면 Yaw 보정).
- 경사 BS `BS_Relaxed_{Walk,Run}_Loop_F_Slopes`는 **SM 경로 Chooser(`CHT_MoverCharacterAnimations_PoseMatch`)만** 참조한다. MM 경로(AN-015)의 경사 대응은 CR slope warping뿐이다.

계획:

1. **CR 구성 에셋.** `LNP.Editor.GASPRef clone`으로 `Content/Characters/ControlRigs/CR_Biped_FootPlacement_UEFN`에 복제(AN-016 ②). 복제본 함수 참조가 GASP 경로 라이브러리를 가리키므로 자기 라이브러리로 다시 잇고, 구면 세 곳을 리그 공간·컴포넌트 Up 기준으로 고친다(`Scripts/AnimOverhaul/FixFootPlacementCR_2-5.py`, 에디터 Python).
2. **C++(`ULNPAnimInstance`, 헤더 변경 → 에디터 재시작).** CR 분기·리셋·디버그 함수. 입력은 2-1 공급값(`GroundNormal`·`BasedMovementDelta`·`bJustTeleported`·`bIsOnGround`).
3. **`ABP_Main_UEFN` 발 배치.** `OffsetRootBone` 뒤에 둔다. 지상 = CR, 공중 = IK 없음(아래 결정), 몽타주 예외 없음(GASP와 같다).
4. **B5(사용자 결정 2026-10-20, AN-021).** GASP MM 경로 그대로 경사는 CR slope warping만 쓴다. 경사 BS·시퀀스 반입과 Chooser 경사 행은 하지 않는다 — 경사 BS는 SM 경로 Chooser만 참조하고, PSD 스키마에 경사 채널이 없어 MM이 경사각을 모른 채 BS 샘플을 고르게 된다.
5. **PIE.** 평지·경사·계단·턱 접지, 제자리 회전 발 고정, 움직이는 발판, 공중↔착지, 근접 콤보·대시·재장전·가드·랙돌 회귀, 적도·승격 적·리슨 2인.

정한 것(근거는 `design/GASPReference.md` §2.12):

- **`UseIKBoneTargets` 끔.** CR은 기본으로 `ik_foot_*`를 애니 발 목표로 쓴다. GASP 클립은 ik_foot = foot이지만, Lyra에서 리타깃한 몽타주·`Fall_Loop`은 ik_foot을 믿을 수 없다. 끄면 FK 발끝(`ball_*`)이 목표다. 그래서 2-3의 블렌드 스택 CopyBone `foot→ik_foot`은 계속 무동작으로 둔다.
- **공중은 IK 없음.** GASP 공중 LegIK는 ik_foot으로 다리를 푼다. LootNPop 공중 포즈(`Fall_Loop`, Lyra 리타깃)는 ik_foot이 위와 같은 이유로 믿을 수 없고, ik_foot = foot이면 LegIK는 무동작이다.
- **시뮬 프록시 `bJustTeleported` 오탐은 그대로 리셋에 쓴다.** 남은 오탐은 메시가 실제로 한 프레임에 크게 튄 경우(2-1)라 핀·바닥 평활을 버리는 것이 맞다.
- **분기 대신 CR 노드 알파(Bool).** 공중이 IK 없음이라 GASP처럼 별도 분기·캐시 포즈를 두지 않고 CR 노드 알파를 `ShouldUseFootPlacementRig`(블렌드 인 0.3s·아웃 0.2s)로 준다. 알파 0이면 CR이 멈추므로 착지 프레임에 `ForceReset`을 켠다(`ShouldResetFootPlacement`). 레이어 함수가 아니라 AnimGraph 안 노드 하나다.
- **움직이는 base 위에서는 핀 고정·루트 감쇠 끔(PIE에서 추가).** `IsOnStaticGround` = 지상 & base가 움직이는 컴포넌트가 아님. 경사 보정은 켠다. 근거는 아래 결과 표.

### 2-5 결과 (2026-10-20, 상세 `history/Phase02_Log.md`)

| 항목 | 결과 |
|:---|:---|
| GASP 확인 | CR 그래프 42개를 서브 핀 단위까지 MCP로 덤프, `Procedural_Feet`는 0-2 Ctrl+C 텍스트와 판정 함수 MCP 읽기. 요약 `design/GASPReference.md` §2.12 |
| CR | `CR_Biped_FootPlacement_UEFN`(GASP 복제). 함수 참조 44개 재연결, 구면 수정 3곳(루트 감쇠 = Up 방향 지연량, 핀 위치·고정 발끝 = 리그 공간 섞기), 루트 감쇠는 base 몫을 뺀 자기 이동량만 본다. 재현 스크립트 `Scripts/AnimOverhaul/FixFootPlacementCR_2-5.py` |
| C++ | `ShouldUseFootPlacementRig`·`ShouldResetFootPlacement`·`IsOnStaticGround`·`IsFootPlacementDebugEnabled`, CVar `LNP.Anim.FootPlacement`·`LNP.Anim.FootPlacementOnMovingBase`·`LNP.Anim.DebugFootPlacement`. 텔레포트 판정은 base가 옮긴 몫을 뺀다. 정지 프레임 인터럽트 추가. 가상 루트 회전 오차 90° 제한을 C++로 옮겼다(아래) |
| ABP | `OffsetRootBone` → CR 노드(붙여넣기 1회) → 무기 후처리. 핀 11개를 C++ 값·함수에 연결(MCP). `OffsetRootBone.MaxRotationError` −1 |
| B5 | AN-021 — 반입·Chooser 행 없음, CR slope warping만 |
| PIE에서 고친 것 | ① 이동 패널 위 CR 매 프레임 리셋: Mover 속도는 base 기준이라 0인데 메시는 패널과 함께 움직여 텔레포트로 판정 → base 몫 제외. ② 아주 느린 정지 뒤 걷기 유지(2-4부터): 진행 중 걷기 포즈가 Idle·Stops 후보를 이김 → 정지 프레임 인터럽트. ③ 패널 위 다리 끊김(핀이 끌리다 풀림)·전신 움찔(패널 상하 속도가 바뀌는 프레임마다 루트 10cm 넘게 튐): CR 평가 시점의 컴포넌트 이동엔 이번 프레임 base 이동이 들어 있는데 공급 base 이동량은 한 프레임 전 값 → 움직이는 base 위에서 핀 고정·루트 감쇠 끔(A/B CVar로 원인 확인). ④ **적도에서 빠르게 돌면 골반·발이 수 m 튐**: 엔진 `OffsetRootBone`의 `MaxRotationError` 클램프가 로컬 오차를 월드 쪽에 곱해 가상 루트가 최대 90° 기욺 → `ik_foot_root` 기준 바닥 평면이 서서 CR 교차점이 수 m 밖으로 → 노드 클램프 끄고 C++에서 Up 둘레로 제한(구면 함정 4번째) |
| 검증(사용자 PIE, 적도) | 평지·여러 각도 경사 오르내림 접지, 제자리 회전 발 미끄러짐 크게 줄어듦, 점프·낙하 착지, 패널 위 정지·이동, 빠른 회전, 근접 콤보·대시·재장전·가드·패링·랙돌 회귀 없음, 1P·2P |

## 단위 2-6 — B8 Additive Leans + ActorPromoted 적 전체 회귀

`Roadmap.md` §4 2-6 행이 범위를 소유한다. Phase 2의 마지막 단위다. GASP Aim Offset(B9)·래그돌 연동(B11)·손 IK(B12)는 범위 밖이다. 에디터는 LootNPop(8000)과 GASP(8001, 읽기 전용) 둘 다 쓴다.

착수 시 확인한 GASP 구성(0-2 Ctrl+C 텍스트 `Saved/GASPRef/leans.txt`, 레이어 `AdditiveLeans`):

- 입력 포즈를 캐시 `PreLeanPose`로 둔다. 몸통은 `BS_Relaxed_Walk_Leans`와 `BS_Relaxed_Run_Leans`(입력 X·Y ← `LeanAmount.X/Y`)를 TwoWayBlend로 섞는다. α는 `Speed2D`를 200~320에서 0~1로 매핑하고 클램프·보간한다. 이 결과를 Mesh Space Additive(α 1)로 얹어 캐시 `BodyLeans`를 만든다. 머리는 `BS_Relaxed_Lean_Head`를 그 위에 다시 Mesh Space Additive로 얹어 캐시 `Body+HeadLeans`를 만든다.
- Bool(← `EnableAO`, 블렌드 0.3s): 참이면 `BodyLeans`, 거짓이면 `Body+HeadLeans`를 쓴다. GASP 주석은 "AO와 머리 기울기를 함께 쓰면 머리가 과회전한다"이다.
- 이동 상태 Blend(Moving만 기울임, 0.3s) → 이동 모드 Blend(Sliding만 통과, 0.3s) → TwoWayBlend(α = 곡선 `Disable_AdditiveLeans`, 보간 속도 5. 일부 회전 애니가 지나치게 기울지 않게 막는다) → 출력. 공중은 이동 상태가 Moving이면 기울인다.
- AnimGraph 위치는 MM 코어 → `Procedural_PreLayering` → **`AdditiveLeans`** → `AimOffset` → 래그돌 → `DefaultSlot`이다. 몽타주는 기울기를 덮는다.
- BS 입력에는 스무딩이 없다(축별 보간 시간 0). `LeanAmount`도 평활하지 않은 원값이다.

계획:

1. **반입.** BS 3개와 시퀀스 13개(`Poses/M_Relaxed_{Walk,Run}_Pose_Lean_{Base,F,B,L,R}`·`M_Relaxed_Stand_Pose_Head_Turn_{Base,L,R}`, GASP MCP `get_dependencies`)를 확인한다. 16개 모두 LootNPop에 이미 있다. 시퀀스 13개는 GASP 5.8과 해시가 같다. BS 3개는 1C의 "경로 외 동일 49개"(`identical_except_path_49.txt`, 헤더만 다름)라 1C-3이 덮어쓰지 않았다. 그래서 BS 3개만 LootNPop 에디터를 끈 상태에서 GASP 원본으로 덮어써 해시를 맞춘다(AN-013·AN-018). 새 외부 의존은 없다.
2. **C++(`ULNPAnimInstance`, 헤더 변경 → 에디터 재시작).** 분기 함수 두 개를 둔다(AN-003). `ShouldApplyAdditiveLeans`는 이동 상태 Moving일 때 참이다(공중 포함. LootNPop에는 Sliding이 없어 이동 모드 Blend가 필요 없다). `ShouldApplyHeadLean`은 FreeAim 무기도 아니고 락온도 아닐 때 참이다. 락온은 InputCmd `FLNPModifierInputs::bIsLockOn`에서 읽어 원격·시뮬 프록시에도 맞게 한다. A/B 확인용 CVar `LNP.Anim.AdditiveLeans`를 둔다.
3. **ABP.** 노드 묶음은 GASP 텍스트를 바꿔 사용자가 붙여넣고, 연결은 MCP로 한다. 분기 바인딩은 위 두 함수, 몸통 α는 `GroundSpeed`(접평면 = GASP `Speed2D`), BS 입력은 `LeanAmount`로 잇는다. 레이어 함수는 MCP로 만들 수 없어 2-3·2-5처럼 AnimGraph 안 노드 묶음으로 둔다.
4. **위치 결정 — 무기 오버레이 뒤, 슬롯 앞.** 무기 서브 ABP 4종(권총·라이플·샷건·롱소드)의 `ApplyWeaponStyleOverlay`는 `spine_02`(깊이 2) 위를 **메시 공간 회전 블렌드**(가중치 1)로 무기 자세로 덮는다. 오버레이 앞에 두면 무기를 든 상태에서는 상체 기울기가 지워지고 골반·허리만 기운다. 뒤에 두면 맨손과 무기 모두 같게 기울고, 슬롯(사격 가산·근접·대시·재장전)은 GASP처럼 그 뒤에서 기울기를 덮는다. 기울기가 AO 뒤에 얹히므로 측면 가속 중에는 총구가 기운 만큼 조준점에서 벗어난다. GASP도 몸통 기울기가 AO와 함께 얹힌다(가산 회전 두 개, 작은 각에서는 순서 무관). 발사 방향은 총구 → 조준점으로 수렴하므로([TechDesign_HitDetection.md](../../TechDesign_HitDetection.md) §7.7) 명중은 그대로이고, 총구 위치만 몇 cm 움직인다.
5. **ActorPromoted 적 전체 회귀.** AI 입력(`SetAIOrientationIntent`) 궤적 예측, 게스트 재시뮬레이션 떨림(특히 `VelocityAcceleration` 차분에서 나오는 `LeanAmount`), `MovementState` 깜빡임, 2-4 전환 DB, 2-5 발 배치, Leans를 본다.
6. **PIE.** 가속·선회 기울기의 방향·크기, 무기별(근접·총기)·조준 상태별 상체·머리, 근접 콤보·대시·재장전·가드·랙돌 회귀, 적도·승격 적·리슨 2인 게스트.

### 2-6 결과 (2026-10-21, 상세 `history/Phase02_Log.md`)

| 항목 | 결과 |
|:---|:---|
| GASP 확인 | `leans.txt`와 GASP MCP `get_dependencies`로 확인. 요약 `design/GASPReference.md` §2.13 |
| 반입 | 16개 모두 이미 있었다. 시퀀스 13개는 5.8과 해시 동일. BS 3개(경로 외 동일 49개, 헤더만 다름)를 GASP 원본으로 덮어써 해시를 맞췄다. 해시 `Saved/AssetRefresh/2026-10-21/import_list_2-6_sha256.txt`, git 제외 확인 |
| C++ | `ShouldApplyAdditiveLeans`(이동 상태 Moving, 공중 포함)·`ShouldApplyHeadLean`(FreeAim·락온이 아님), `bIsLockOn`(InputCmd `FLNPModifierInputs`), CVar `LNP.Anim.AdditiveLeans`, 디버그 표시·`[AnimB3]` 로그에 Lean 값·적용 여부. 헤더 변경 → 에디터 종료 빌드 |
| ABP | `ApplyWeaponStyleOverlay` → **Leans 노드 묶음 19개**(붙여넣기 1회, `Scripts/AnimOverhaul/BuildLeansPaste_2-6.py`) → `Slot 'FullBodyAdditivePreAim'`. 연결 3개는 MCP. 바인딩은 C++ 이름 경로로 풀렸다(경고를 오류로 다루는 컴파일 통과) |
| 위치 근거 | 무기 서브 ABP 4종이 `spine_02` 위를 메시 공간 회전 블렌드로 덮어, 오버레이 앞이면 무기 상체 기울기가 지워진다 → 뒤. 슬롯은 기울기를 덮는다(GASP와 같음) |
| 검증(사용자 1·2인 PIE, 적도) | 맨손·롱소드·총기(ADS·이동 재장전)·락온 스트레이프·점프·활공·그래플·랙돌, 리슨 2인 게스트에서 호스트·승격 적 이동·선회·전환·발 배치 — "전체적으로 어색한 부분 없음". 자체 PIE: 총기 정지 상태 `off`(머리 기울기 꺼짐) 확인, 새 경고 0(Mover `UpDirection` 0 벡터·초기화 전 궤적 예측·롤백은 10-20 로그에도 있는 기존 노이즈) |
| 남긴 것 | 사용자 PIE 구간에 디버그 CVar가 꺼져 있어 호스트·게스트 Lean 수치 대조는 없다(육안 검증만). 자동 PIE는 플레이어가 움직이지 않으면 적이 승격되지 않아 적 로그를 못 모았다 |

**Phase 2 ActorPromoted 적 전체 회귀(2-6 항목 5).** 승격 적은 `ABP_Main_UEFN`을 공유하므로 2-1~2-6 결과를 모두 받는다. 단위마다 2인 PIE에서 승격 적을 봤고(2-1 값 로그, 2-2·2-4 DB 변경 횟수, 2-5 텔레포트 판정), 2-6에서 Leans를 더한 최종 상태를 리슨 2인 게스트 화면으로 다시 봤다. AI 입력 궤적 예측·게스트 재시뮬레이션 떨림·`MovementState` 깜빡임·전환 DB·발 배치·Leans 모두 어색함이 없었다.

## Phase 2 종료 (2026-10-21)

최종 포즈 파이프라인(`ABP_Main_UEFN`):

```
Motion Matching (Chooser 52행 → DB 51개, 블렌드 스택: Steering×2 · OrientationWarping · ResetRoot)
→ Bool(지상) / 공중 Fall_Loop 고정 (AN-007)
→ Linked Layer ApplyWeaponStyleOverlay (무기 서브 ABP: 상체 자세 · AO · 가드)
→ Additive Leans (2-6)
→ Slot FullBodyAdditivePreAim → DefaultSlot → FullBody → [UpperBody 레이어드] → AdditiveHitReact
→ DeadBlending → OffsetRootBone → CR_Biped_FootPlacement_UEFN
→ Linked Layer ApplyWeaponPostProcess → PoseHistoryCollector → Output
```

공통 설계 반영은 [TechDesign_CombatAnimation.md](../../TechDesign_CombatAnimation.md) §1이다. 엔진 노드의 구면 함정 4건(Offset Root 3건 + `MaxRotationError`)과 `CalculateDirection` 월드 Z는 같은 문서 §7.4와 `design/GASPReference.md` §2.10·§2.12에 있다.

## 완료 조건 체크리스트

- [x] 2-0a: 실사용 목록 확정(98개, 0-3 대비 차이 설명), 리타깃 사본 98개 `Retargeted/Lyra/`, 몽타주 대조 결함 0, 서브 ABP 5개 경고 0 컴파일, 재생성 절차·스크립트, 2-0b 임시 로코모션 계획 수정 (2026-10-18)
- [x] 2-0b: 전환 — 결정 A·B 반영, 근접 콤보·ANS 판정·근접 보정·재장전·가드/패링·랙돌·HitStop, 적도·`ActorPromoted`·2인 PIE, ISKM 무영향(ISKM 에셋 미변경). 알려진 차이: 스트레이프 방향 클립 → 2-2 (2026-10-19)
- [x] 2-1: B0 공급값(상태·이력·궤적 파생값·가속·지면·술어) C++, 궤적 C++ 소유, Pose History GASP 설정, DeadBlending, 디버그 CVar, 2인·적도·승격 적 값 확인, 회귀 없음 (2026-10-19)
- [x] 2-2: B1 상태별 PSD 묶음 Chooser(멀티 결과)·루프 DB 10개(GASP 복제), 방향별 스트레이프 복구(시퀀스 26개 반입), 인터럽트·블렌드 시간 C++, MM 노드 GASP 설정, 적도 이동 방향 수정, 1·2인·적도·승격 적 PIE 회귀 없음 (2026-10-19)
- [x] 2-3: B2 블렌드 스택 체인(Steering×2·OrientationWarping·ResetRoot·CopyBone)과 B3 Offset Root Bone(모드·반감기 C++, 전신 몽타주 Release, base 이동량, 구면 함정 3건 대응), AO·FacingDelta 가상 루트 기준, 적도·리슨 2인·승격 적·움직이는 발판 확인, 회귀 없음. 정지 회전은 `MaxRotationError` 90° 임시(2-4 TurnInPlace) (2026-10-20)
- [x] 2-4: B6 전환 DB 41개(Starts·Stops·Pivots·Turns·Spins·TurnInPlace·GaitTransitions·Lands, GASP 복제)와 52행 Chooser, 시퀀스 239개 반입, 출발·제자리 회전 인터럽트 예외, 착지 DB(AN-007 유지), 적도·리슨 2인·승격 적 PIE 회귀 없음. `MaxRotationError` 90° 안전망 유지 (2026-10-20)
- [x] 2-5: B4 발 배치 CR(GASP 복제·구면 수정 3곳·함수 참조 재연결)과 AnimGraph CR 노드, 착지 리셋·움직이는 base 처리, B5는 AN-021(반입 없음), 엔진 `MaxRotationError` 적도 함정 대응, 적도·리슨 2인·승격 적 PIE 회귀 없음 (2026-10-20)
- [x] 2-6: B8 Additive Leans(GASP 노드 묶음, 분기 C++ bool 2개, 무기 오버레이 뒤·슬롯 앞), Leans 소스 16개 해시 일치(BS 3개 덮어쓰기), `ActorPromoted` 적 전체 회귀, 적도·리슨 2인·승격 적 PIE 회귀 없음 (2026-10-21)
- [x] **Phase 2 완료 (2026-10-21)** — 블록마다 적도·NPC·2인 확인, 무기 레이어·슬롯·ANS 회귀 없음(`Roadmap.md` §3 완료 조건)
