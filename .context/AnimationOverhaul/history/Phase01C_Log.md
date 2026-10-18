# Phase 1C 작업 로그

> 소유 정보: Phase 1C(GASP 소스 에셋 최신화)의 날짜별 수행 내용·검증 결과·시행착오. 현재 유효한 반입 목록과 단위 계획은 `phases/Phase01C_GASPAssetRefresh.md`가 소유한다.
> 큰 목록·원시 데이터는 저장소 밖 `Saved/AssetRefresh/2026-10-18/`(git 제외)에 있다. 이 로그에는 요약과 파일 이름만 둔다.

## 2026-10-18 — 단위 1C-1 대조표와 백업

읽기·백업·실험만 했다. LootNPop 에셋·C++·Config, GASP 에셋은 바꾸지 않았다. GASP 에디터(8001)에서는 읽기 툴만 썼고 `read_graph_dsl`은 쓰지 않았다(조회 뒤 `ABP_UEFN_Mannequin_PostProcess` `is_dirty=false`).

### 1. 해시 대조 (411개)

상대 경로가 같은 411쌍의 SHA-256이 **전부 다르다.** 저장 엔진 버전·패키지 경로·저장 해시가 헤더에 들어가므로 바이트 비교만으로는 판정이 안 된다. 그래서 uasset 헤더(이름 맵·임포트 맵)를 직접 읽어 본문(`TotalHeaderSize` 이후)과 하드 참조를 따로 비교했다.

| 분류 | 수 | 뜻 | 1C-3 처리 |
|:---|---:|:---|:---|
| 데이터 변경 | 290 | 본문이 다르다(시퀀스 275·몽타주 45 포함 클래스별 수는 `table_replace.tsv`) | 덮어쓴다 |
| 참조만 변경 | 42 | 본문은 같고 커브 압축 설정 참조만 `ACLAnimCurveCompressionSettings` → `SandboxAnimCurveCompressionSettings`로 바뀌었다 | 덮어쓴다 |
| 경로 외 동일 | 49 | 본문·이름·하드 참조가 패키지 경로만 빼고 같다 | 생략 가능(덮어써도 무해) |
| 리다이렉터 | 30 | 옛 본과 5.8 모두 `ObjectRedirector`다. 포즈 29개가 `Animations/Poses/`로 옮겨진 흔적과 `CHT_TraversalAnims_PoseMatch` → `CHT_TraversalMontages_CMC` | **덮어쓰지 않는다.** 1C-2 리다이렉터 정리 대상 |

→ 교체 목록은 **332개**(290 + 42). 저장 엔진은 옛 본이 전부 5.7(일부 5.8), 5.8 본은 Fortnite 41.00·5.8·5.7 등이 섞여 있다.

주요 데이터 변화(이름 맵 차분 기준): 191개 시퀀스의 커브 압축이 ACL → GASP `SandboxAnimCurveCompressionSettings`, 135개가 Control Rig 참조 직렬화 형식 변경(`ControlRigClass` → `ControlRigAssetReference`), 97개에 Foley BP 노티파이 추가, 34개 Traversal 몽타주에 `BP_NotifyState_MontageBlendOut`·`PSD_Traversal` 추가, 28개에서 `AnimNotifyState_PoseSearchBranchIn` 제거, 커브 `movedata_speed`·`enable_strafewarping` 추가.

### 2. 의존성 추적과 반입 목록

- GASP MCP로 `CHT_PoseSearchDatabases_Relaxed` → PSD 71개 → 시퀀스 460개(MM 경로)를 `get_dependencies`로 따라갔다(원시 결과 `raw/mm_deps_gasp_mcp.json`). 그중 68개는 LootNPop에 이미 있고 **392개가 신규**다.
- MCP `get_dependencies`는 하드·소프트를 구분하지 않으므로, 로드 경고를 내는 **하드 참조**는 uasset 임포트 맵(FObjectImport 40바이트)을 직접 파싱해 확정했다(`raw/hard_closure.json`, `raw/import_stages.json`).
- **0-3 조사에 없던 의존이 나왔다.**
  - 시퀀스의 `AnimNotifyState_PoseSearchBranchIn.Database`가 GASP 구성 에셋 `PSD_SM_CMC_Idles`·`PSD_SM_CMC_Loops`·`PSD_SM_CMC_Transitions`를 하드 참조한다(1C-3 대상 14개, Phase 2 대상 72개). 옛 본은 이 필드가 비어 있었다.
  - Traversal 몽타주 34개가 `PSD_Traversal`, `BP_NotifyState_MontageBlendOut`(→ `BPI_SandboxCharacter_Pawn` 등), LootNPop에 없는 Traversal 시퀀스 34개를 하드 참조한다.
  - 새 `PA_UEFN_Mannequin`이 새 에셋 `Rigs/PM_UEFN_Mannequin`(PhysicalMaterial)을 참조한다.
  - 새 `SKM_UEFN_Mannequin`이 `ABP_UEFN_Mannequin_PostProcess`(포스트프로세스 ABP)와 `Character_LodSettings`를 참조한다. 옛 본은 둘 다 비어 있었다(§4).
  - Foley 부모 `BP_AnimNotify_FoleyEvent`는 시퀀스가 직접 참조하지 않는다. 자식 11종만 참조한다.
  - `Blueprints/AnimModifiers/`는 폴더 전체가 아니라 `AM_BakePhaseCurveFromFootstepNotifies` 하나만 필요하다. 다른 `AM_FootSteps_*`는 Foley 노티파이를 끌고 온다.
- 결과는 Phase 문서 "경로별 반입 목록"과 `import_list_1C3.txt`(369개)·`import_list_Phase2_MM.txt`(392개)·`redirect_targets.txt`(BP 노티파이 13종, GASP PSD 4개).

### 3. 리다이렉트 실험 (AN-017)

LootNPop 에디터·바이너리를 건드리지 않도록 **격리된 임시 C++ 프로젝트**(`Saved/AssetRefresh/RedirectLab/`, git 제외)를 만들었다. no-op `ULNPAnimNotify_GASPPlaceholder`(UAnimNotify)·`ULNPAnimNotifyState_GASPPlaceholder`(UAnimNotifyState)를 넣고, GASP 원본 사본 4개(`M_Relaxed_Run_Pivot_B_F_Lfoot` — Foley Run_L/R 12개 + EarlyTransition 1개, `M_Neutral_Stand_Idle_Loop` — `PSD_SM_CMC_Idles` 참조, 스켈레톤, 커브 압축 설정)만 두었다. Python으로 시퀀스를 로드해 노티파이 인스턴스 클래스를 찍고 로그를 셌다. 원시 로그는 `redirect_experiment/`.

| 실행 | 설정 | 노티파이 13개 | 경고·LoadErrors |
|:---|:---|:---|:---|
| A 커맨드렛 | 리다이렉트 없음 | 전부 null | 0 (Verbose "Failed to resolve import"만 14줄) |
| B 커맨드렛 | `ClassRedirects`만 | 전부 null — 리다이렉트는 적용되지만 임포트 클래스 타입이 `BlueprintGeneratedClass`로 남아 못 찾는다 | 0 (Verbose 14줄) |
| C 커맨드렛 | `ClassRedirects` + `OverrideClassName="/Script/CoreUObject.Class"` | **13개 모두 자리표시자 클래스로 풀림** | 0 |
| D 에디터 모드 | C와 같음 | 13개 풀림 | **LoadErrors 4건**("dependent package ... was not available" — BP 패키지 3 + `PSD_SM_CMC_Idles`) |
| E 에디터 모드 | C + `PackageRedirects=(OldName=...,Removed=true)` (BP 3 + PSD 1) | 13개 풀림 | **0** |
| F 에디터 모드 | `PackageRedirects Removed`만 (C++·ClassRedirects 없음) | 전부 null | **0** |

결론:

- **커맨드렛은 누락 의존을 Verbose로만 남긴다.** 경고 여부는 에디터 모드(`UnrealEditor.exe -ExecutePythonScript`)로 봐야 한다. 에디터의 Load Errors 창이 여기서 나온다.
- 클래스 리다이렉트만으로는 경고가 안 사라진다. 시퀀스가 BP **패키지** 자체에 의존하므로 `PackageRedirects ... Removed=true`가 반드시 같이 필요하다. 같은 방법으로 GASP PSD 하드 참조(`BranchIn.Database`)도 경고 없이 null이 된다.
- BP → 네이티브 클래스 리다이렉트는 `OverrideClassName="/Script/CoreUObject.Class"`가 있어야 풀린다(엔진 `BaseEngine.ini`의 `AnimNotify_PlaySound_C` 항목과 같은 형식).
- **C++ 없이도(F) 경고 0을 만족한다.** 차이는 노티파이 이벤트가 자리표시자 인스턴스로 남느냐(E), 클래스 없는 빈 이벤트로 남느냐(F)다. F에서도 시퀀스 자체·BlockTransition·BranchIn 같은 엔진 노티파이는 그대로다. 소스 파일은 어느 쪽이든 수정하지 않는다.
- 자리표시자 클래스에 없는 BP 프로퍼티(`Event` 태그·`VolumeMultiplier` 등)는 경고 없이 버려졌다. 발소리를 재구현하려면 같은 이름의 UPROPERTY를 가진 클래스로 E 방식을 쓰면 값을 살릴 수 있다(미실험).
- `UnrealEditor.exe`를 `-nullrhi`로 띄우고 `quit_editor()`로 닫으면 종료 시 한 번 세그폴트가 났다(D). 로그 기록 뒤라 결과에는 영향이 없고 E·F는 정상 종료했다.

### 4. 스켈레톤·메시 차이

랩 프로젝트에서 옛 본(`GASP_Mannequin/Meshes`)과 5.8 본(`UEFN_Mannequin/Meshes`)을 함께 로드해 `AnimPoseExtensions`·`SkeletalMeshComponent.get_ref_pose_transform`으로 비교했다(`raw/skeleton_mesh_compare.json`). 메시 정점 수는 MCP로 읽었다.

| 항목 | 옛 본 | 5.8 | 영향 |
|:---|:---|:---|:---|
| 본 | 88 | **93** — 끝에 `props_root`·`prop_01`·`poi`와 가상 본 `VB hand_l_prop_01`·`VB hand_r_prop_01` 추가 | 기존 88개의 순서·이름은 그대로(인덱스 보존). 새 본은 레퍼런스 포즈만 가진다 |
| 바인드(레퍼런스) 포즈 | — | 25개 본에서 쿼터니언 차 1e-6 미만, 이동·스케일 차 0 | **사실상 동일.** ISKM 바인드 포즈 함정 해당 없음. 메시 레퍼런스 포즈와 스켈레톤 레퍼런스 포즈도 양쪽 모두 일치 |
| 가상 본 | 없음 | 2개(위) | `VB IK_Hand_*`는 여전히 없다(§2.7 판단 유지) |
| 소켓 | 메시 2(`palm_l/r_Socket`) | 메시 5 — `RagdollTrace`(spine_03), `prop_01_Socket`, `VB hand_r_prop_01_Socket` 추가 | 기존 2개는 위치·회전 동일 |
| 스켈레톤 커브 메타데이터 | — | `face__l/r_eye_h/v` 4개 추가 | 영향 없음 |
| 메시 지오메트리 | 정점 6,009 / 3,423 / 2,349 (LOD 0/1/2) | **7,438 / 4,799 / 3,599** | Interchange로 다시 임포트된 메시다. 바운드·머티리얼 슬롯(`lambert1`)은 같다. ISKM `ASTP_Enemy_UEFN_*`와 그로부터 만든 `SM_UEFN_Mannequin`은 1C-4에서 다시 굽거나 다시 만들어야 할 수 있다 |
| 포스트프로세스 ABP | 없음 | **`ABP_UEFN_Mannequin_PostProcess`** — head 스케일 1.15, thigh_l/r (1, 1.12, 1.12), foot 1.0의 Modify Bone 체인 | `SKM_UEFN_Mannequin`을 쓰는 모든 컴포넌트(현재 `ActorPromoted` 적 `VisualMesh`, 2-0b부터 `AnimSourceMesh`)에서 머리·허벅지 비율이 바뀌고 인스턴스마다 ABP 평가 비용이 붙는다 |
| LOD 설정 | 없음 | `Character_LodSettings` 연결 | — |
| PhysicsAsset | `PA_UEFN_Mannequin` | 같은 경로, 새 본은 `PM_UEFN_Mannequin` 참조 | 1A가 쓰는 플레이어 `VisualMesh`(MoverSample)와는 무관 |

AN-011로 이 스켈레톤이 이후 모든 캐릭터의 기준이 된다. 1C-3 이후 기준은 **93본 5.8 스켈레톤**이다.

### 5. 백업

`Content/Characters/GASP_Mannequin/` 전체(436개, 424MB — MotionMatchingData·Enemy 포함)를 `Saved/AssetRefresh/2026-10-18/backup/GASP_Mannequin/`에 복사했다. SHA-256 목록은 `backup_sha256.txt`(436줄)이고, 복사 직후 원본을 다시 해시해 목록과 일치함을 확인했다.

### 6. 출처 미판별 4개

| 에셋 | 판별 | 근거 | 제안 |
|:---|:---|:---|:---|
| `Animations/ABP_UEFN` | **LootNPop 산출물**(구성 에셋) | git b7700da(2026-05-22)에서 `Content/MotionMatchingData/ABP_UEFN`으로 추가, ac49e71(2026-05-25)에서 GASP 폴더로 옮겨지며 git에서 빠졌다. 참조자는 `CHT_PSD_UEFN`(컨텍스트 클래스)뿐이고 `CHT_PSD_UEFN`은 현재 참조자가 없다 | 1C-2에서 `MotionMatchingData/`와 함께 LootNPop 소유 폴더로 옮겨 다시 추적하거나, `CHT_PSD_UEFN`과 함께 삭제 후보 — 사용자 결정 |
| `Animations/Poses/M_Pose_01` | **옛 GASP 원본** | AssetImportData 경로 `Build/ArtSource/UE5/Characters/M_UEFN/Animation/Poses/M_Pose_01.fbx`(Epic 내부 경로). 참조자 없음 | **삭제 후보** |
| `Animations/Run/M_Relaxed_Run_Loop_F_Troy` | **옛 GASP 원본**(5.8에서 빠짐) | 임포트 경로 `.../M_UEFN/Animation/MotionMatching/Run/FBX/M_Relaxed_Run_Loop_F_troy.fbx`. 참조자 `PSD_Run_UEFN`(LootNPop MM 데이터) | `PSD_Run_UEFN`이 2-2에서 교체되면 삭제 후보. 그때까지 유지 |
| `Meshes/SM_UEFN_Mannequin` | **LootNPop 산출물**(SKM에서 에디터로 만든 StaticMesh) | 임포트 정보 없음. 참조자 `DA_EnemyEntityConfig_ActorPromoted_Melee01`·`PureEntity_Melee01`·`PureEntity_Ranged01` | `Content/Characters/Retargeted/`로 이동(git 제외 유지). 새 SKM에서 다시 만들지 1C-4에서 판단 |

삭제는 하지 않았다.

### 7. 산출물 이동 목록·재생성 절차

이동 목록은 `retargeted_move_list.tsv`(Enemy 14 + `SM_UEFN_Mannequin`). 재생성 절차는 Phase 문서 1C-1 절. 새로 알게 된 것:

- Enemy 14개의 원본은 모두 LootNPop 안의 **Lyra 스켈레톤 시퀀스**다. `A_SW_*` 9개는 `Lyra_Mannequin/Animations/Sword/`(원 FBX는 마켓플레이스 Warrior 팩), `MM_*` 5개는 `Lyra_Mannequin/Animations/{Actions,Locomotion/Rifle,Locomotion/Shotgun}/`. 원본 14개 모두 존재를 확인했다.
- 리타기터는 시퀀스에 기록되지 않는다. `RTG_Lyra_to_GASP_noRootMotion`으로 다시 리타깃한 것이 확인된 것은 `MM_Rifle_Jog_Fwd_UEFN`뿐이다([TechDesign_EnemyNPC_LowLOD.md](../../TechDesign_EnemyNPC_LowLOD.md) §6.7, fc32afb). 나머지는 5b(89a7b47, 2026-09-06) 때 만들어져 noRootMotion 리타기터(09-13)보다 먼저다.
- `A_SW_RelaxWalk_UEFN`은 참조자가 없다.
- Additive였던 `MM_Shotgun_Fire_UEFN`은 리타깃 뒤 `AdditiveAnimType`을 `AAT_None`으로 바꿨다(같은 문서 §6.7). 재생성 때 이 후처리를 반복해야 한다.

### 시행착오·주의

- **해시 비교만으로 "바뀐 파일"을 고를 수 없다.** 411개 전부 해시가 다르다. 본문·하드 참조를 나눠 봐야 한다.
- uasset 요약 헤더는 UE5 버전 1018에서 썸네일 테이블 오프셋 뒤에 int32 두 개가 더 있다(실측). 파서 없이 이름 맵을 읽을 때 주의.
- UBT는 같은 엔진 실행 파일로 Live Coding 세션이 떠 있으면 다른 프로젝트 빌드도 거부한다. 다른 프로젝트 모듈만 빌드할 때는 `-NoHotReloadFromIDE`로 우회할 수 있다(LootNPop 바이너리는 건드리지 않는다).
- GASP 원본 경로의 `Blueprints/AnimModifiers/` 폴더를 통째로 반입하면 Foley 노티파이를 끌고 온다. 필요한 것은 한 파일이다.
- 랩 프로젝트는 2.6GB다(Intermediate·DDC 포함). 1C-3 검증에 재사용할 수 있게 남겨 두었다.
