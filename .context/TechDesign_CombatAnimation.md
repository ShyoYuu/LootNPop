# 컴뱃 애니메이션 시스템 기술 설계

## 1. 한눈에 보기

Motion Matching 공용 로코모션 위에 **무기별 Linked Anim Layer를 런타임 교체**한다. 메인 AnimBP는 직선 파이프라인만 두고, 무기별 블렌딩은 전부 서브 ABP에 위임한다.

```
[ALNPCharacterBase::ApplyWeaponVisuals()]
       │  표현 세트·조준 모드 태그 전환 + LinkAnimClassLayers() + 무기 메시 어태치
       ▼
[ABP_Lyra — 메인 AnimBP (AnimInstance = ULNPAnimInstance)]
   Motion Matching (PSD 전신 포즈) + Pose History Collector
       │
   Linked Anim Layer (ALI_WeaponStyles::ApplyWeaponStyleOverlay)
       │   └─ ABP_Sub_Unarmed / Pistol / Rifle / Shotgun / LongSword 런타임 교체
       │
   Slot 'DefaultSlot' (검술 공격·피격 몽타주)
       │
   Slot 'FullBody' (대시 — Lyra 몽타주가 쓰는 슬롯, 2026-09-16 추가)
       │
   Slot 'UpperBody' (재장전 — TechDesign_Ability.md §5.5)
       │
   Output Pose
```

### 무기별 이동·조작 방식

| 상황 | 조준 모드 | 회전 방식 | 이동 방식 |
|------|----------|----------|----------|
| 맨손 / 롱소드 (락온 해제) | None | 입력 방향으로 캐릭터 회전 | 항상 앞으로 이동 |
| 원거리 무기 | FreeAim | 카메라 정면 고정, 조준점 상시 표시 | 전후좌우 Strafe |
| 락온 중 (무기 무관) | — (컴포넌트 상태) | 타깃 방향 고정 (`ULNPLockOnComponent`) | 전후좌우 Strafe |

### 핵심 클래스

| 클래스 | 역할 |
|:---|:---|
| `ULNPAnimInstance` | Mover 상태 → ABP 변수 공급 (속도·방향·Guard·Aim Offset·왼손 IK 타겟) |
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
| `ABP_Lyra` | Main AnimBP | Motion Matching + 레이어 합성 파이프라인 (§1) |
| `ALI_WeaponStyles` | Anim Layer Interface | `ApplyWeaponStyleOverlay` 레이어 함수 계약 |
| `ABP_Sub_Unarmed` | Sub AnimBP | 베이스 포즈 통과 |
| `ABP_Sub_Pistol` | Sub AnimBP | spine_01 이상 파지 포즈 + `AO_MM_Pistol_Idle_ADS` |
| `ABP_Sub_Rifle` | Sub AnimBP | spine_01 이상 파지 포즈 + `AO_MM_Rifle_Idle_ADS` |
| `ABP_Sub_Shotgun` | Sub AnimBP | Rifle과 동일 구성. 아이들만 `MM_Shotgun_Idle_ADS`, AO는 Rifle 것을 공용 |
| `ABP_Sub_LongSword` | Sub AnimBP | 롱소드 Stance + `bIsGuarding` Guard 자세 분기 + 왼손 Two Bone IK |

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

---

## 4. 오버레이 상세

### 4.1 원거리 무기 Aim Offset — 구형 월드 보정

AO 에셋은 서브 ABP가 적용하고, `AimYaw`/`AimPitch`는 `ULNPAnimInstance`가 계산한다.

"카메라 Yaw − 캐릭터 Yaw" 방식은 월드 Euler 뺄셈이라 캐릭터 Up이 World Up과 벌어지면 틀린다. 대신 `GetBaseAimRotation()`의 방향 벡터를 캐릭터 로컬 좌표계로 변환(`InverseTransformVectorNoScale`)한 뒤 Pitch(±90 클램프)·Yaw를 뽑는다.

**`GetBaseAimRotation()`의 공급원은 폰 종류마다 다르다.** 계산식은 그대로 두고 공급원만 갈아끼운다.

| 폰 | 공급원 |
|:---|:---|
| 로컬 제어 플레이어 | `Controller->GetControlRotation()` |
| 원격 플레이어·시뮬레이티드 프록시 | Mover InputCmd의 `ControlRotation` (복제됨, → [TechDesign_Networking.md](TechDesign_Networking.md)) |
| 적 NPC | 액터 전방 + 복제된 로컬 `AimPitchDeg` — 컨트롤러가 없어 `APawn` 기본값은 수평이다 (→ [TechDesign_EnemyNPC.md](TechDesign_EnemyNPC.md) §6) |

적 NPC도 같은 `ABP_Lyra`·서브 ABP를 쓰고 AO 노드가 `AimPitch`/`AimYaw`에 바인딩돼 있으므로, 적 조준은 **애니메이션 에셋 수정 없이 `GetBaseAimRotation()` 하나로 끝난다.**

### 4.2 왼손 Two Bone IK

오른손은 무기 소켓 어태치로 정확하지만 왼손은 애니메이션 그대로라 그립에서 어긋난다.

- 무기 메시에 `LeftHandGrip` 소켓 배치.
- `ULNPAnimInstance`가 매 프레임 소켓 월드 위치(`LeftHandGripLocation`)와 존재 여부(`bHasLeftHandGrip`)를 갱신.
- 서브 ABP의 Two Bone IK 노드(`lowerarm_l → hand_l`)가 왼손을 끌어당긴다. 소켓이 없으면 IK 비활성.
- 현재 IK 노드는 **`ABP_Sub_LongSword`에만** 있다. Rifle·Shotgun 서브 ABP에는 없다(AnimInstance 변수는 준비돼 있어 노드만 추가하면 된다).

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

`UCharacterTrajectoryComponent`는 `ACharacter` 전용이라 쓰지 않는다. Mover 2.0의 `UMoverTrajectoryPredictor`가 `OrientationIntent` 기반으로 미래 경로를 예측해 PSD가 게걸음/뒷걸음 포즈를 고른다 — 별도 구현 없음.

### 5.4 이동 입력 차단

`OnProduceInput()`은 ASC에 `LNP.Block.MovementInput`이 있으면 **이동 벡터만 0으로** 만든다. `OrientationIntent`·`ControlRotation`은 유지되어 회전·조준은 계속 동작한다.

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

`PlayHitReact(HitFromWorldDir)`: 피격 방향을 로컬 공간으로 변환 → 4방향 태그 분류 → `PlayMontage(HitReaction, Direction)`. `GameplayCue.LNP.Character.HitReact` 노티파이가 몽타주와 피격자 HitStop(0.08초)을 함께 재생한다. 근접 공격자 쪽 HitStop(본인 0.2초 / 구경꾼 0.08초)은 별도 경로다(→ [TechDesign_HitDetection.md](TechDesign_HitDetection.md)).

**히트리액트 몽타주에는 입력 차단 ANS를 붙이지 않는다** (2026-08-29 결정). 행동을 끊는 판단은 경직 시스템(→ [TechDesign_Ability.md §2.5](TechDesign_Ability.md))이 하고, 히트리액트는 코스메틱이다.

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
- `CalculateDirection`은 캐릭터 회전 기준이라 그대로 유효.

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
