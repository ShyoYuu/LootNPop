# 컴뱃 애니메이션 시스템 기술 설계

## 1. 한눈에 보기

Motion Matching 공용 로코모션 위에 **무기별 Linked Anim Layer를 런타임 교체**한다. 메인 AnimBP는 직선 파이프라인만 두고, 무기별 블렌딩은 전부 서브 ABP에 위임한다. 아래 순서는 AnimationOverhaul Phase 2(GASP Mover ABP 재구성, 2026-10-21 완료)의 최종 포즈 파이프라인이다. Phase 3 물리 히트리액트는 이 ABP가 아니라 표시 ABP(§3.3)에 들어간다.

```
[ALNPCharacterBase::ApplyWeaponVisuals()]
       │  표현 세트·조준 모드 태그 전환 + LinkAnimClassLayers() + 무기 메시 어태치
       ▼
[ABP_Main_UEFN — 메인 AnimBP (AnimInstance = ULNPAnimInstance, 스켈레톤 SK_UEFN_Mannequin, §3.3)]
   Motion Matching (PSD 전신 포즈, 궤적 = ULNPAnimInstance::Trajectory,
   │                DB 묶음 = CHT_PoseSearchDatabases_UEFN 멀티 결과 — ULNPAnimInstance::UpdateMotionMatchingNode)
   │   └ 블렌드 스택 그래프(샘플마다): CopyBone foot→ik_foot → Steering(궤적 facing) → Steering(제자리 회전 DB)
   │                                  → Orientation Warping(LastNonZeroVelocity) → Reset Root   (2026-10-20 AnimationOverhaul 2-3)
       │
   Blend Poses by Bool (bIsOnGround) — 공중이면 Fall_Loop 고정 재생 (§5.5)
       │
   Linked Anim Layer (ALI_WeaponStyles::ApplyWeaponStyleOverlay)
       │   └─ ABP_Sub_{Unarmed,Pistol,Rifle,Shotgun,LongSword}_UEFN 런타임 교체
       │
   Additive Leans (측면 가속 몸통 기울기 + 머리 기울기, 이동 중만 — 2026-10-21 AnimationOverhaul 2-6)
       │
   Slot 'FullBodyAdditivePreAim' (사격) → Slot 'DefaultSlot' (검술 공격·피격) → Slot 'FullBody' (대시)
       │
   Save Cached 'PreUpperBody' → Layered Blend per Bone ← Slot 'UpperBody' (재장전 — TechDesign_Ability.md §5.5)
       │
   Slot 'AdditiveHitReact' (가산 피격)
       │
   Dead Blending (0.25s, 관성 블렌드 요청을 받는 자리 — 2026-10-19 AnimationOverhaul 2-1)
       │
   Offset Root Bone (태그 OffsetRoot — 메시 루트가 캡슐을 부드럽게 따라온다. 전신 몽타주 중 Release, 2026-10-20 2-3)
       │
   Control Rig CR_Biped_FootPlacement_UEFN (발 배치 — 경사 보정·발 고정·루트 감쇠, 알파 = 지상, 2026-10-20 2-5)
       │
   Linked Anim Layer (ALI_WeaponStyles::ApplyWeaponPostProcess)
       │
   Pose History Collector (출력 직전 — 절차 보정까지 반영된 포즈를 기록)
       │
   Output Pose
```

**루트 추종(Offset Root Bone).** 메시 루트는 캡슐을 바로 따르지 않고, 애니메이션 루트모션(Steering이 궤적 facing 쪽으로 휘게 한 것)으로 움직이다가 반감기로 캡슐에 다가간다(이동 30cm·회전 90° 상한). 모드·반감기·지면 법선은 `ULNPAnimInstance` 함수가 핀으로 공급하고, 움직이는 base 이동량과 가상 루트 Up 정렬은 `NativeThreadSafeUpdateAnimation`이 태그로 노드를 찾아 넣는다. `DefaultSlot`·`FullBody` 몽타주 중에는 Release라 몽타주 루트모션·근접 보정 워핑(§6.6)은 캡슐과 메시가 같이 간다. 엔진 노드의 구면 함정은 §7.4. GASP 값·구성은 AnimationOverhaul `design/GASPReference.md` §2.10.

**발 배치(Control Rig).** GASP `CR_Biped_FootPlacement`를 복제해 구면용으로 고친 `CR_Biped_FootPlacement_UEFN`이 지상에서 발을 실제 지면에 맞추고(접지 트레이스·바닥 평활·골반 보정), 정지 중 디딘 발을 고정하며(애니 곡선 `contact_l/r`), 캡슐 계단 스냅을 루트 높이 감쇠로 부드럽게 한다. 노드 알파는 `ULNPAnimInstance::ShouldUseFootPlacementRig`(지상)로 Bool 블렌드되고 공중에는 IK가 없다. 착지 프레임에 CR 상태를 리셋한다. 애니 발 목표는 FK 발끝이다(`UseIKBoneTargets` 끔 — Lyra 리타깃 몽타주의 ik_foot을 믿을 수 없다). 움직이는 base(이동 패널) 위에서는 발 고정·루트 감쇠를 끈다(`IsOnStaticGround`) — 공급하는 base 이동량이 CR 평가 시점보다 한 프레임 늦어 핀이 끌리고 루트가 튀었다. 경사 전용 애니는 쓰지 않는다(AN-021). 구성·근거는 AnimationOverhaul `design/GASPReference.md` §2.12, CR 재현 스크립트 `Scripts/AnimOverhaul/FixFootPlacementCR_2-5.py`, CVar `LNP.Anim.FootPlacement`·`LNP.Anim.FootPlacementOnMovingBase`·`LNP.Anim.DebugFootPlacement`.

**기울기(Additive Leans).** GASP `AdditiveLeans` 레이어를 AnimGraph 노드 묶음으로 옮겼다. 측면 가속(`ULNPAnimInstance::LeanAmount`, 캐릭터 로컬 축)을 Walk·Run 기울기 BS(α = `GroundSpeed` 200~320)에 넣어 Mesh Space Additive로 얹고, 그 위에 머리 기울기 BS를 더한다. 분기는 C++ bool 두 개다. `ShouldApplyAdditiveLeans`는 이동 상태 Moving일 때(공중 포함), `ShouldApplyHeadLean`은 FreeAim 무기도 락온도 아닐 때 참이다. AO와 머리 기울기가 겹치면 머리가 과회전하고, 락온 중에는 시선이 타깃에 고정돼야 해서다. 락온 여부는 InputCmd `FLNPModifierInputs::bIsLockOn`에서 읽는다 — 루즈 태그는 건 머신에만 있다(§2). 곡선 `Disable_AdditiveLeans`가 일부 회전 애니에서 기울기를 끈다. **위치는 무기 오버레이 뒤·슬롯 앞이다.** 무기 서브 ABP는 `spine_02` 위를 메시 공간 회전 블렌드로 무기 자세로 덮어서, 앞에 두면 무기를 든 상체의 기울기가 지워진다. 몽타주 슬롯은 GASP처럼 기울기를 덮는다. AO 뒤에 얹히므로 측면 가속 중 총구가 조금 기울지만, 발사 방향은 총구 → 조준점으로 수렴한다([TechDesign_HitDetection.md](TechDesign_HitDetection.md) §7.7). CVar `LNP.Anim.AdditiveLeans`(0 끔), 확인은 `LNP.Anim.DebugLocomotion`(화면 `lean x,y on/off +head`, 2면 `[AnimB3]` 1초 로그). GASP 구성은 AnimationOverhaul `design/GASPReference.md` §2.13.

**애니 변수 공급층.** 상태 enum(이동 모드·이동 상태·보행·자세·회전 모드·이동 방향)과 그 이력, 궤적 파생값(`Trj_*`·`FutureFacingDelta`), 가속, 지면 법선·경사, 상태 술어(`bIsPivoting`·`bShouldTurnInPlace`·`bJustLanded_*` 등)는 `ULNPAnimInstance`가 메인 인스턴스에서만 C++로 계산한다(AnimationOverhaul AN-002·AN-003). 궤적 생성(Mover 예측기)도 C++가 소유하고, `PoseHistoryCollector`의 `TransformTrajectory` 핀이 C++ `Trajectory`에 이름으로 바인딩돼 있다. 계산식·GASP 출처는 AnimationOverhaul `design/GASPReference.md` §2.8, 확인용 CVar는 `LNP.Anim.DebugLocomotion`(1 화면 표시, 2 + 전이 로그).

### 무기별 이동·조작 방식

| 상황 | 조준 모드 | 회전 방식 | 이동 방식 |
|------|----------|----------|----------|
| 맨손 / 롱소드 (락온 해제) | None | 입력 방향으로 캐릭터 회전 | 항상 앞으로 이동 |
| 원거리 무기 | FreeAim | 카메라 정면 고정, 조준점 상시 표시 | 전후좌우 Strafe |
| 락온 중 (무기 무관) | — (컴포넌트 상태) | 타깃 방향 고정 (`ULNPLockOnComponent`) | 전후좌우 Strafe |

### 핵심 클래스

| 클래스 | 역할 |
|:---|:---|
| `ULNPAnimInstance` | Mover 상태 → ABP 변수 공급 (속도·방향·Guard·락온·Aim Offset·왼손 IK 타겟, 로코모션 궤적·상태·술어, 루트·발 배치·기울기 판정 함수) |
| `ALNPCharacterBase` | `ApplyWeaponVisuals()` 레이어 교체, `EvaluateMontage()`/`PlayMontage()` Chooser 평가, 콤보 상태 |
| `ULNPMontageChooserContext` | Chooser Table(`CHT_Montage`) 평가 입력 (WeaponType / SituationType / Value 태그) |
| `UANS_LNP*` 5종 | 몽타주 타임라인 구간별 제어 (§6.3) |
| `ULNPAbility_MeleeAttack` | 공격 몽타주 재생 + 근접 공격 타겟 보정 (§6.6) |

---

## 2. Gameplay Tags

| 분류 | 태그 | 설명 |
|------|------|------|
| 표현 세트 | `LNP.VisualSet.Unarmed / Pistol / Rifle / Shotgun / LongSword` | `ULNPWeaponVisualSet::AnimSetTag`. **무기 식별자가 아니다** — 런처는 샷건 세트를 공유해 `LNP.VisualSet.Shotgun`이다 |
| 조준 | `LNP.AimMode.None / FreeAim` | `ULNPWeaponData::DefaultAimMode`가 정한다 |
| 락온 | `LNP.AimMode.LockOn` | `ULNPLockOnComponent`가 타겟 지정/해제 때 부여·제거. ⚠️ **루즈 태그라 복제되지 않는다** — 락온을 건 머신에만 있다. 시뮬레이션 판정(대시 방향)은 `FLNPModifierInputs::bIsLockOn`을 쓴다 |
| 차단 | `LNP.Block.MovementInput` / `LNP.Block.AttackInput` | 공격 몽타주 구간은 ANS가, 경직 구간은 GA_Stagger의 `ActivationOwnedTags`가 소유 (§6.5) |
| 콤보 | `LNP.State.ComboWindow` | ANS_ComboWindow가 열고 `TryActivateAttack`이 소비 |
| Guard | `LNP.State.Guarding` / `LNP.State.ParryWindow` | InputHandler가 ASC에 유지/타이머 관리 |
| 경직 | `LNP.State.Staggered` | GA_Stagger가 어빌리티 수명 동안 소유. 공격·가드·대시 입력 게이트와 재경직 차단에 쓰인다 |
| Mover | `LNP.Mover.IsSprinting` / `IsGuarding` / `IsADS` | Modifier 활성 여부. 앞의 둘이 `ULNPAnimInstance` 변수 소스 |

`LNP.Action.Attacking`은 정의만 남아 있고 부여·조회하는 코드가 없다.

---

## 3. AnimBP 구조

### 3.1 에셋 구성

| 에셋 | 종류 | 내부 처리 |
|--------|------|------|
| `ABP_Main_UEFN` | Main AnimBP | Motion Matching + 레이어 합성 파이프라인 (§1). 로코모션 Chooser `CHT_PoseSearchDatabases_UEFN`(§5.3) |
| `ALI_WeaponStyles` | Anim Layer Interface | `ApplyWeaponStyleOverlay` 레이어 함수 계약 |
| `ABP_Sub_Unarmed_UEFN` | Sub AnimBP | 베이스 포즈 통과 |
| `ABP_Sub_Pistol_UEFN` | Sub AnimBP | spine_01 이상 파지 포즈 + `AO_MM_Pistol_Idle_ADS` |
| `ABP_Sub_Rifle_UEFN` | Sub AnimBP | spine_01 이상 파지 포즈 + `AO_MM_Rifle_Idle_ADS` |
| `ABP_Sub_Shotgun_UEFN` | Sub AnimBP | Rifle과 동일 구성. 아이들만 `MM_Shotgun_Idle_ADS`, AO는 Rifle 것을 공용 |
| `ABP_Sub_LongSword_UEFN` | Sub AnimBP | 롱소드 Stance + `bIsGuarding` Guard 자세 분기 + 왼손 Two Bone IK |

서브 ABP가 쓰는 Lyra 애니는 UEFN으로 오프라인 리타깃한 사본(`Content/Characters/Retargeted/Lyra/*_UEFN`, git 제외)이다. AO·몽타주 이름은 사본 이름에서 `_UEFN`을 뺀 원본 이름으로 적는다. 전환 전 Lyra 스켈레톤 구성(`ABP_Lyra`·`ABP_Sub_*`)은 되돌림용으로 남아 있고 런타임에서는 참조하지 않는다(2026-10-18 AnimationOverhaul 2-0b).

**설계 원칙:** 어떤 본에 얼마나 블렌딩할지는 서브 ABP가 정한다. 메인 ABP는 무기를 모른다.

**Shotgun이 Rifle AO를 공용해도 되는 이유:** Lyra 원본도 `ABP_ShotgunAnimLayers`가 `ABP_RifleAnimLayers`의 데이터 전용 자식이다. `AO_MM_Rifle_Idle_ADS`는 메시 스페이스 회전 애디티브라 델타가 절대 회전으로 적용되므로, 베이스 아이들이 바뀌어도 조준 각도가 유지된다(로컬 스페이스 애디티브였다면 어긋난다).

### 3.2 런타임 레이어 교체 (`ApplyWeaponVisuals`)

```
ApplyWeaponVisuals(WeaponData)   ← 서버·클라이언트 공용 단일 진입점 (멱등)
├─ 조기 반환: 같은 무기로 이미 적용됐으면 아무것도 안 한다 (LinkAnimClassLayers 중복 = 포즈 튐)
├─ ASC: 기존 표현 세트·조준 모드 태그 제거 → 신규 태그 부여
├─ InputHandler: SetFaceMoveDirection(!bFreeAim), 조준 모드가 바뀌었으면 NotifyAimModeChanged()
├─ AnimSourceMesh->LinkAnimClassLayers(VisualSet->AnimLayerClass)   (없으면 UnarmedAnimLayerClass)
└─ WeaponMesh: 메시 교체 + 소켓 어태치 + 상대 오프셋(VisualSet->WeaponMeshRelative*)

호출자 — 무기 원본은 ULNPEquipmentComponent::WeaponSlot(플레이어) / EnemyConfig(적), 둘 다 복제된다:
├─ 푸시: 슬롯 적용 직후(서버) · OnRep_WeaponSlot / OnRep_EnemyConfig(클라)
└─ 풀:   BeginPlay · OnRep_PlayerState · PossessedBy (RefreshWeaponVisuals — 캐시 무효화 후 재적용)
   상세는 TechDesign_Inventory.md §4.1
```

표현 필드는 무기 DA가 아니라 **`ULNPWeaponVisualSet`**(`WeaponData->VisualSet`)에 있다 — 메시·소켓·그립 보정·애님 레이어·`AnimSetTag`가 한 덩어리다. 여러 무기가 같은 세트를 가리키는 것이 "표현만 재활용한다"의 유일한 선언이다.
⚠️ 그 태그는 무기를 식별하지 않는다. 규칙(쿨다운 등)의 키로 쓰면 세트를 공유하는 무기들이 함께 묶인다 — 규칙의 키는 무기 DataAsset 자신이다.
⚠️ `RefreshWeaponVisuals()`는 `InitAbilitySystem()` **뒤에** 불러야 한다. 후자가 표현 세트 태그를 Unarmed로 되돌린다.

### 3.3 메시 구성 — 포즈 원본과 표시 메시

```
AnimSourceMesh (숨김) ── SKM_UEFN_Mannequin + ABP_Main_UEFN, bDisablePostProcessBlueprint = true
   │   로코모션·무기 레이어·몽타주·ANS·왼손 IK가 모두 여기서 돈다 (플레이어·승격 적 공통)
   ▼  (VisualMesh가 부모 메시의 포즈를 받는다)
VisualMesh ──┬─ 플레이어: SKM_Manny_Simple(MoverSample) + ABP_Retarget (태그 RTG_GASP_to_MS → 리타기터 맵) → 왼손 IK
             └─ 승격 적:  SKM_UEFN_Mannequin + ABP_VisualCopyPose (Copy Pose From Mesh, 같은 스켈레톤) → 왼손 IK
   랙돌·물리 히트리액트의 대상. 무기 메시는 VisualMesh 소켓에 붙는다
```

- **적은 Leader Pose가 아니라 ABP 노드로 복사한다.** Leader Pose면 `VisualMesh`가 ABP를 돌리지 않아, 물리 히트리액트용 RigidBodyWithControl 노드를 둘 곳이 없다. Phase 3-1에서 두 표시 ABP(`ABP_Retarget`·`ABP_VisualCopyPose`)의 왼손 IK(§4.2) 뒤, 출력 앞에 RBWC를 넣었다(§6.4, → AnimationOverhaul `research/PhysicsHitReactSpike.md` §1.5·§6).
- **`AnimSourceMesh`에도 포스트프로세스 ABP를 끈다**(AnimationOverhaul AN-019). GASP 5.8 메시에 붙어 온 `ABP_UEFN_Mannequin_PostProcess`의 머리·허벅지 고정 스케일이 포즈 원본에 들어가지 않게 한다.
- **플레이어 리타기터 `RTG_GASP_to_MS`의 대상 IK 리그는 `IK_MS_FootGoal`이다.** MoverSample `IK_Mannequin`을 복제해 다리 체인 끝·목표를 `foot`으로 옮기고 다리 목표를 솔버에서 뗐다. 다리는 FK로 원본 회전을 그대로 받고, IK는 팔만 풀어 무기 그립(오른손 기준 왼손 위치)을 맞춘다. 원본 리그의 다리 목표(`ball`)를 그대로 풀면 달릴 때 다리가 프레임마다 100° 넘게 뒤집힌다. 대상 리타깃 포즈는 **정렬하지 않은 기본 포즈**다. `auto_align_all_bones`로 정렬하면 견착 위치 오차는 줄지만 위팔·아래팔 기준 자세가 13°·22° 돌아 총구가 27° 숙여진다(세 총 모두). 정렬하지 않으면 총구 방향은 Lyra와 같고, 총이 상체에 대해 약 9cm 뒤·4cm 위에 남는다 → 무기별 견착 보정(§4.2 아래)으로 맞춘다. 원본 UEFN 리그에 없는 대상 체인(트위스트·`ik_*`)은 매핑하지 않고, op 스택 맨 앞의 Copy Base Pose가 본 이름으로 로컬 포즈를 복사한다. 설정 스크립트 `Scripts/AnimRetarget/FixGASPtoMSRetargeter.py`(`verify` 인자로 튐·그립 측정).
- **몽타주 슬롯 그룹은 C++가 맞춘다**(AnimationOverhaul AN-020). GASP `SK_UEFN_Mannequin`에는 `DefaultGroup{DefaultSlot}`·`Partials{UpperBody}`뿐이라, `ULNPAnimInstance::NativeInitializeAnimation`이 Lyra와 같은 표를 `USkeleton`에 런타임 등록한다: `DefaultGroup` ← `DefaultSlot`·`FullBody`·`FullBodyAdditivePreAim`·`UpperBody`·`UpperBodyAdditive`, `AdditiveGroup` ← `AdditiveHitReact`. 이게 없으면 ① 피격 몽타주가 공격과 같은 그룹이 되어 공격을 끊고 ② 재장전(`UpperBody` + `UpperBodyAdditive`)은 그룹이 갈려 `HasValidSlotSetup()`에 걸려 **재생되지 않는다**. `SetSlotGroupName`은 패키지를 dirty로 만들지 않는다. ⚠️ 에디터에서 `SK_UEFN_Mannequin`을 열어 저장하면 이 그룹이 소스 에셋에 들어간다 — 저장하지 말 것(AN-018).

---

## 4. 오버레이 상세

### 4.1 원거리 무기 Aim Offset — 구형 월드 보정

AO 에셋은 서브 ABP가 적용하고, `AimYaw`/`AimPitch`는 `ULNPAnimInstance`가 계산한다.

"카메라 Yaw − 캐릭터 Yaw" 방식은 월드 Euler 뺄셈이라 캐릭터 Up이 World Up과 벌어지면 틀린다. 대신 `GetBaseAimRotation()`의 방향 벡터를 캐릭터 로컬 좌표계로 변환한 뒤 Pitch(±90 클램프)·Yaw를 뽑는다. 기준 좌표계는 캡슐이 아니라 **메시가 실제로 향한 방향**(Offset Root 가상 루트가 캡슐보다 더 돈 만큼을 더한 회전)이다 — 메시가 캡슐보다 늦게 돌 때도 총구가 조준점을 유지한다. 서브 ABP 인스턴스는 메인 인스턴스의 가상 루트를 읽는다(2026-10-20 AnimationOverhaul 2-3).

**`GetBaseAimRotation()`의 공급원은 폰 종류마다 다르다.** 계산식은 그대로 두고 공급원만 갈아끼운다.

| 폰 | 공급원 |
|:---|:---|
| 로컬 제어 플레이어 | `Controller->GetControlRotation()` |
| 원격 플레이어·시뮬레이티드 프록시 | Mover InputCmd의 `ControlRotation` (복제됨, → [TechDesign_Networking.md](TechDesign_Networking.md)) |
| 적 NPC | 액터 전방 + 복제된 로컬 `AimPitchDeg` — 컨트롤러가 없어 `APawn` 기본값은 수평이다 (→ [TechDesign_EnemyNPC.md](TechDesign_EnemyNPC.md) §6) |

적 NPC도 같은 메인 ABP(`ABP_Main_UEFN`)·서브 ABP를 쓰고 AO 노드가 `AimPitch`/`AimYaw`에 바인딩돼 있으므로, 적 조준은 **애니메이션 에셋 수정 없이 `GetBaseAimRotation()` 하나로 끝난다.**

### 4.2 왼손 Two Bone IK

오른손은 무기 소켓 어태치로 정확하지만 왼손은 애니메이션 그대로라 그립에서 어긋난다.

- **그립 목표:** 무기 메시의 `LeftHandGrip` 소켓이 우선이다(롱소드). 소켓이 없는 Lyra 총기 메시(라이선스 원본이라 소켓을 추가하지 않는다)는 표현 세트의 `ULNPWeaponVisualSet::LeftHandGripOffset`(무기 메시 컴포넌트 공간, `bUseLeftHandGripOffset`)을 쓴다. 값은 전환 전 Lyra 경로의 조준 아이들(서브 ABP 기본 자세)에서 잰 `hand_l` 위치다(`Scripts/AnimRetarget/ComputeLeftHandGripOffsets.py`). 라이플 (6.5, 29.7, 7.0)·샷건 (7.3, 27.8, 6.9), 권총은 리타깃만으로 맞아 쓰지 않는다. Lyra 원본에서도 조준과 힙파이어의 무기 기준 왼손 위치가 2.5cm쯤 다르다.
- `ULNPAnimInstance`가 매 프레임 그립 월드 위치(`LeftHandGripLocation`)·존재 여부(`bHasLeftHandGrip`)·알파(`LeftHandIKAlpha`)를 갱신한다. 알파는 소켓 그립이면 1이다. 오프셋 그립이면 `1 − max(DisableLHandIK 커브, DefaultSlot·FullBody·UpperBody 몽타주 가중치)`로, 재장전·대시·경직 동안 손이 그립을 떠날 수 있게 한다. 사격·피격은 가산 슬롯이라 IK를 유지한다. 커브·몽타주는 포즈 원본(`AnimSourceMesh`) 인스턴스에서 읽는다(표시 ABP도 이 클래스를 부모로 쓴다). `DisableLHandIK` 커브는 Lyra 라이플 재장전에만 있다.
- **견착 보정(플레이어만):** `ULNPWeaponVisualSet::RightHandCorrection`(무기 공간 cm)만큼 `ABP_Retarget`의 오른팔 Two Bone IK가 손을 옮긴다(Effector = BoneSpace `hand_r` + `ULNPAnimInstance::RightHandIKOffset`, 관절 목표 = 현재 `lowerarm_r`, 손 회전 유지). 무기가 따라가고 왼손 그립 IK도 무기 기준이라 함께 따라간다. 값은 정렬하지 않은 런타임 리타깃 결과의 "오른손 `spine_05` 기준 위치"를 Lyra와 같게 만드는 이동량이다(`ComputeLeftHandGripOffsets.py`가 계산). 라이플 (0.5, 8.7, −3.8)·샷건 (0.1, 9.7, −3.9)·권총 (1.9, 6.7, −4.9) — Y가 총구 쪽, Z가 위. 알파 `RightHandIKAlpha`는 왼손 오프셋 그립과 같은 몽타주·커브 차단을 따른다. 승격 적(UEFN 표시 메시)은 오차가 달라 넣지 않았다. 순서: 리타깃 → 오른팔 IK → 왼손 IK → 출력.
- **IK는 두 군데다.** ① 포즈 원본: `ABP_Sub_LongSword_UEFN`의 Two Bone IK(`bHasLeftHandGrip`) — 롱소드만. ② **표시 메시**: `ABP_Retarget`(플레이어)·`ABP_VisualCopyPose`(승격 적)의 포즈 노드 뒤 Two Bone IK(`hand_l` → `LeftHandGripLocation`, 알파 `LeftHandIKAlpha`). 무기가 `VisualMesh` 소켓에 붙어 있으므로 ②에서 맞춰야 리타깃(UEFN → MoverSample) 비율 차이와 무관하게 그립에 닿는다. Phase 3 RBWC는 ② 뒤에 있다(§6.4).

### 4.3 Guard 자세 분기

`ULNPAnimInstance::bIsGuarding`(= `MoverComponent->IsGuarding()`)을 `ABP_Sub_LongSword`의 Bool Blend 노드가 소비해 Guard 자세와 기본 Stance를 블렌딩한다.

---

## 5. 이동·회전 연동 (Mover 2.0)

### 5.1 bFaceMoveDirection

`ApplyWeaponVisuals()`가 `SetFaceMoveDirection(DefaultAimMode != FreeAim)`으로 정하고, `ULNPInputHandlerComponent::OnProduceInput()`이 `OrientationIntent`를 만든다.

| 조건 | 입력 방향 회전 | 결과 |
|---------|:---:|------|
| AimMode None, 락온 해제 | O | 입력 방향으로 캐릭터 회전 — 앞으로만 달림 |
| AimMode FreeAim, 또는 락온 중 | X | `HorizonForward`(카메라 정면) 고정 — Strafe |

락온 중 회전은 `OnProduceInput()`이 `IsLockOnActive()`를 직접 보고 덮어쓴다(같은 머신의 로컬 상태다). 락온 토글에 조준 모드 제한은 없다 — 원거리 무기로도 락온할 수 있다.

### 5.2 LockOn 회전

`ULNPLockOnComponent`가 타겟 방향을 `ULNPControlRotationComponent`에 소프트 보정(적립 델타) + 하드 클램프(최대 이탈각)로 전달한다. 카메라가 타겟을 추적하고, Strafe 캐릭터는 카메라 정면을 보므로 자연히 타겟을 향한다(카메라 의존 방식 채택).

### 5.3 Motion Matching Strafe

`UCharacterTrajectoryComponent`는 `ACharacter` 전용이라 쓰지 않는다. Mover 2.0의 `UMoverTrajectoryPredictor`가 `OrientationIntent` 기반으로 미래 경로를 예측한다. 예측기 생성과 궤적 갱신(이력 0.033s×15, 예측 0.1s×15)은 `ULNPAnimInstance`가 한다(2026-10-19 전까지는 ABP EventGraph).

**방향별 DB 선택(2026-10-19 AnimationOverhaul 2-2).** 스트레이프 방향은 PSD 하나 안에서 궤적으로 고르지 않고 Chooser가 DB를 나눠 고른다(GASP 5.8 구성). `ULNPAnimInstance`의 `MovementDirection`(F·B·좌·우, 히스테리시스 임계 — 이동 방향 회전·질주면 항상 F)이 행 입력이다.

| Chooser 행(`CHT_PoseSearchDatabases_UEFN`, 멀티 결과 52행) | DB |
|:---|:---|
| 지상·Idle | `PSD_Stand_Idles_UEFN`(속력 20 미만), 감속 `PSD_Stand_{Walk,Run,Sprint}_Stops_UEFN`(속력 구간), 제자리 회전 `PSD_Stand_TurnInPlace_UEFN`, 착지 `PSD_Stand_Idle_Lands_{Light,Heavy}_UEFN` |
| 지상·Moving·Walk/Run × F·B·좌·우 | 루프 `PSD_Stand_{Walk,Run}_{F,B,LL,RL}_Loops_UEFN`(직전 프레임 지상) + 술어가 켜진 프레임에 출발 `_Starts`·급선회 `_F_Turns`·피벗 `_Pivots`·스핀 `_Spins`·착지 `_Lands_*`, Run은 보행이 바뀐 프레임에 `Run_F_GaitTransitions` |
| 지상·Moving·Sprint | `PSD_Stand_Sprint_{Loops,Starts,Turns,Lands_*}_UEFN` |
| 공중 | `PSD_Jump_UEFN`(MM 노드는 공중에서 꺼져 있다, §5.5) |

전환 행(2026-10-20 AnimationOverhaul 2-4)은 GASP 5.8 중첩 테이블을 평면으로 펼친 것이다. 피벗·스핀은 `MovementDirection_Recent` 비트마스크("최근 방향 ≠ 지금 방향"), 착지는 지상 첫 프레임의 `bJustLanded_Light/Heavy`로 고른다 — 공중 `Fall_Loop` 고정(§5.5)은 그대로다. 행 정의 원본은 `Scripts/AnimOverhaul/BuildLocomotionChooser_2-4.py`, GASP 대응은 [AnimationOverhaul design/GASPReference.md](AnimationOverhaul/design/GASPReference.md) §2.11.

- 스키마는 GASP 복제다(루프 `PSS_Relaxed_Loops_UEFN`, Idle `PSS_Relaxed_Idle_UEFN`, 전환용 Starts·Stops·StandTurn·Pivots·WalkSpins·RunSpins·Jump). 정규화는 `PSN_Relaxed_UEFN` 하나(DB 51개)다.
- `OnUpdate_MotionMatching`은 `ULNPAnimInstance::UpdateMotionMatchingNode`만 호출한다. 이 함수가 Chooser를 멀티 결과로 평가해 `SetDatabasesToSearch`를 부르고, `GetMMInterruptMode`(이동 모드 변화·이동 중 방향 변화·출발 프레임·정지 중 제자리 회전 요청이면 `InterruptOnDatabaseChange`)·`GetMMBlendTime`(지상 0.5s, 착지 0.2s)을 적용한다.
- **출발 프레임·제자리 회전 인터럽트는 LootNPop 추가다.** 진행 포즈는 자기 DB 스키마로 평가되는데(`DoNotInterrupt`여도 후보에 남는다) Idle 스키마는 궤적 가중치가 0이라, 끊지 않으면 움직이거나 돌아도 Idle 포즈가 Starts·TurnInPlace 후보를 이긴다. PIE 증상: "앞으로 걷는데 Idle 포즈로 미끄러짐"(Starts DB가 있어도), "제자리 회전이 깜박이며 뚝 돎"(술어 96회에 회전 DB 4회). 제자리 회전 조건은 "Idle·`bShouldTurnInPlace`·지금 DB에 `TurnInPlace` 태그 없음"이라 회전 클립이 시작되면 다시 끊지 않는다.
- 대각 입력은 가장 가까운 방향 DB가 고른다. 허리를 이동 방향으로 돌리는 Orientation Warping은 AnimationOverhaul 2-3.

### 5.4 이동 입력 차단

`OnProduceInput()`은 ASC에 `LNP.Block.MovementInput`이 있으면 **이동 벡터만 0으로** 만든다. `OrientationIntent`·`ControlRotation`은 유지되어 회전·조준은 계속 동작한다.

### 5.5 공중 포즈 고정 — 모션매칭 우회

공중(`bIsOnGround == false`)에서는 모션매칭 출력 대신 `MM_Unarmed_Jump_Fall_Loop`를 Sequence Player로 루프 재생한다. 모션매칭 노드 바로 뒤의 Blend Poses by Bool(블렌드 0.25s)이 고르므로 무기 오버레이·슬롯·포즈 히스토리는 그대로 얹힌다 — 활공·그래플 중 사격·가드 자세도 유지된다.

**왜 모션매칭에 맡기지 않는가.** 공중에서 Chooser(`CHT_PSD_Lyra`)는 `PSD_Jump_Lyra`(Start·Start_Loop·Apex·Fall_Loop·Fall_Land)를 고르는데, 공용 스키마 `PSS_CoreLocomotion_Lyra`로는 이 DB 안의 애니메이션들이 서로 구분되지 않는다.

| 원인 | 결과 |
|:---|:---|
| 궤적 채널(가중치 7)의 샘플 플래그가 전부 `PositionXY`·`VelocityXY`·`FacingDirectionXY` — **수직 성분이 없다** | 상승·정점·하강·활공이 질의에서 똑같다. 착지 임박도 알 수 없어 Fall_Land가 공중에서 아무 때나 뽑힌다 |
| 점프 애니메이션은 거의 제자리 동작 | 궤적 비용이 후보 전부에서 비슷하게 커서, 순위를 발 위치·속도(가중치 1)만이 가른다 |
| 정규화 세트가 조깅 DB와 함께 정규화, `ContinuingPoseCostBias` -0.01 | 후보 간 비용 차가 노이즈 수준이라 유지 보너스로 못 버틴다 |
| 전환마다 0.2s 블렌드 → 블렌드된 발 자세가 포즈 히스토리로 되먹임, `PoseReselectHistory` 0.3s | 어느 포즈와도 안 맞는 질의가 계속 만들어져 후보를 돌아가며 고른다 = **다리 떨림** |

자유낙하는 체공이 짧아 묻혔지만, 활공(수평 1200cm/s로 궤적 질의가 DB에서 멀다)·그래플처럼 체공이 긴 기믹에서 두드러졌다. Rewind/`a.AnimNode.MotionMatching.DebugDrawInfo 1`로 Current Asset이 Fall_Loop·Apex·Fall_Land 사이를 쉴 새 없이 오가는 것을 확인했다(2026-10-14).

**`bIsOnGround`를 쓰는 이유:** Chooser가 점프 DB로 넘어가는 조건과 같은 변수라, 모션매칭이 점프 DB를 쓸 모든 상황을 빠짐없이 대체한다. 기믹별 상태(`IsGliding`·`IsGrappleFlying`…)를 나열하지 않으므로 새 공중 기믹도 따로 처리할 필요가 없다.

**감수한 대가:** 점프 도약(Jump_Start·Apex)이 사라져 점프 직후 바로 Fall_Loop로 블렌드된다. 보완이 필요하면 점프 시작을 몽타주로 재생한다. 하강 중에만 고정하는 안(수직 속도 < 0)도 검토했으나, 그래플·런처처럼 상승 구간이 긴 기믹에는 떨림이 남을 수 있고 앞으로 생길 공중 기믹마다 조건을 따져야 해서 공중 전체로 정했다.

> 디버그 CVar: `a.AnimNode.MotionMatching.DebugDrawInfo 1`(+`DebugDrawInfoVerbose 1`) — 캐릭터 옆에 현재 에셋·비용·블렌드 스택. 공중에선 모션매칭 노드가 비활성이라 표시가 꺼지는 것이 정상이다.

---

## 6. GAS 연동

### 6.1 Chooser 기반 몽타주 선택

상황별 몽타주(공격·대시·히트리액트·패링 성공·경직·재장전)는 단일 Chooser Table `CHT_Montage`에서 고른다.

- `EvaluateMontage(Situation, Value)` → 현재 표현 세트 태그를 WeaponType으로 채워 평가. `PlayMontage()`는 평가 + `Montage_Play`.
- 열 구성: WeaponType / SituationType / Value (각각 Has Tag) → `UAnimMontage*`. 조건-몽타주 매핑은 에디터에서만 관리.
- Context 오브젝트는 캐릭터당 1개(`MontageCtx`, BeginPlay 생성)를 재사용.
- ⚠️ Context의 프로퍼티 이름(`WeaponType` 등)은 `CHT_Montage` 컬럼 바인딩이 참조하므로 바꾸지 않는다.

### 6.2 콤보 시스템 — ComboWindow 태그 소비 방식

단일 몽타주의 섹션(`Section_N`) 분기로 다단 콤보를 구현한다.

```
공격 입력 → TryActivateAttack()
├─ TAG_State_Staggered 있음 → false (경직 중에는 어떤 경로로도 공격 불가)
├─ TAG_State_ComboWindow 있음 → 태그 즉시 소비 + IncrementComboIndex
│     + Server_SetComboIndex(원격 클라) + 현재 공격 어빌리티 취소 + 재발동 → 다음 섹션
├─ TAG_Block_AttackInput 있음 → false (선딜 중 연타 차단)
└─ 그 외 → ResetCombo 후 첫 공격 발동 (발동 실패 시 인덱스 원복)
```

- **`Server_SetComboIndex`:** 인덱스는 입력 머신에서만 갱신되는데 서버 어빌리티가 서버 인덱스로 섹션을 고른다. 없으면 서버·관전자는 항상 1타만 재생한다.
- ⚠️ 경직 검사는 콤보 창 분기보다 **앞**이다. 콤보 창 분기는 `Block.AttackInput`을 보지 않아, 경직 진입 프레임에 창 태그가 남으면 차단을 건너뛴다.
- ⚠️ 리셋 RPC는 `TryActivateAttack_Impl()` **앞**에서 보낸다. 뒤면 서버가 활성화를 먼저 처리해 낡은 인덱스로 재생한다.
- 발동 실패 시 인덱스를 원복한다. 안 하면 블락↔콤보 창 사이 1프레임에 0으로 떨어져 2타만 무한 반복된다.

### 6.3 ANS 5종 — 몽타주 타임라인 구간 제어

근접 공격 어빌리티의 수명은 몽타주 재생과 같지만, 구간별(선딜·히트·콤보 창) 차단은 몽타주 시각에 묶여야 하므로 ANS를 타임라인에 중첩 배치한다. 에디터 작업만으로 구간 길이를 조절한다. 재생 속도는 `AttackSpeed` 어트리뷰트라 ANS 구간도 함께 압축된다(의도된 동작).

| 구간 | ANS | 동작 |
|------|-----|------|
| 선딜 | `ANS_LNPBlockMovementInput` | `Block.MovementInput` 추가/제거 |
| 히트 | `ANS_LNPMeleeHitWindow` | Mass 엔티티 생성 → 매 Tick 칼날 본(`sword_tip`/`sword_root`) 위치 기록 → 종료 시 파괴 (§7.1) |
| 입력 차단 | `ANS_LNPAttackInputBlock` | `Block.AttackInput` 추가/제거 — 선딜 연타 차단 |
| 콤보 창 | `ANS_LNPComboWindow` | `State.ComboWindow` 추가/제거 — §6.2의 소비 대상 |
| 후딜 | `ANS_LNPCancelMontageOnMovement` | NotifyTick: 이동 입력 감지 시 `Montage_Stop(BlendOutTime)` |

UE는 `Montage_Stop` 취소 시에도 활성 ANS에 `NotifyEnd`를 보장한다. 그래도 어빌리티 종료 콜백의 `ClearRelativeTag()`가 `Block.AttackInput`·`State.ComboWindow`를 한 번 더 걷어낸다(안전망).

### 6.4 히트리액트

`PlayHitReact(HitFromWorldDir, ImpactPoint, Damage, bMelee)`는 공격 반대 방향을 캐릭터 Up의 접평면에 투영하고 `VisualMesh` 표시 애님 인스턴스에 물리 반응을 예약한다. Phase 3-2에서 가벼운 피격의 4방향 태그 분류·HitReaction 몽타주 호출을 제거했다. `GameplayCue.LNP.Character.HitReact` 노티파이가 물리 반응과 피격자 HitStop(0.08초)을 함께 재생한다. 경직(Light·Heavy·Parried)은 별도 몽타주 경로를 유지한다. 근접 공격자 쪽 HitStop(본인 0.2초 / 구경꾼 0.08초)은 별도 경로다(→ [TechDesign_HitDetection.md](TechDesign_HitDetection.md)).

**큐 파라미터 규약** — 서버의 `FLNPApplyDamageGECommand`가 정한다: `Location` = 맞은 지점, `Normal` = 피격자 → 공격자, `RawMagnitude` = 피해, `NormalizedMagnitude` = 근접이면 1. 두 크기 필드는 0이면 직렬화되지 않는다. 큐를 낼지와 근접 여부는 `ELNPHitReactKind`(None = 화염·화상 틱처럼 큐 없음 / Ranged / Melee)다.

**물리 히트리액트**(애니메이션 개선 Phase 3, 코스메틱·비복제 AN-004) — 두 표시 ABP(`ABP_Retarget`·`ABP_VisualCopyPose`, 부모 `ULNPPhysicalHitReactAnimInstance` : `ULNPAnimInstance`)의 왼손 IK 뒤·출력 앞 RigidBodyWithControl 노드가 피격 구간에만 알파를 올린다(유지 0.15초 + 페이드 0.35초, 애님 시간이라 HitStop만큼 늘어난다. 상시 켜면 이동만으로 상체가 출렁인다). 골반·다리는 키네마틱, 상체는 30Hz 부모 공간 모터로 애니 포즈에 복귀하고, 중력은 끈다(구면). 세기 = `clamp(피해/20, 0.5, 2) × (근접 1.5)`. CVar `LNP.Anim.PhysHitReactMode` 기본값은 **100 = 지점 임펄스(4) + 골반 밀림(32) + 척추만 강도 낮추기(64)**다. 사용자 비교에서 선택한 116에서 몽타주 생략 비트 16만 뺀 동일 물리 구성이다. 진단용 1(가슴 선속도)·2(젖힘)·8(온몸 강도 낮추기, 64보다 우선)은 유지한다. 이전 비트 16은 효과가 없고, 0은 물리 반응을 끈다. 랙돌 중에는 알파 0이고 그 사이 온 피격은 버린다(표현 풀 재사용 대비). 설정·비교 근거는 AnimationOverhaul `research/PhysicsHitReactSpike.md`.

골반은 RBWC `KinematicTargets`로 월드 오프셋을 준다. 기준 세기에서 이동 3cm·젖힘 6°·피격 지점 좌우에 따른 비틀림 7.5°, 애님 시간 0.1초에 정점이다(2026-10-23 사용자 과장 완화 요청으로 종전 4cm·8°·10°보다 25% 감소, 빌드·실측·사용자 선호 판정 통과). 회전 축은 캐릭터 Up·밀림 방향으로 계산해 구면에서도 동작한다. 척추 강도는 피격 구간에 0.1배로 낮추고 복귀시키며, 메시별 PhysicsAsset의 실제 바디·부모 관계가 맞는 컨트롤만 공급한다(없는 이름은 경고를 낸다). 캡슐·Mover는 움직이지 않는다. 왼손 IK의 기존 차단 조건은 `DefaultSlot`·`FullBody`·`UpperBody`와 `DisableLHandIK` 커브이며, 제거한 가산 HitReact 몽타주는 원래 이 슬롯 차단 대상이 아니었다. 물리는 기존대로 왼손 IK 뒤에서 적용한다.

**히트리액트 몽타주에는 입력 차단 ANS를 붙이지 않는다** (2026-08-29 결정). 행동을 끊는 판단은 경직 시스템(→ [TechDesign_Ability.md §2.5](TechDesign_Ability.md))이 하고, 히트리액트는 코스메틱이다.

**플레이어 근접 공격 중 물리 반응 억제**(AN-024, 2026-10-22 사용자 결정). `ANS_LNPMeleeHitWindow`는 실제 `WeaponMesh`의 칼끝·뿌리 위치를 읽으므로 표시 포즈에 얹은 물리가 판정 궤적에도 섞인다. `ULNPPhysicalHitReactAnimInstance`는 소유자가 `ALNPPlayerCharacter`이고 메인 인스턴스에 `ANS_LNPMeleeHitWindow`를 포함한 몽타주가 남아 있으면 반응을 억제한다. 준비·회수·블렌드아웃까지 포함하고, 이미 시작된 반응도 즉시 알파 0으로 종료하며 그동안의 피격 예약은 버린다. 공격이 끝난 뒤 늦게 재생하지 않는다. 로컬 제어 여부나 비복제 태그에 의존하지 않아 서버·소유 클라·시뮬레이티드 프록시 모두 같은 기준을 쓴다. NPC는 이 제한 없이 공격 중 물리를 허용하며, 그에 따른 NPC 무기 궤적 흔들림은 사용자가 허용한 예외다. 경직 몽타주·HitStop은 그대로다.

진단은 기본 꺼짐인 `LNP.Anim.PhysHitReactLog`로 피격·억제·메인 몽타주를 확인한다. Phase 3 검증에 쓴 매 평가 포즈 계측 훅·`PhysHitReactMeasure`는 결과 보존 후 제거했다. 실제 바디 목록·측정 방법·수치·증거 한계는 AnimationOverhaul `research/PhysicsHitReactSpike.md` §10에 있다.

1. 차단 ANS를 달면 **모든 피격이 곧 경직**이 되어 누적·임계 체계가 무의미해진다.
2. 히트리액트는 각 머신이 큐를 받아 로컬 재생하고 `AddLooseGameplayTag`는 복제되지 않는다. 차단 시점이 머신마다 RTT만큼 갈린다.
3. 다음 피격·공격의 `Montage_Stop`이 Begin/End 페어를 깰 여지가 남는다.

### 6.5 경직 — ANS가 아니라 어빌리티가 구간을 소유한다

경직은 지속이 본질이므로 어빌리티를 살려 둔다. `ULNPAbility_Stagger`가 `ActivationOwnedTags`로 `Block.AttackInput` · `Block.MovementInput` · `State.Staggered`를 소유한다. 어빌리티 수명 = 차단 구간이라 태그 페어가 깨질 수 없고, 서버 권위이며, 소유 클라이언트에는 GAS 활성화 복제로 전달된다.

- **그로기(Light):** 지속 시간이 없다. 경직도가 자연회복으로 임계 아래로 내려가면 `ULNPPoiseProcessor`가 취소한다.
- **다운(Heavy):** `UAbilityTask_WaitDelay`로 `ULNPSettings::PoiseDownLockSeconds` 동안 유지.
- 경직은 재장전을 취소한다(`CancelAbilitiesWithTag`). 상세는 → [TechDesign_Poise.md](TechDesign_Poise.md).

경직 몽타주에는 ANS를 배치하지 않는다 — 몽타주는 `GameplayCue`가 재생하는 코스메틱이다.

### 6.6 근접 공격 타겟 보정

조준이 살짝 빗나갔을 때 허공을 베지 않도록 공격 초반에 타겟 쪽으로 끌어당긴다. **플레이어 전용**이다(`ULNPLockOnComponent` 보유로 판별) — 적 NPC는 StateTree 스티어링이 접근 거리를 맞춘다.
**위치는 Motion Warping 또는 LayeredMove가, 회전은 항상 `OrientationIntent`가 담당한다.**

| 상황 | 위치 보정 | 회전 보정 |
|:---|:---:|:---:|
| 락온 ON | O (락온 타겟) | X (카메라가 이미 추적) |
| 락온 OFF · 정지 | O (전방 탐색 타겟) | O |
| 이동 인풋 중 | **X** | O |

회전 보정은 `MoveInput`을 건드리지 않고 `OrientationIntent`만 덮어쓰므로 이동 중에도 공존한다. 보정은 `EndAbility`의 `ClearMeleeAssist()`가 해제한다 — 남으면 캐릭터가 타겟을 바라본 채 굳는다.

**입력 출처:** 대상 좌표·락온 여부·이동 입력 여부는 소유 클라이언트가 **공격을 누른 순간** `CaptureMeleeAssistInput`으로 찍어 발동 요청에 싣는 `FLNPMeleeAssistTargetData`에서만 읽는다(사유 §7.5④). 없으면 보정하지 않는다.

**타겟 선정** — 락온 중이면 그 타겟. 꺼져 있으면 `ULNPTargetQuerySubsystem`의 상시 원뿔 질의(소유 클라이언트에서만 등록)가 **캐릭터 전방 기준** 점수로 고른다. 서버는 탐색하지 않는다.

```
점수 = AngleWeight x (1 - 각도/최대각) + DistanceWeight x (1 - 거리/탐색반경)
```

- 거리·각도는 모두 **접평면 성분**으로 잰다(§7.4와 같은 규약).
- 기본값(각도 0.6 / 거리 0.4)에서는 정중앙의 적(최소 0.6점)이 경계의 적(최대 0.4점)을 항상 이긴다. 각도가 사실상 1순위, 거리는 타이브레이커다. 그래서 탐색각을 넓혀도 옆의 적이 정면 적을 가로채지 않는다.
- ⚠️ `MeleeAssistMaxSearchAngleDeg`(기본 75)는 **정면으로부터의 이탈각**이다.
- ⚠️ **시야 차단 검사가 없다.** 위치 보정은 막히면 그냥 안 움직이지만 회전 보정은 벽 너머 적을 향해 몸을 돌린다.

**보정량** — 이미 가까우면 0, 멀수록 커지되 상한에서 잘린다. 무기의 `MeleeIdealDistance`가 0이면 위치 보정을 쓰지 않는다.

```
Gap  = max(0, 접평면거리 - 무기의 MeleeIdealDistance)
보정 = min(Gap x Strength, MeleeAssistMaxCorrectionDistance)
```

#### 경로 선택 — 애니메이션에 루트모션 이동량이 있는가

섹션 내 이동량 최대 구간의 루트모션이 `MeleeAssistMinWindowRootMotion`(10cm) 이상이면 Motion Warping, 미만이면 LayeredMove. 기준은 **"워프할 원본이 있는가"**다 — 루트모션은 애니메이터가 의도한 이동이므로 있으면 스케일하고, in-place면 코드가 속도를 얹는다. 기준을 낮추면 안 되는 이유는 §7.5①.

> 2026-08-30 실측: 현 검술 세트(`AM_SW_Attack_01`)는 `bEnableRootMotion = true`지만 두 섹션 모두 이동량 0.00cm라 항상 LayeredMove로 간다.

#### 경로 A — LayeredMove (in-place 애니메이션)

- 구간: **[섹션 시작, 그 섹션의 첫 `ANS_LNPMeleeHitWindow` 시작]** — 선딜 동안 끌려가 칼날이 살아날 때 정렬이 끝난다. 히트 윈도우가 없으면 `MeleeAssistWarpWindowSeconds`로 폴백.
- `FLayeredMove_LinearVelocity`(대시와 같은 패턴)로 `타겟 방향 x 속도`를 그 시간만큼 더한다.
- 요구 속도가 `MeleeAssistMaxPullSpeed`를 넘으면 **속도를 지키고 덜 당긴다.** 총 이동량이 보정 거리를 넘을 수 없고, 막히면 Mover가 충돌을 풀며 그냥 안 움직인다.

#### 경로 B — Motion Warping (루트모션이 실린 애니메이션)

- 구간: 섹션 안에서 루트모션 순 이동량이 최대인 창을 1/30초 스텝으로 찾는다. 결과는 (몽타주, 섹션, 창 길이)로 캐시.
- 몽타주 에셋은 수정하지 않는다. `ULNPMeleeAssistWarpModifier`(`URootMotionModifier_SkewWarp` 파생 — protected인 `MaxSpeedClampRatio`를 넣기 위한 최소 클래스)를 `AddModifier`로 런타임 등록한다. 같은 (몽타주, 창)이 이미 있으면 재사용한다 — 레이어드 무브가 창보다 먼저 끝나 엔진의 모디파이어 제거 조건이 성립하지 않아 공격마다 쌓이기 때문이다.
- 루트모션이 Mover를 통과해야 워프가 동작하므로 `FLayeredMove_AnimRootMotion`을 함께 큐잉한다.
- ⚠️ 레이어드 무브의 시작점은 창이 아니라 **섹션 시작**이다. 창 시작을 넣으면 스윙 구간의 루트모션이 선딜에 적용된다. 창 밖은 모디파이어가 비활성이라 원본 루트모션이 그대로 통과한다.

두 경로 모두 `MixMode = AdditiveVelocity`다(Override면 보정이 이동을 통째로 대체). 보정 대상이 없으면 아무것도 큐잉하지 않아 평소 공격 이동은 그대로다.

**컴포넌트 배선:** `ALNPCharacterBase`가 `UMotionWarpingComponent`를 생성하면 `UMoverComponent::InitializeComponent`가 `UMotionWarpingMoverAdapter`를 자동으로 붙인다. UE 5.8 Mover 2.0은 `ACharacter` 없이도 모션 워핑을 지원한다.

**튜닝:** `ULNPSettings`의 `Combat|Melee Assist` 섹션(기본 강도 0.5, 최대 보정 150cm, 탐색 반경 300cm). CVar `LNP.Melee.Assist.Strength`(음수면 설정값) / `ForceMode`(-1 자동, 0 Pull, 1 Warp) / `Debug`. 강도 읽기는 `LNPMeleeAssist::GetStrength()` 한 곳이다.

---

## 7. 어필 포인트 (트러블슈팅 & 엔진 분석)

### 7.1 UAnimNotifyState는 싱글턴이다 — 멀티플레이 근접 판정 실패 버그

ANS는 몽타주 에셋에 배치된 **단일 오브젝트**를 그 몽타주를 재생하는 모든 AnimInstance(서버·클라이언트·여러 캐릭터)가 공유한다. 스윙의 엔티티 핸들을 평범한 멤버에 저장하던 초기 구현은 PIE 2인 PvP에서 두 스윙이 겹치는 순간 서로 덮어써 판정이 실패했다.

**해결:** `TMap<TWeakObjectPtr<USkeletalMeshComponent>, FActiveSwing>`으로 MeshComp별 상태를 분리. AnimNotify 계열에 상태를 둘 때의 원칙으로 헤더에 명문화했다.

### 7.2 Deferred BuildEntity + AddTag의 아키타입 전환 함정

히트 윈도우 엔티티를 `BuildEntity`와 `AddTag`로 같은 디퍼드 배치에 넣으면 아키타입 전환 타이밍 때문에 같은 프레임 쿼리가 엔티티를 놓쳤다. **Fragment(`FLNPWeaponTraceFragment`)의 존재 자체를 "활성 공격 윈도우" 신호로** 쓰고 Tag를 쓰지 않는다.

### 7.3 몽타주 취소 안전망 — TimeToLive

`NotifyEnd`가 누락될 때(액터 파괴 등)를 대비해 Fragment에 `TimeToLive = TotalDuration + 0.2s`를 심고, `LNPWeaponTraceProcessors`의 Lifetime 프로세서가 만료 엔티티를 파괴한다. ANS 수명과 Mass 수명의 이중 안전장치다.

### 7.4 구형 월드에서의 애니메이션 수치 보정

- `GroundSpeed`: `Size2D()`는 월드 XY 평면 전제 → 캐릭터 Up에 수직인 평면으로 `VectorPlaneProject`해 계산.
- Aim Offset: §4.1의 로컬 좌표계 변환.
- 이동 방향 `Direction`: 엔진 `UKismetAnimationLibrary::CalculateDirection`은 캐릭터 회전 기준이지만 속도를 `GetSafeNormal2D`(월드 Z 제거)로 정규화해 **적도에서 틀린 방향을 낸다**. 캐릭터 Up 둘레 부호각(전방 → 속도)으로 직접 계산한다(2026-10-19 AnimationOverhaul 2-2, 방향별 DB 선택에서 드러남).
- Offset Root Bone(엔진 `AnimNode_OffsetRootBone`): ① 지상이면 가상 루트 위치를 `GroundNormal`(기본 월드 Z) 평면에 투영 → 컴포넌트 Up을 핀으로 준다. ② `bClampToTranslationVelocity`는 보정 방향을 `GetSafeNormal2D`로 만든다 → 켜지 않는다. ③ Accumulate·Interpolate 모드는 캡슐 회전 변화를 가상 루트에 더하지 않아 구면을 걸으면 메시가 기운다 → 매 프레임 가상 루트 Up을 컴포넌트 Up으로 스윙 정렬. Steering 목표 facing도 같은 정렬을 한다(2026-10-20 AnimationOverhaul 2-3). ④ `MaxRotationError` 클램프는 오차를 컴포넌트 로컬로 구한 뒤 월드 쪽에 곱해, 적도에서 빠르게 돌면 가상 루트가 최대 90° 기운다 → 노드 값은 −1, 같은 90° 제한을 C++가 Up 정렬 직후 컴포넌트 Up 둘레로 건다(2-5).
- 발 배치 CR: GASP 원본은 루트 높이 감쇠·발 고정에서 월드 `.Z`·XY 성분을 직접 쓴다 → 컴포넌트 Up 방향 지연량·리그 공간 섞기로 고쳤다. 접지 트레이스·평면 보정은 리그 공간이라 그대로다(2-5).
- Offset Root Bone `MaxRotationError`: 엔진 클램프가 로컬 오차를 월드 쪽에 곱해 적도에서 가상 루트를 최대 90° 기울인다 → 노드 값 −1, C++가 컴포넌트 Up 둘레로 90° 제한(2-5).
- Additive Leans: GASP는 Lean 축을 `RotationFromXVector(Velocity)`(월드 Z 기준)로 만든다 → 측면 방향을 `Up × 접평면 속도`로 계산한다(2-1·2-6).

### 7.5 Motion Warping을 구면 월드·Mover·멀티플레이에 얹을 때의 함정 4건

**① 루트모션이 없는 구간에 워프를 걸면 캐릭터가 날아간다.**
`URootMotionModifier_SkewWarp::ProcessRootMotion`은 창 이동량이 `2e-4` 미만이면 `StartTransform`→타겟 Lerp 경로로 빠지는데, **그쪽에는 `MaxSpeedClampRatio` 클램프가 없다.** 막히면 보정 거리 전체가 한 프레임 속도(≈9,000cm/s)가 되어 Falling으로 튕겨나간다(2026-08-30 실측).
→ 워프 창을 "이동량 최대 구간"으로 고르고 기준 미달이면 워프하지 않는다(§6.6). 선딜 구간 창은 검 공격에서 오히려 이동량이 가장 없는 구간이었다.

**② 기준점은 `GetVisualRootLocation()`(발밑)이고, `bIgnoreZAxis`는 월드 Z 기준이다.**
워프 타겟을 `GetActorLocation()`(캡슐 중심)으로 넘기면 캡슐 반높이만큼 캐릭터를 위로 밀어 올린다.
→ 타겟을 `GetPrimaryVisualComponent()` 위치 기준으로 잡고 `bWarpToFeetLocation = true`로 짝을 맞춘다. `bIgnoreZAxis`는 구면에서 쓸 수 없으므로(적도에서 월드 Z는 수평) 끄고 접평면 투영을 직접 한다.

**③ 회전 워프(`bWarpRotation`)는 이동 모드가 즉시 되감는다.**
레이어드 무브의 `AngularVelocityDegrees`는 이동 모드 제안에 더해질 뿐인데, 이동 모드가 매 프레임 `OrientationIntent`를 향해 되돌린다.
→ 회전 보정은 `ULNPInputHandlerComponent::SetMeleeAssistOrientation`으로 `OrientationIntent`를 덮어쓴다. InputCmd 필드라 복제·롤백이 따라오고, "시뮬레이션 입력은 InputCmd를 탄다" 규약(`TechDesign_CharacterMovement.md` §7.1)도 지켜진다.

**④ 보정 입력은 소유 클라이언트가 만들어 발동 요청에 싣는다.**
- 컴포넌트 로컬 상태: 서버가 원격 클라이언트의 락온을 몰라 **다른 적**을 고른다. 락온은 "이 적을 치겠다"는 의사라 정확히 반대로 작동한다.
- 서버 자체 탐색: 권위 현재 위치와 게스트의 보간된 과거 위치가 달라 목적지가 갈린다.
- InputCmd(`FLNPModifierInputs`, 과거 방식): 서버의 `GetLastInputCmd()`가 입력 버퍼만큼 과거라 목적지가 최대 341cm 갈리고, 서버만 "이동 입력 중"으로 보는 스윙이 13/45였다.
→ `FLNPMeleeAssistTargetData`로 옮긴 뒤 목적지 차이는 최대 0.81cm(양자화)다 → [TechDesign_Networking.md](TechDesign_Networking.md) §4.8.

**남은 차이 — 공격자 자신의 위치.** 서버의 원격 폰은 뒤에서, 게스트는 예측으로 앞에서 시뮬레이션되어 `dist`가 서버에서 5~25cm 크다. 보정량 차이는 대부분 수 cm이고 이상 거리 경계에서만 "게스트는 회전만 / 서버는 위치 보정"으로 갈린다(3/58). 리컨실리에이션이 메우는 크기라 **수용한다.** (필요하면 보정 벡터 자체를 스냅샷에 싣는다 — 대신 클라이언트가 변위를 지시하게 된다.)
⚠️ 2P 로그 대조는 액터 이름이 아니라 `target`·`dist` 수치와 시각으로 한다 — 복제 액터 인스턴스 이름은 머신마다 다르다.

---

## 8. 미구현 / 잔여 작업

- **근접 보정 강도의 사용자 환경설정 이전:** 환경설정 UI가 생기면 `LNPMeleeAssist::GetStrength()`의 출처만 바꾼다.
- **루트모션 공격 모션 확보 시:** 이동량이 실린 모션을 넣으면 §6.6 경로 선택이 자동으로 Motion Warping을 탄다. 코드는 이미 있다.
- **근접 보정 시야 차단 검사(선택):** 라인 트레이스 한 번으로 벽 너머 회전 보정을 막을 수 있다.
- **원거리 무기 왼손 IK:** `ABP_Sub_Rifle`·`ABP_Sub_Shotgun`에 Two Bone IK 노드 추가(§4.2).
- **락온 조준 모드 제한:** "LockOn은 AimMode None일 때만" 설계는 미구현이다 — 원거리 무기로도 락온이 걸린다(§5.1).
- **`ABP_Sub_LongSword` Look At:** 락온 중 neck_01·head가 타겟을 미세 추적하는 Bone Control. 현재는 캐릭터 전체 회전(§5.2)으로 대체 — 필요성 재평가 후 적용.
- **이동 중 상체 공격:** `UpperBody` 슬롯은 재장전용으로 생겼지만 근접 공격 몽타주는 `DefaultSlot` 전신이다. 상체 분리는 필요 시 도입.
- **Guard/Parry GameplayCue 에셋:** 태그·코드 경로는 완성, VFX/SFX 에셋 연결이 에디터 잔여 작업.
- **넉다운·기상 몽타주:** 경직 Chooser 행(`Situation.Stagger` × `Value.Stagger.Light/Heavy/Parried`)은 배선 완료. 현재 Heavy는 `AM_MM_HitReact_Front_Hvy_01` — 넉다운 몽타주를 확보하면 이 행의 에셋만 교체한다(→ [TechDesign_Poise.md](TechDesign_Poise.md)).
- **그로기 루프 포즈:** 그로기는 가변 길이인데 몽타주는 원샷이라 긴 그로기에서 남은 시간이 idle 포즈로 굳는다.
