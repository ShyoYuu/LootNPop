# UE 5.8 애니메이션 기능 사실 조사

> 상태: 검증 필요(각 Phase 착수 시 재확인)
> 마지막 갱신: 2026-10-16
> 소유 정보: 계획 수립 때 확인한 엔진·GASP 사실과 출처. 결정은 `Decisions.md`가 소유한다.

조사 방법: 로컬 엔진 설치본(`C:/Program Files/Epic Games/UE_5.8`)의 플러그인 메타데이터·소스, 로컬 GASP 프로젝트(`D:/UnrealProjects/GameAnimationSample`) 에셋 목록, 공개 자료(§6).

## 1. UAF (Unreal Animation Framework)

- 엔진 `Engine/Plugins/Experimental/UAF/` 아래 `UAF`·`UAFAnimGraph`·`UAFAnimNode`·`UAFChooser`·`UAFControlRig`·`UAFLayering`·`UAFMass`·`UAFMirroring`·`UAFPoseSearch`·`UAFStateTree`·`UAFWarping`이 있다. 전부 Experimental이다.
- Mover 연동은 `Experimental/MoverAnimNext`(FriendlyName "Mover UAF", Experimental)다.
- 로컬 GASP에서는 UAF 캐릭터 에셋을 찾지 못했다(바이너리 문자열 검색으로는 `CR_Biped_FootPlacement` 하나만 걸림). 공개 자료에는 GASP에 UAF 캐릭터가 포함된다는 언급이 있어, Phase 5 착수 시 다시 확인한다.
- 공개 자료의 평가: 5.8은 "평가해 볼 만한" 첫 버전이지만 출시 타이틀에 걸 단계는 아니라는 의견. Mover 자체도 5.8에서 Beta로 가지 못했다(Epic 포럼 답변).
- `UAFChooser`·`UAFPoseSearch`가 있어 Chooser 테이블과 PSD는 UAF에서도 재사용될 것으로 본다 → AN-003의 근거. Phase 5에서 실증한다.

## 2. PhysicsControl과 GASP 래그돌

- 엔진 플러그인 `Engine/Plugins/Animation/PhysicsControl`. 메타데이터상 Experimental·Beta 표시가 없다(공개 기사는 Experimental로 소개). 설명: Physics Control Component와 Rigid Body With Control 애님 그래프 노드로 스태틱·스켈레탈 메시를 물리적으로 제어.
- 플러그인 소스에서 복제(`Replicated`·`GetLifetimeReplicatedProps`) 코드를 찾지 못했다 → 머신별 로컬 연출로 써야 한다(AN-004와 일치).
- GASP 구성: `Blueprints/SandboxCharacter_Mover_Ragdoll`(PhysicsControl 사용), `Characters/UEFN_Mannequin/Rigs/PCA_SandboxCharacter`(PhysicsControl 프로필 에셋), `Blueprints/MovementModes/BP_MovementMode_Ragdoll`, `Blueprints/GM_Sandbox_Ragdoll`, 기상 몽타주 Chooser 입출력 `S_CHT_GetUpMontages_IN/OUT`, `E_RagdollInjuryState`, `S_RagdollProperties`, `BP_AnimNotify_TriggerRagdoll`.
- 공개 자료가 소개하는 용도는 넘어짐·부상·허우적거림·구르기 같은 파워드 래그돌과 모션매칭 기상이다. **전용 히트리액트 시스템은 소개되지 않았다.** 상체 히트리액트는 이 부품으로 직접 구성해야 한다.
- LootNPop 쪽: 랙돌은 `VisualMesh`의 PhysicsAsset을 쓰고(`ALNPCharacterBase::EnterRagdoll`), Chaos 중력을 끄고 `TickRagdollGravity`로 구면 중력을 준다([TechDesign_CharacterMovement.md](../../TechDesign_CharacterMovement.md) §9).

## 3. Locomotor (사족 드론, 트랙 L)

- 엔진 `Engine/Plugins/Experimental/Animation/Locomotor`. Experimental + Beta, 설명 "Procedural animation for Control Rig". 공개 헤더: `Locomotor.h`·`LocomotorCore.h`·`RigUnit_Locomotor.h`.
- GASP 예제: `IsolatedExamples/Locomotor/`의 `BP_Walker`·`CR_Walker`·`ABP_Walker`·`GM_Locomotor`·`PC_Locomotor`, 메시 `Mech/Meshes/SK_Mech`(사족: `leg_fr_01~04_l/r`·`leg_bk_01~04_l/r`·`foot_fr/bk_01_l/r`·`legbase_*`·`legpump_*`), 리그 `Mech/Rigs/CR_Mech`. **Physics Asset은 폴더에 없다.**
- 설정 구조: `FMovementSettings`(SpeedMin/Max·PhaseSpeed·Acceleration — 자체 속도로 몸통을 움직인다), `FStepSettings`(보폭 공중 비율·발 높이·발끼리 충돌·지면 sphere cast).
- ⚠️ **월드 Z Up 하드코딩**(`LocomotorCore.cpp`): `FVector(0,0,1)` 상수(450·776행), 지면 투영 sphere cast가 월드 ±Z 오프셋이고 실패 시 `ZAxisVector` 반환(616~629행), 높이 오프셋을 월드 `.Z`로 저장(66·749행), 몸통 흔들림을 `.Z`에 더함(894행), 발끼리 충돌을 월드 XY 2D 평면에서 계산(~593~611행). 시뮬레이션은 `CurrentWorld`(실제 월드 공간)에서 돌고 `ExecuteContext.ToVMSpace`로 되돌린다(`RigUnit_Locomotor.cpp` 140·150행). 구면 월드에서는 북극 근처에서만 맞는다 → 포크 필요.
- 이동 외 전투 모션은 예제에 없다(AN-008).

## 4. 마커리스 모션 캡처

- 이름: MetaHuman Animator Markerless Motion Capture 플러그인. UE 5.8, Experimental, 무료(상업 이용 포함 보도), Windows 전용, Fab에서 설치(약 5GB).
- 단일(모노) 카메라 영상에서 바디(+얼굴·손)를 추출해 MetaHuman 애니메이션으로 출력. 처리는 로컬.
- 필요: MetaHuman Animator 플러그인, Capture Manager 플러그인, Live Link Hub. 엔진에 `Engine/Plugins/MetaHuman/MetaHumanAnimator` 등이 설치돼 있다.
- 공개 정확도 수치는 기반 모델 제작사(Meshcapade) 발표라 독립 검증되지 않았다. 초기 사용 후기는 고정 카메라에서 양호.
- 이 프로젝트에 걸리는 한계(추정, 1B에서 실측): 무기 미추적 → 손목 회전 오차가 칼 궤적 오차로 직결, 게임 영상의 카메라 컷·이동·히트스톱·VFX 가림, 과장된 비인체 비율.
- 권리: 다른 게임의 모션은 해당 스튜디오의 모캡·애니메이션 작업물이다(AN-010).

## 5. 애니 노드의 Up 축 가정 (구면 월드 점검표)

엔진 `Engine/Plugins/Animation/AnimationWarping/Source/Runtime/Private/BoneControllers/` 기준.

| 노드 | Up 축 | 판단 |
|:---|:---|:---|
| `AnimNode_FootPlacement` | 접근 방향·IK 루트 평면을 대부분 컴포넌트 공간 `UpVector`로 계산(48·49·1204·1445행) | 메시가 중력 Up에 정렬돼 있으면 안전. 2-3에서 적도 확인 |
| `AnimNode_OrientationWarping` | 축 선택 기본값 `UpVector`(46행), 나머지는 디버그 그리기 | 컴포넌트 공간이면 안전 |
| `AnimNode_OffsetRootBone` | `UpVector`는 디버그 그리기에만 등장 | 안전 추정. 2-2에서 확인 |
| `AnimNode_SlopeWarping` | 기본 `GravityDir = -FVector::UpVector`(월드) | ⚠️ 기본값 금지. 쓰려면 중력 방향을 매 프레임 공급 |
| Locomotor | 월드 Z 하드코딩(§3) | ⚠️ 포크 필요 |

## 6. LootNPop의 옛 GASP 소스 에셋 현황 (2026-10-18)

파일 시스템 상대 경로 대조와 LootNPop MCP `get_referencers`로 조사했다.

- `Content/Characters/GASP_Mannequin/`(MotionMatchingData 제외) 429개 ↔ 최신 GASP `Content/Characters/UEFN_Mannequin/` 2,363개.
  - 상대 경로가 같은 것 411개. 바이트 동일 여부는 아직 비교하지 않았다(1C-1).
  - LootNPop에만 있는 것 18개: `Animations/Enemy/*` 14개(5.7MB, LootNPop이 만든 리타깃 적 애니), `Animations/ABP_UEFN`, `Poses/M_Pose_01`, `Run/M_Relaxed_Run_Loop_F_Troy`, `Meshes/SM_UEFN_Mannequin`.
  - 폴더별(구/신): Animations 414/2,345, Materials 4/5, Meshes 4/3, Rigs 4/7, Textures 3/3.
- git: `GASP_Mannequin/` 중 추적되는 것은 `MotionMatchingData/` 7개(CHT·PSD 4·PSN·PSS)뿐이다. **`Animations/Enemy/*`도 `.gitignore` 37행 `Content/Characters/GASP_Mannequin/*`에 걸려 추적되지 않는다** — LootNPop 산출물인데 저장소에 없다.
- GASP 시퀀스의 게임 의존성 예: `M_relaxed_ragdoll_shove_stand_B_A` → `/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin` 하나. LootNPop의 같은 스켈레톤은 `/Game/Characters/GASP_Mannequin/Meshes/SK_UEFN_Mannequin`에 있다(경로만 다름).
- `SKM_UEFN_Mannequin` 참조자(LootNPop 쪽): `BP_LNPEnemy`, `DA_EnemyEntityConfig_PureEntity_Melee01/Ranged01`, `ASTP_Enemy_UEFN_Sword_Body/Shotgun_Body`, 리타기터 `RTG_GASP_to_MS`·`RTG_Lyra_to_GASP`·`RTG_Lyra_to_GASP_noRootMotion`, Enemy 애니 14개.
- `SK_UEFN_Mannequin` 참조자: 위 Enemy 애니, 무기 `SKM_LongSword_UEFN`·`SKM_Shotgun_UEFN`, `PSS_CoreLocomotion_UEFN`, GASP 유래 시퀀스·블렌드 스페이스 전부.
- 코드·Config·`.context` 문서(이 폴더 제외)에는 `GASP_Mannequin` 경로 문자열이 없다.
- MCP에는 Migrate·Replace References·Python 실행 도구가 없다. 같은 프로젝트 안의 `AssetTools.move`/`duplicate`/`delete`와 의존성 조회는 있다.

## 7. 출처

- [Download the latest Game Animation Sample Project — UE 5.8 (Epic)](https://www.unrealengine.com/tech-blog/download-the-latest-game-animation-sample-project-now-updated-for-ue-5-8)
- [GASP 5.8 정리 (ProjProd)](https://www.projprod.com/post/game-animation-sample-project-unreal-engine-5-8)
- [GASP 5.8 (The Rookies)](https://www.therookies.co/blog/headlines/unreal-engines-game-animation-sample-project)
- [UE 5.8 LiveLink & Animation Framework Deep Dive (Lunar Lizard)](https://lunarlizard.bg/blog/ue58-livelink-animation-framework/)
- [Mover UAF API 문서](https://dev.epicgames.com/documentation/unreal-engine/API/PluginIndex/MoverAnimNext?lang=en-US)
- [UAF 플러그인 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/PluginIndex/UAF)
- [Mover 로드맵 포럼 스레드](https://forums.unrealengine.com/t/any-roadmap-updates-on-mover/2833898)
- [MetaHuman Animation from Mono Video Capture (Epic 문서)](https://dev.epicgames.com/documentation/metahuman/metahuman-animation-from-mono-video-capture-in-unreal-engine)
- [Free MetaHuman Animator Markerless Mocap plugin (CG Channel)](https://www.cgchannel.com/2026/06/get-the-free-metahuman-animator-markerless-mocap-plugin/)
- [MetaHuman gets markerless mocap (Digital Production)](https://digitalproduction.com/2026/06/23/metahuman-gets-markerless-mocap/)
