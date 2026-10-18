# Phase 2 — 스켈레톤 전환과 GASP Mover ABP 재구성

> 상태: 진행 중(2-0a 완료 2026-10-18, 다음 2-0b — 결정 A·B 확정)
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

## 단위 2-0b — 전환 (계획 초안, 결정 A·B 확정 2026-10-18)

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

## 완료 조건 체크리스트

- [x] 2-0a: 실사용 목록 확정(98개, 0-3 대비 차이 설명), 리타깃 사본 98개 `Retargeted/Lyra/`, 몽타주 대조 결함 0, 서브 ABP 5개 경고 0 컴파일, 재생성 절차·스크립트, 2-0b 임시 로코모션 계획 수정 (2026-10-18)
- [ ] 2-0b: 전환 — 결정 A·B 반영, 근접 콤보·ANS 판정·근접 보정·재장전·가드/패링·랙돌·HitStop, 적도·`ActorPromoted`·2인 PIE, ISKM 무영향
- [ ] 2-1 ~ 2-6: `Roadmap.md` §4
