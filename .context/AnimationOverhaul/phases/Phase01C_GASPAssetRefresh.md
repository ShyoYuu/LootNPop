# Phase 1C — GASP 소스 에셋 최신화

> 상태: 초안(착수 직전에 단위별 세부를 다시 확인)
> 선행: Phase 0 완료(MCP 읽기 방법, 폴더·git 규칙 AN-016, 노티파이·모디파이어 처리 AN-017, 스켈레톤 기준 AN-011)
> 후행: Phase 2(2-0a Lyra 세트 UEFN 리타깃부터)는 이 Phase가 끝난 뒤 최신 시퀀스·스켈레톤으로 시작한다
> 순서: 단위 1C-1 → 1C-2 → 1C-3 → 1C-4. 단위마다 커밋하고 사용자 확인을 받는다.

## 배경

`Content/Characters/GASP_Mannequin/`(MotionMatchingData 제외)은 예전 GASP 버전에서 가져온 소스 에셋이다. 사용자 결정으로 **중복 에셋이라도 GASP 5.8 최신본으로 바꾼다**(AN-012).

2026-10-18 조사(`research/UE58_AnimationFeatures.md` §6):

| 구분 | 수 | 처리 |
|:---|---:|:---|
| 최신 GASP `Characters/UEFN_Mannequin/`과 상대 경로가 같은 에셋 | 411 | 최신본으로 교체. 1C-1 결과 실제 덮어쓰기는 332개(양쪽 리다이렉터 30·경로 외 동일 49 제외) |
| 최신 GASP에 없는 에셋 — `Animations/Enemy/*` 14개(LootNPop이 만든 리타깃 적 애니, 5.7MB), 그리고 출처를 판별해야 하는 4개(`Animations/ABP_UEFN`, `Poses/M_Pose_01`, `Run/M_Relaxed_Run_Loop_F_Troy`, `Meshes/SM_UEFN_Mannequin`: 옛 GASP에만 있던 것인지 LootNPop 산출물인지) | 18 | 보존(AN-014). 4개는 1C-1에서 출처 판별 |
| 최신 GASP에만 있는 에셋 | 약 1,950 | 일괄 반입하지 않는다. Phase 2 블록이 요구할 때만 가져온다(AN-001) |
| `MotionMatchingData/`(LootNPop 구성 에셋, git 추적) | 7 | 교체 대상 아님. 1C-2에서 `CHT_PSD_UEFN`은 삭제, 6개는 `Content/Characters/MotionMatchingData/`로 이동. 참조 유지 확인과 PSD 재빌드는 1C-4 |

**누가 이 에셋을 쓰는가.** 플레이어는 Lyra 스켈레톤(`ABP_Lyra`, `PSD_*_Lyra`)이다. UEFN 스켈레톤·메시는 **적**이 쓴다: `BP_LNPEnemy`, `DA_EnemyEntityConfig_PureEntity_Melee01/Ranged01`, `ASTP_Enemy_UEFN_Sword_Body/Shotgun_Body`, 무기 `SKM_LongSword_UEFN`·`SKM_Shotgun_UEFN`, 리타기터 `RTG_GASP_to_MS`·`RTG_Lyra_to_GASP`·`RTG_Lyra_to_GASP_noRootMotion`. 그래서 이 Phase의 회귀 위험은 대부분 적(특히 ISKM 순수 엔티티)에 있다.

**AN-011 이후.** Phase 2 단위 2-0b부터 플레이어·`ActorPromoted` 적의 `AnimSourceMesh`도 `SKM_UEFN_Mannequin`이 되고, Lyra 무기·몽타주 애니는 이 Phase가 확정한 UEFN 스켈레톤으로 리타깃된다(2-0a). 그래서 1C-1의 스켈레톤 차이 확인은 적뿐 아니라 이후 모든 캐릭터의 기준을 정하는 일이다. 1C가 끝난 뒤 스켈레톤을 다시 바꾸면 리타깃 사본을 다시 만들어야 한다.

## 핵심 방식 — 경로를 GASP 원본에 맞춘 뒤 파일로 덮어쓴다 (AN-013, 확정)

언리얼 에셋은 패키지 경로로 서로를 참조한다. GASP 최신본은 `/Game/Characters/UEFN_Mannequin/...`을 참조하고, LootNPop의 기존 본은 `/Game/Characters/GASP_Mannequin/...`에 있다. 그대로 복사하면 스켈레톤이 두 벌이 되고, 411쌍을 하나씩 Replace References로 통합해야 한다(MCP에 해당 도구가 없어 수작업이 된다).

대신:

1. LootNPop 에디터에서 폴더 `/Game/Characters/GASP_Mannequin/` → `/Game/Characters/UEFN_Mannequin/`으로 **이동**한다(MCP `AssetTools.move`). 참조자(적 BP·DA·ASTP·리타기터·PSD)가 새 경로로 갱신된다.
2. 경로가 GASP와 같아졌으므로, 에디터를 끈 상태에서 교체 목록(1C-1 결과 332개)을 GASP 최신본으로 **덮어쓴다**. 내부 참조가 그대로 맞는다.
3. 앞으로 GASP를 다시 업데이트할 때도 같은 덮어쓰기로 끝난다.

대가: 추적되는 참조 에셋이 한 번 다시 저장되고, `.gitignore` 패턴과 문서 경로를 고쳐야 한다. 제외 대상인 GASP 폴더 이름이 GASP 원본과 같아진다.

## 반입 목록과 경로 규칙 (AN-016·AN-017)

- 반입은 GASP 패키지 경로 그대로 한다. 대상은 `Characters/UEFN_Mannequin/`의 필요분, 시퀀스가 참조하는 `Misc/SandboxAnimCurveCompressionSettings`와 `Blueprints/AnimModifiers/AM_*`(엔진 의존만)이다. GASP 경로 안의 GASP 구성 에셋(PSD·PSS·Chooser·ABP)은 반입하지 않는다.
- Foley 노티파이 11종(`BP_AnimNotify_FoleyEvent_*` → `/Game/Audio/Foley/*`), `BP_NotifyState_EarlyTransition`(→ `BPI_SandboxCharacter_ABP`·CMC ABP), `BP_NotifyState_MontageBlendOut`(Traversal 몽타주, → `BPI_SandboxCharacter_Pawn` 등)은 반입하지 않는다. `Config/DefaultEngine.ini` `[CoreRedirects]`로 처리한다. 1C-1 실험 결과(`history/Phase01C_Log.md` §3): **BP 패키지마다 `PackageRedirects=(OldName="<패키지>",Removed=true)`가 반드시 필요하다**(없으면 에디터 Load Errors "dependent package ... was not available"). 여기에 `ClassRedirects=(OldName="<패키지>.<클래스>_C",NewName="/Script/LootNPop.LNPAnimNotify_GASPPlaceholder",OverrideClassName="/Script/CoreUObject.Class")`를 더하면 노티파이 인스턴스가 no-op 네이티브 클래스로 살아남고, 빼면 클래스 없는 빈 이벤트가 된다. 두 방식 모두 경고 0이다. **사용자 결정(2026-10-18, AN-017): `PackageRedirects ... Removed=true`만 쓴다(C++ 없음)** — 노티파이는 클래스 없는 빈 이벤트로 남는다. 발소리가 필요해지면 no-op C++ 클래스 + `ClassRedirects`(실험 E)로 바꿀 수 있다.
- 시퀀스가 하드 참조하는 GASP 구성 에셋(`AnimNotifyState_PoseSearchBranchIn.Database` → `PSD_SM_CMC_*`, Traversal 몽타주 → `PSD_Traversal`)도 반입하지 않고 같은 `PackageRedirects ... Removed=true`로 끊는다(실험에서 `PSD_SM_CMC_Idles` 경고 0, 필드는 null).
- 소스 파일은 수정하지 않는다(노티파이 제거·재저장 금지). 다음 GASP 업데이트도 같은 덮어쓰기로 끝나야 한다.
- 아래 **경로별 반입 목록**이 `README.md` §4 "새로 클론한 환경에서 소스 에셋 확보"의 원본이다.
- LootNPop 구성 에셋은 GASP 경로 폴더 안에 두지 않는다. UEFN MM 구성 에셋은 `Content/Characters/MotionMatchingData/`(git 추적)에 둔다(1C-2에서 이동).
- LootNPop이 만든 리타깃 산출물은 git에서 제외하고 `Content/Characters/Retargeted/`에 둔다. 재생성 절차(리타기터·대상 목록)를 만든 단위가 남긴다.

### 경로별 반입 목록 (1C-1 확정, 2026-10-18)

경로는 `Content/` 기준이고, GASP 5.8과 LootNPop에서 같은 상대 경로다. 파일 단위 목록은 `Saved/AssetRefresh/2026-10-18/import_list_1C3.txt`·`import_list_Phase2_MM.txt`(git 제외, 재생성은 로그 §1·§2 방법).

| 경로 | 1C-3 | Phase 2(2-2) | 비고 |
|:---|---:|---:|:---|
| `Characters/UEFN_Mannequin/` 교체(1C-2 이동 뒤 덮어쓰기) | 332 | — | 데이터 변경 290 + 커브 압축 참조만 변경 42. 경로 외 동일 49는 생략 가능. **리다이렉터 30개는 복사 금지**(1C-2에서 정리) |
| `Characters/UEFN_Mannequin/Animations/Traversal/**` 시퀀스(신규) | 34 | — | 교체되는 Traversal 몽타주 34개의 하드 의존. **사용자 결정(2026-10-18): Traversal 몽타주 34개는 5.8로 교체한다** — 신규 시퀀스 34개를 반입하고 `PSD_Traversal`·`BP_NotifyState_MontageBlendOut`은 아래 리다이렉트로 끊는다 |
| `Characters/UEFN_Mannequin/Rigs/PM_UEFN_Mannequin`(신규) | 1 | — | 새 `PA_UEFN_Mannequin`이 참조 |
| `Characters/UEFN_Mannequin/Animations/**` MM 경로 시퀀스(신규) | — | 392 중 필요분 | `CHT_PoseSearchDatabases_Relaxed` 경로. Walk 114·Crouch 108·Run 103·Jump 22·Sprint 21·Traversal 14·Slide 6·Idle 4. 추가 내부 의존 없음(스켈레톤만) |
| `Misc/SandboxAnimCurveCompressionSettings` | 1 | (같은 파일) | 1C-3 대상 225개, Phase 2 대상 360개가 참조 |
| `Blueprints/AnimModifiers/AM_BakePhaseCurveFromFootstepNotifies` | 1 | (같은 파일) | Phase 2 시퀀스 4개가 참조. **폴더 전체가 아니라 이 파일만** — 다른 `AM_FootSteps_*`는 Foley 노티파이를 참조한다 |
| 반입 금지 → 리다이렉트 | — | — | 아래 표 |

리다이렉트 대상(`Saved/AssetRefresh/2026-10-18/redirect_targets.txt`):

| 종류 | 패키지(`/Game/...`) | 필요 시점 |
|:---|:---|:---|
| Foley 노티파이(`UAnimNotify`) 11종 | `Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_` + `Walk_L`·`Walk_R`·`Run_L`·`Run_R`·`Handplant_L`·`Handplant_R`·`Jump`·`Land`·`Roll`·`Scuff_L`·`Scuff_R` | 1C-3(11종 모두). Phase 2 MM 시퀀스는 이 중 Walk·Run·Handplant·Jump만 쓴다 |
| 노티파이 스테이트(`UAnimNotifyState`) | `Blueprints/AnimNotifies/BP_NotifyState_EarlyTransition` | 1C-3(시퀀스 1개), Phase 2(12개) |
| 노티파이 스테이트(`UAnimNotifyState`) | `Blueprints/AnimNotifies/BP_NotifyState_MontageBlendOut` | 1C-3(Traversal 몽타주 34개 — 교체 확정) |
| GASP PSD(패키지 Removed만) | `Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/PSD_SM_CMC_Idles`·`PSD_SM_CMC_Transitions`, `Characters/UEFN_Mannequin/Animations/MotionMatchingData/Databases/PSD_Traversal` | 1C-3 |
| GASP PSD(패키지 Removed만) | `.../ExperimentalStateMachineData/PSD_SM_CMC_Loops` | Phase 2 |

⚠️ `Removed`로 끊은 GASP PSD 경로는 나중에 같은 경로로 반입해도 로드되지 않는다. GASP PSD는 AN-016에 따라 반입하지 않으므로 충돌은 없다.

## 단위 1C-1 — 대조표와 백업

1. 411개를 **바이트 단위로 비교**(해시)해 실제로 바뀐 파일만 교체 목록에 남긴다.
2. 교체 목록과 Phase 2가 쓸 MM 경로 시퀀스의 **최신본 의존성**을 GASP MCP `get_dependencies`로 따라간다. 0-3 조사로 알려진 외부 의존은 커브 압축 설정, `AM_BakePhaseCurveFromFootstepNotifies`, Foley 10종, EarlyTransition이다(`design/GASPReference.md` §2.7). 위 "반입 목록과 경로 규칙"에 따라 반입 목록과 리다이렉트 대상 클래스 목록을 확정하고, 리다이렉트 실험을 한다. 놓치면 교체 뒤 참조가 깨진다.
3. 스켈레톤·메시 변화를 확인한다(본 목록·가상 본·소켓·커브·바인드 포즈). 바인드 포즈가 바뀌면 ISKM이 영향을 받는다.
4. 교체 전 원본 전체를 `Saved/AssetRefresh/<날짜>/`에 백업하고 해시 목록을 남긴다.
5. 출처 미판별 4개를 참조자와 생성 이력으로 판별한다. 아무도 참조하지 않는 옛 GASP 에셋은 삭제 후보로 올린다(삭제는 사용자 확인 후). `ABP_UEFN`은 `CHT_PSD_UEFN`만 참조하는 옛 UEFN 모션매칭 실험으로 보인다(0-3).
6. LootNPop 산출물(Enemy 14개 + 판별 결과)의 이동 대상을 확정한다. AN-014가 확정됐으므로 `Content/Characters/Retargeted/`로 옮기고 git에서는 계속 제외한다. 14개의 재생성 절차(`RTG_Lyra_to_GASP_noRootMotion`, 원본 Lyra 시퀀스 목록)를 남긴다.

완료 조건: 교체·보존·신규 대조표, 경로별 반입 목록·리다이렉트 대상 목록, 리다이렉트 실험 결과, 스켈레톤·메시 차이 보고, 백업, 출처 판별, LootNPop 산출물 이동 목록·재생성 절차.

### 1C-1 결과 요약 (2026-10-18, 상세 `history/Phase01C_Log.md`)

산출물 위치 `Saved/AssetRefresh/2026-10-18/`(git 제외):

| 파일 | 내용 |
|:---|:---|
| `table_replace.tsv` | 교체 대상 381개(리다이렉터 제외)의 클래스·차이 분류·필요 여부. 덮어쓰기 필요 332 |
| `table_redirectors.tsv` | 양쪽 모두 리다이렉터인 30개 |
| `keep_only_lootnpop.txt` | 보존 18개(LootNPop에만 있음) |
| `new_only_gasp58.txt` | 최신 GASP에만 있는 1,952개(일괄 반입 안 함) |
| `compare_common.tsv`·`compare_classified.tsv` | 411쌍 SHA-256과 본문·이름 맵 비교 원자료 |
| `import_list_1C3.txt`·`import_list_Phase2_MM.txt`·`redirect_targets.txt` | 위 "경로별 반입 목록"의 파일 단위 목록 |
| `retargeted_move_list.tsv` | `Retargeted/` 이동 15개와 원본 시퀀스 |
| `backup/`·`backup_sha256.txt` | 교체 전 `GASP_Mannequin/` 전체 436개 백업과 해시 |
| `redirect_experiment/` | 실험 설정·스크립트·요약 로그(랩 프로젝트 `Saved/AssetRefresh/RedirectLab/`) |

- **대조:** 411쌍 해시가 전부 다르다(헤더에 엔진 버전·경로가 들어감). 본문 비교로 데이터 변경 290, 참조만 변경 42, 경로 외 동일 49, 양쪽 리다이렉터 30.
- **스켈레톤:** 본 88 → 93(끝에 `props_root`·`prop_01`·`poi`·가상 본 2개 추가, 기존 인덱스 보존). **바인드 포즈 변화 없음**(쿼터니언 차 < 1e-6). 소켓 3개·커브 메타데이터 4개 추가. 메시는 다시 임포트돼 정점 수가 늘었고(LOD0 6,009 → 7,438), **포스트프로세스 ABP `ABP_UEFN_Mannequin_PostProcess`(머리 1.15배·허벅지 1.12배 스케일)가 새로 연결됐다.**
- **출처 판별:** `ABP_UEFN`·`SM_UEFN_Mannequin`은 LootNPop 산출물, `M_Pose_01`·`M_Relaxed_Run_Loop_F_Troy`는 옛 GASP 원본. 사용자 결정(2026-10-18)으로 `M_Pose_01`과 `ABP_UEFN`+`CHT_PSD_UEFN`은 1C-2에서 삭제했다(백업에 있음). `Troy`는 2-2에서 `PSD_Run_UEFN`을 교체할 때까지 유지한다.

### LootNPop 산출물 이동 목록과 재생성 절차 (1C-2·AN-016 ③)

1C-2에서 `AssetTools.move`로 옮겼다(2026-10-18). 목록 원본은 `retargeted_move_list.tsv`.

| 옮긴 것(옛 `/Game/Characters/GASP_Mannequin/`) | 새 위치(`/Game/Characters/Retargeted/`) | 원본 |
|:---|:---|:---|
| `Animations/Enemy/A_SW_{Attack_01,Attack_02,Damage_Backward,Damage_Fast,Damage_Left,Damage_Right,Idle,RelaxWalk,Run}_UEFN` (9) | `Enemy/` 같은 이름 | `/Game/Characters/Lyra_Mannequin/Animations/Sword/A_SW_*`(이름에서 `_UEFN` 제거) |
| `Animations/Enemy/MM_Death_Front_01_UEFN`, `MM_Shotgun_Fire_UEFN` | `Enemy/` | `Lyra_Mannequin/Animations/Actions/` |
| `Animations/Enemy/MM_Rifle_Jog_Fwd_UEFN`, `MM_Rifle_Walk_Fwd_UEFN` | `Enemy/` | `Lyra_Mannequin/Animations/Locomotion/Rifle/` |
| `Animations/Enemy/MM_Shotgun_Idle_ADS_UEFN` | `Enemy/` | `Lyra_Mannequin/Animations/Locomotion/Shotgun/` |
| `Meshes/SM_UEFN_Mannequin` | `Meshes/` | `SKM_UEFN_Mannequin`에서 에디터로 만든 StaticMesh(임포트 정보 없음) |

재생성(새로 클론한 환경 또는 스켈레톤 교체 뒤):

1. 1C-3 이후 기준 스켈레톤(`/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin`, 93본)이 들어와 있는지 확인한다.
2. IK Retargeter 배치 리타깃으로 위 원본 14개를 `RTG_Lyra_to_GASP_noRootMotion`(대상 메시 `SKM_UEFN_Mannequin`)으로 리타깃하고, 접미어 `_UEFN`을 붙여 `Retargeted/Enemy/`에 저장한다. 이 리타기터는 Pelvis Motion `Scale Horizontal`=0이다. ISKM은 `root` 변위를 버리고 pelvis 변위는 남기므로 제자리 클립이 된다([TechDesign_EnemyNPC_LowLOD.md](../../TechDesign_EnemyNPC_LowLOD.md) §6.7). 지금 사본 중 이 리타기터로 만든 것이 확인된 것은 `MM_Rifle_Jog_Fwd_UEFN`뿐이다. 다른 클립의 좌우 골반 흔들림이 사라지는지 1C-4에서 본다.
3. `MM_Shotgun_Fire_UEFN`은 리타깃 뒤 `AdditiveAnimType`을 `AAT_None`으로 바꾸고 썸네일에 전신 포즈가 나오는지 본다(같은 문서 §6.7).
4. `SM_UEFN_Mannequin`은 `SKM_UEFN_Mannequin`에서 다시 만든다(스켈레탈 메시 에디터 Make Static Mesh, LOD0). 머티리얼은 `M_UEFN_Mannequin`.
5. `ASTP_Enemy_UEFN_*`·`ASL_Enemy_UEFN_*`가 새 시퀀스를 가리키는지, ISKM에서 재생되는지 확인한다(1C-4 1번 항목과 같다).

## 단위 1C-2 — 경로 정렬 (에디터 작업, 커밋 단위)

1. AN-014에 따라 LootNPop 산출물을 먼저 옮긴다(`AssetTools.move`).
2. `GASP_Mannequin` → `UEFN_Mannequin` 폴더 이동, 리다이렉터 정리.
3. `MotionMatchingData/` 7개를 LootNPop 소유 폴더로 옮긴다(AN-016). `.gitignore`: `Content/Characters/GASP_Mannequin/*`·예외 줄을 지우고 GASP 원본 경로 단위 패턴(`Content/Characters/UEFN_Mannequin/`, `Content/Misc/SandboxAnimCurveCompressionSettings.uasset`, `Content/Blueprints/AnimModifiers/`)과 `Content/Characters/Retargeted/`를 넣는다. `Content/Blueprints/` 전체는 제외하지 않는다(LootNPop이 나중에 쓸 수 있다).
4. 참조 무결성 확인: 이동 전후 참조자 목록 비교, 로드 경고 0.
5. 추적 파일 변경을 커밋한다(경로만 바뀌고 동작은 같아야 한다). PIE에서 적 표시·애니가 그대로인지 확인한다.

완료 조건: 새 경로에서 기존과 같은 동작, 리다이렉터 없음, 커밋.

### 1C-2 결과 (2026-10-18, 상세 `history/Phase01C_Log.md`)

| 경로 | 내용 | git |
|:---|:---|:---|
| `Content/Characters/UEFN_Mannequin/` | 옛 `GASP_Mannequin/`의 GASP 소스 388개(리다이렉터 30·삭제 3·이동 15·MM 6 제외) | 제외 |
| `Content/Characters/Retargeted/{Enemy,Meshes}/` | Enemy 리타깃 애니 14개, `SM_UEFN_Mannequin` | 제외 |
| `Content/Characters/MotionMatchingData/` | `PSD_{Idle,Jump,Run,Walk}_UEFN`, `PSN_`·`PSS_CoreLocomotion_UEFN` | 추적 |
| 삭제 | `Animations/Poses/M_Pose_01`, `Animations/ABP_UEFN`, `MotionMatchingData/CHT_PSD_UEFN` | 백업에 있음 |

- 다시 저장된 추적 참조자: `BP_LNPEnemy`, 적 DA 3개, `ASTP_Enemy_UEFN_*` 4개, `ASL_Enemy_UEFN_*` 2개, `RTG_GASP_to_MS`·`RTG_Lyra_to_GASP`·`RTG_Lyra_to_GASP_noRootMotion`, `SKM_LongSword_UEFN`. `SKM_Shotgun_UEFN`(git 제외)도 다시 저장됐다.
- 기존 GASP 소스 6개(`IK_`·`PA_UEFN_Mannequin`, `ABP_UEFN_Mannequin_PostProcess`, 시퀀스 3개)에 원래부터 있던 깨진 소프트 참조 `/Game/Characters/GASP_Mannequin/GASP_Mannequin/Meshes/SKM_UEFN_Mannequin`(경로 이중)이 남아 있다. 이동이 만든 것이 아니다(백업에도 있다). 6개 모두 1C-3 덮어쓰기 대상이니 1C-3에서 다시 확인한다.

## 단위 1C-3 — 최신본 덮어쓰기

1. `[CoreRedirects]`에 위 "리다이렉트 대상" 1C-3 행의 `PackageRedirects ... Removed=true`를 넣는다(BP 노티파이 13종 + GASP PSD 3개, Traversal 포함). C++·빌드는 없다(AN-017).
2. LootNPop 에디터를 끈다. `import_list_1C3.txt`의 파일(교체 332 + 신규 35(Traversal 시퀀스 34 + `PM_UEFN_Mannequin`) + 외부 2)을 GASP에서 같은 경로로 복사한다. 리다이렉터 30개는 복사하지 않는다(1C-2에서 정리됨). GASP 파일은 읽기만 한다(AN-009).
3. 에디터를 다시 켜고 로드 경고·깨진 참조를 확인한다. Foley·EarlyTransition 누락 클래스 경고가 없어야 한다.
4. MCP로 교체 에셋의 클래스·의존성을 다시 조회해 대조표와 맞는지 확인한다.

완료 조건: 깨진 참조 0, 로드 경고 0(또는 원인과 대응 기록).

### 1C-3 결과 (2026-10-18, 상세 `history/Phase01C_Log.md`)

- `Config/DefaultEngine.ini` `[CoreRedirects]`에 `PackageRedirects ... Removed=true` 16줄(BP 노티파이 13 + GASP PSD 3)을 넣었다. `redirect_targets.txt`의 `1C3` 행과 일치한다.
- 369개(A 332 + B 35 + C 2)를 GASP에서 복사했다. SHA-256 369/369가 GASP 원본과 일치한다(`post1C3_sha256.txt`). 덮어쓰기 전 332개는 `backup_pre1C3/`에 있다(1C-2 상태로 되돌릴 때 쓴다).
- 재시작 로그와 369개 + PSD 6 MCP 로드: 새 경고·LoadErrors 0, 로드 실패 0, 더티 0. `PSD_{Idle,Jump,Walk,Run}_UEFN` BuildIndex 성공.
- 의존성 조회(482개)에서 존재하지 않는 `/Game/` 의존은 Removed 16개 패키지와 기존 누락 `AN_PlayWeaponMontage`(Lyra, 무기 몽타주 3)뿐이다. 1C-2에서 남아 있던 이중 경로 소프트 참조 `GASP_Mannequin/GASP_Mannequin/...`은 덮어쓰기로 사라졌다(디스크 grep 0).

## 단위 1C-4 — 회귀 검증

1. **ISKM 순수 엔티티 적:** `ASTP_Enemy_UEFN_*` 재베이크가 필요한지, ISKM 거리에서 몸통·무기 메시가 보이고 애니가 도는지 확인한다(바인드 포즈 함정, [TechDesign_EnemyNPC_LowLOD.md](../../TechDesign_EnemyNPC_LowLOD.md) §6.7).
   - **1C-3 뒤 확인된 회귀(2026-10-18 사용자 PIE):** ISKM 적의 무기는 정상이지만, 몸통이 T 포즈로 굳고 애니가 재생되지 않는다. Actor 적은 정상이다. 원인은 LootNPop이 옛 GASP 소스에 직접 넣었던 설정 2건이 덮어쓰기로 사라진 것이다. ① `SKM_UEFN_Mannequin` 빌드 세팅 `bOptimizeForInstancing`(전 LOD) — PIE 로그 `AnimSequenceTransformProvider: ... requires 'Optimize for Instancing'` ② `M_UEFN_Mannequin`의 `bUsedWithInstancedStaticMeshes` — 로그 `missing usage flag InstancedStaticMeshes`, 먼 ISM 적이 기본 머티리얼로 그려진다. `bUsedWithInstancedSkinnedMesh`는 5.8 원본에도 있다. 이름 맵 차분상 사라진 LootNPop 수정은 이 2건뿐이다(나머지는 GASP 쪽 변경)
   - **제안(사용자 확인 필요):** 두 설정을 소스에 다시 적용해 저장하고, "반입 후처리 2건"으로 이 문서와 `README.md` §4에 남겨 다음 GASP 업데이트 때도 반복한다. 그 뒤 PIE로 몸통 애니를 확인하고, `ASTP_Enemy_UEFN_*`·`SM_UEFN_Mannequin`을 다시 구워야 하는지(새 메시 정점 수 변화) 판단한다. 다른 안은 `Retargeted/`에 SKM·머티리얼 사본을 만들고 참조를 옮기는 것이다(소스 원본 유지, 추적 에셋 재저장 필요)
2. **리타깃 적 애니(LootNPop 산출물):** 새 스켈레톤에서 그대로 맞는지, 다시 리타깃해야 하는지 확인한다.
3. **`ActorPromoted` 적:** `BP_LNPEnemy` 승격 후 로코모션·공격·랙돌. 새 `SKM_UEFN_Mannequin`의 포스트프로세스 ABP(머리 1.15배·허벅지 1.12배 스케일, 1C-1 §4)를 받아들일지, 컴포넌트 `bDisablePostProcessBlueprint`로 끌지 정한다(소스 메시는 수정하지 않는다).
4. **PSD:** `Characters/MotionMatchingData/PSD_*_UEFN` 재빌드, 경고 확인.
5. 적도 근처 PIE, 리슨 2인 PIE.
6. 문서: 바뀐 경로를 관련 TechDesign 문서에 반영한다.

완료 조건: 위 항목 통과, `Current.md`·`Roadmap.md` 갱신.

## 완료 조건 체크리스트

- [x] 1C-1: 대조표·반입/리다이렉트 목록·리다이렉트 실험·스켈레톤 차이·백업·산출물 이동 목록 (2026-10-18)
- [x] 1C-2: 경로 정렬, `.gitignore`, 커밋 (2026-10-18, 사용자 PIE에서 ISKM·Actor 적 회귀 없음 확인)
- [x] 1C-3: 최신본 덮어쓰기, 깨진 참조 0 (2026-10-18, 369개 해시 일치, 로드 경고 0)
- [ ] 1C-4: ISKM·리타깃 애니·승격 적·PSD·2인 PIE 회귀 통과
