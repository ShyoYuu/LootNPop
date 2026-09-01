# 컴뱃 애니메이션 시스템 기술 설계

## 1. 한눈에 보기

Motion Matching 기반 공용 로코모션 위에 **무기별 Linked Anim Layer를 런타임 교체**하는 구조. 메인 AnimBP는 직선 파이프라인만 유지하고, 무기별 블렌딩 로직은 전부 서브 ABP로 위임한다.

```
[Character / EquipWeapon()]
       │  Gameplay Tag 전환 + LinkAnimClassLayers() + 무기 메시 어태치
       ▼
[ABP_BaseCharacter — 메인 AnimBP]
   Motion Matching (맨손 PSD 전신 포즈)
       │
   Linked Anim Layer (ALI_WeaponStyles::ApplyWeaponStyleOverlay)
       │   └─ ABP_Sub_Unarmed / Pistol / Rifle / Shotgun / LongSword 가 런타임 교체
       │      각 서브 ABP가 자체 본·가중치로 오버레이 (AO, Guard 분기, 왼손 IK)
       │
   Slot 'FullBody' (GAS 몽타주)
       │
   Pose History (Motion Matching 다음 프레임 쿼리용)
       │
   Output Pose
```

### 무기별 이동·조작 방식

| 무기 | 조준 모드 | 회전 방식 | 이동 방식 |
|------|----------|----------|----------|
| 맨손 / 롱소드 (비락온) | None | 입력 방향으로 캐릭터 회전 | 항상 앞으로 이동 |
| 피스톨 / 라이플 | FreeAim | 카메라 정면 고정, 조준점 상시 표시 | 전후좌우 Strafe |
| 롱소드 (락온 중) | LockOn | 타깃 방향 고정 (LockOnComponent) | 전후좌우 Strafe |

### 핵심 클래스

| 클래스 | 역할 |
|:---|:---|
| `ULNPAnimInstance` | Mover 상태 → ABP 변수 공급 (속도·방향·Guard·Aim Offset·왼손 IK 타겟) |
| `ALNPCharacterBase` | `EquipWeapon()` 레이어 교체, `EvaluateMontage()` Chooser 평가, 콤보 상태 관리 |
| `ULNPMontageChooserContext` | Chooser Table 평가 입력 (WeaponType / SituationType / Value 태그) |
| `UANS_LNP*` 5종 | 몽타주 타임라인 구간별 제어 (아래 §6.3) |

---

## 2. Gameplay Tags

| 분류 | 태그 | 설명 |
|------|------|------|
| 무기 | `LNP.Weapon.Unarmed / Pistol / Rifle / Shotgun / LongSword` | 장착 무기. `EquipWeapon()`이 전환 |
| 조준 | `LNP.AimMode.None / FreeAim / LockOn` | `ULNPWeaponData::DefaultAimMode`로 지정. LockOn 전환은 None일 때만 허용 |
| 액션 | `LNP.Action.Attacking` | 공격 애니메이션 재생 중 |
| 차단 | `LNP.Block.MovementInput` / `LNP.Block.AttackInput` | 입력 차단. 공격 몽타주 구간은 ANS가, 경직 구간은 GA_Stagger의 `ActivationOwnedTags`가 소유 (§6.5) |
| 콤보 | `LNP.State.ComboWindow` | 콤보 입력 수용 구간 (ANS_ComboWindow가 열고 TryActivateAttack이 소비) |
| Guard | `LNP.State.Guarding` / `LNP.State.ParryWindow` | InputHandler가 ASC에 유지/타이머 관리 |
| 경직 | `LNP.State.Staggered` | GA_Stagger가 어빌리티 수명 동안 소유. 가드·대시 입력 게이트와 재경직 차단에 쓰인다 |
| Mover | `LNP.Mover.IsSprinting` / `LNP.Mover.IsGuarding` | Modifier 활성 여부. `ULNPAnimInstance` 변수 소스 |

---

## 3. AnimBP 구조

### 3.1 에셋 구성

| 에셋 | 종류 | 내부 처리 |
|--------|------|------|
| `ABP_BaseCharacter` | Main AnimBP | Motion Matching + 레이어 합성 파이프라인 (§1 다이어그램) |
| `ALI_WeaponStyles` | Anim Layer Interface | `ApplyWeaponStyleOverlay` 레이어 함수 계약 |
| `ABP_Sub_Unarmed` | Sub AnimBP | 베이스 포즈 통과 |
| `ABP_Sub_Pistol` | Sub AnimBP | spine_01 이상 파지 포즈 + `AO_Pistol` |
| `ABP_Sub_Rifle` | Sub AnimBP | spine_01 이상 파지 포즈 + `AO_Rifle` + 왼손 Two Bone IK |
| `ABP_Sub_Shotgun` | Sub AnimBP | Rifle과 동일 구성. 아이들만 `MM_Shotgun_Idle_ADS`, Aim Offset은 `AO_MM_Rifle_Idle_ADS`를 공용 (§아래 주석) |
| `ABP_Sub_LongSword` | Sub AnimBP | 롱소드 Stance + `bIsGuarding` Guard 자세 분기 + 왼손 Two Bone IK |

**설계 원칙:** 어떤 본에 얼마나 블렌딩할지는 각 서브 ABP가 직접 결정한다. 메인 ABP는 무기를 모른다.

**Shotgun이 Rifle Aim Offset을 공용하는 이유:** Lyra 샘플에 샷건 전용 AO가 없다. 누락이 아니라
Epic도 같은 선택을 한 것으로, 원본 `ABP_ShotgunAnimLayers`는 `IsDataOnly=True`인 `ABP_RifleAnimLayers`의
자식이며 아이들 포즈만 교체한다. 기술적으로도 안전하다 — `AO_MM_Rifle_Idle_ADS`의 샘플은
`AAT_RotationOffsetMeshSpace`(메시 스페이스 회전 애디티브)이고 레퍼런스 포즈가 중앙 샘플 0프레임이라,
델타가 절대 회전으로 적용된다. 베이스 포즈가 Rifle 아이들에서 Shotgun 아이들로 바뀌어도 조준 추종 각도가
유지된다 (로컬 스페이스 애디티브였다면 어긋났을 것).

### 3.2 런타임 레이어 교체 (`EquipWeapon`)

```
ApplyWeaponVisuals(WeaponData)   ← 서버·클라이언트가 부르는 단일 진입점 (멱등)
├─ 조기 반환: 이미 같은 무기로 적용됐으면 아무것도 안 한다 (LinkAnimClassLayers 중복 = 포즈 튐)
├─ ASC: 기존 무기·조준모드 태그 제거 → 신규 태그 부여
├─ SetFaceMoveDirection(!bFreeAim)   — 회전 방식 전환
├─ AnimSourceMesh->LinkAnimClassLayers(WeaponData->AnimLayerClass)
└─ WeaponMesh: 메시 교체 + 소켓 어태치 + 상대 오프셋(WeaponMeshRelativeLocation/Rotation) 적용

호출자 — 무기 원본은 ULNPEquipmentComponent::WeaponSlot(플레이어) / EnemyConfig(적)이고 둘 다 복제된다:
├─ 푸시: 슬롯 적용 직후(서버) · OnRep_WeaponSlot / OnRep_EnemyConfig(클라)
└─ 풀:   Pawn의 BeginPlay · OnRep_PlayerState · PossessedBy (PlayerState↔Pawn 도착 순서 무관)
   상세는 TechDesign_Inventory.md §4.1
```

---

## 4. 오버레이 상세

### 4.1 원거리 무기 Aim Offset — 구형 월드 보정 (구현 완료)

AO 에셋은 서브 ABP 내부에서 적용하고, `AimYaw`/`AimPitch` 값은 `ULNPAnimInstance`가 계산한다.

일반적인 "카메라 Yaw − 캐릭터 Yaw" 방식은 **월드 Euler 뺄셈**이라서, 캐릭터 Up이 World Up과 벌어지는 구형 월드에서는 오차가 커진다. 에임 방향 벡터를 캐릭터 로컬 좌표계로 변환한 뒤 각도를 추출해 해결:

```cpp
FRotator BaseAimRotation = Character->GetBaseAimRotation();
FVector LocalAimDir = Character->GetActorTransform().InverseTransformVectorNoScale(BaseAimRotation.Vector());
AimPitch = FMath::ClampAngle(LocalAimDir.Rotation().Pitch, -90, 90);
AimYaw   = FRotator::NormalizeAxis(LocalAimDir.Rotation().Yaw);
```

멀티플레이 동기화: 시뮬레이티드 프록시의 `GetBaseAimRotation()`은 Mover InputCmd의 ControlRotation을 재사용하도록 오버라이드되어 있다 (→ [TechDesign_Networking.md](TechDesign_Networking.md)).

**`GetBaseAimRotation()`의 공급원은 폰 종류마다 다르다.** 이 위의 계산식은 그대로 두고 공급원만 갈아끼운다:

| 폰 | 공급원 |
|:---|:---|
| 로컬 제어 플레이어 | `Controller->GetControlRotation()` |
| 원격 플레이어·시뮬레이티드 프록시 | Mover InputCmd의 `ControlRotation` (복제됨) |
| 적 NPC | 액터 전방 + 복제된 로컬 `AimPitchDeg` — 컨트롤러가 없어 `APawn` 기본값은 수평이다 (→ [TechDesign_EnemyNPC.md](TechDesign_EnemyNPC.md) §6 상하 조준) |

적 NPC의 AO가 별도 배선 없이 동작하는 이유는 `BP_LNPEnemy`의 AnimClass가 플레이어와 같은 `ABP_Lyra`이고, 서브 ABP의 AO 노드가 이미 `LNPAnimInstance:AimPitch`/`AimYaw`에 바인딩되어 있기 때문이다. 즉 적 조준 작업은 **애니메이션 에셋을 건드리지 않고 `GetBaseAimRotation()` 하나로 끝난다.**

### 4.2 왼손 Two Bone IK (구현 완료)

양손 무기(롱소드·라이플)에서 오른손은 무기 소켓 어태치로 정확하지만 왼손은 애니메이션 그대로라 그립에서 어긋난다.

- 무기 메시에 `LeftHandGrip` 소켓 배치.
- `ULNPAnimInstance`가 매 프레임 소켓 월드 위치(`LeftHandGripLocation`)와 존재 여부(`bHasLeftHandGrip`)를 갱신.
- 서브 ABP에서 Two Bone IK 노드(`lowerarm_l → hand_l`)가 해당 위치로 왼손을 끌어당김. 소켓이 없는 무기는 자동으로 IK 비활성.

### 4.3 Guard 자세 분기 (구현 완료)

`ULNPAnimInstance::bIsGuarding`(= `MoverComponent->IsGuarding()`, Guard Modifier 태그 기반)을 `ABP_Sub_LongSword`의 Bool Blend 노드가 소비하여 Guard 자세와 기본 Stance를 블렌딩.

---

## 5. 이동·회전 연동 (Mover 2.0)

### 5.1 bFaceMoveDirection

`ULNPInputHandlerComponent::OnProduceInput()`이 `OrientationIntent`를 결정:

| AimMode | bFaceMoveDirection | 결과 |
|---------|:---:|------|
| None | `true` | 입력 방향으로 캐릭터 회전 — 앞으로만 달림 |
| FreeAim / LockOn | `false` | 카메라 정면 고정 — WASD Strafe |

`EquipWeapon()`이 `DefaultAimMode`를 읽어 자동 설정.

### 5.2 LockOn 회전 (구현 완료)

`ULNPLockOnComponent`가 타겟 방향을 `ULNPControlRotationComponent`에 소프트 보정(적립 델타) + 하드 클램프(최대 이탈각)로 전달 → 카메라(컨트롤 회전)가 타겟을 추적하고, Strafe 상태의 캐릭터는 `HorizonForward`(카메라 정면)를 바라보므로 자연스럽게 타겟 방향으로 고정된다. (구 문서의 "접근 방법 A — 카메라 의존" 채택)

### 5.3 Motion Matching Strafe

`UCharacterTrajectoryComponent`는 `ACharacter` 전용이라 미사용. Mover 2.0의 `UMoverTrajectoryPredictor`가 `OrientationIntent` 기반으로 미래 경로를 예측해 PSD에서 게걸음/뒷걸음 포즈를 재생 — 별도 구현 불필요.

### 5.4 이동 입력 차단

`OnProduceInput()`에서 ASC가 `LNP.Block.MovementInput` 보유 시 **이동 벡터만 0으로** 차단. OrientationIntent·ControlRotation은 유지되어 캐릭터 회전·조준은 계속 동작.

---

## 6. GAS 연동

### 6.1 Chooser 기반 몽타주 선택

모든 상황별 몽타주(공격 콤보·대시·히트리액트·패링)는 단일 Chooser Table에서 선택한다.

```cpp
// ULNPMontageChooserContext { WeaponType, SituationType, Value } 를 채워 평가
UAnimMontage* M = EvaluateMontage(TAG_Montage_Situation_Dash, TAG_Montage_Value_Direction_Left);
```

- 열 구성: WeaponType(Has Tag) / SituationType(Has Tag) / Value(Has Tag) → `UAnimMontage*`
- C++/GAS 코드 변경 없이 에디터에서 조건-몽타주 매핑 관리.
- Context 오브젝트는 캐릭터당 1회 생성 후 재사용 (`MontageCtx`).

### 6.2 콤보 시스템 — ComboWindow 태그 소비 방식

단일 몽타주의 섹션 분기로 다단 콤보를 구현. 콤보 연결은 `TryActivateAttack()`이 태그를 직접 소비한다:

```
공격 입력 → TryActivateAttack()
├─ TAG_State_ComboWindow 있음 → 태그 즉시 소비(중복 진입 방지) + IncrementComboIndex
│     + Server_SetComboIndex RPC(원격 클라) + 현재 어빌리티 취소 + 재발동  → 다음 섹션
├─ TAG_Block_AttackInput 있음 → false (선딜 중 연타 차단)
└─ 그 외 → ResetCombo 후 첫 공격 발동
```

`Server_SetComboIndex`가 필요한 이유: `CurrentComboIndex`는 입력을 소유한 머신에서만 갱신되는데, 서버의 MeleeAttack 어빌리티가 몽타주 섹션을 서버 측 인덱스로 고르므로 동기화하지 않으면 서버·관전자는 항상 콤보 1만 재생하고 히트 판정 타이밍도 어긋난다.

### 6.3 ANS 5종 — 몽타주 타임라인 구간 제어

근접 공격 어빌리티는 `Montage_Play` 직후 종료되므로 `ActivationOwnedTags`를 쓸 수 없다. 대신 몽타주 타임라인에 ANS를 중첩 배치해 프레임 단위로 제어한다. 에디터 작업만으로 각 구간의 길이·동작을 독립 조절 가능.

| 구간 | ANS | 동작 |
|------|-----|------|
| 선딜 | `ANS_LNPBlockMovementInput` | `TAG_Block_MovementInput` 추가/제거 — 이동 입력 차단 |
| 히트 | `ANS_LNPMeleeHitWindow` | Mass 엔티티 생성 → 매 Tick 칼날 본 위치 기록 → 종료 시 파괴 (§7.1) |
| 입력 차단 | `ANS_LNPAttackInputBlock` | `TAG_Block_AttackInput` 추가/제거 — 선딜 연타 차단 |
| 콤보 창 | `ANS_LNPComboWindow` | `TAG_State_ComboWindow` 추가/제거 — §6.2의 소비 대상 |
| 후딜 | `ANS_LNPCancelMontageOnMovement` | NotifyTick: 이동 입력 감지 시 `Montage_Stop(BlendOutTime)` — 이동 선입력 캔슬 |

> UE는 `Montage_Stop` 취소 시에도 활성 ANS 전체에 `NotifyEnd`를 보장하므로 태그 Add/Remove 페어가 항상 유지된다.

### 6.4 히트리액트

`PlayHitReact(HitFromWorldDir)`: 피격 방향을 캐릭터 로컬 공간으로 변환 → 4방향 태그 분류 → Chooser로 방향별 몽타주 재생. HitStop(`CustomTimeDilation` 0.1, 타이머 복원)과 함께 GameplayCue 노티파이에서 호출된다.

**히트리액트 몽타주에는 입력 차단 ANS를 붙이지 않는다** (2026-08-29 결정). 행동을 끊는 판단은 전적으로
경직 시스템(→ [TechDesign_Ability.md §2.5](TechDesign_Ability.md))이 하고, 히트리액트는 코스메틱으로 남는다. 이유 셋:

1. 경직 시스템의 존재 이유가 "보통 피격은 행동을 끊지 않는다"이다. 히트리액트에 차단 ANS를 달면
   **모든 피격이 곧 경직**이 되어 누적·임계 체계가 통째로 무의미해진다.
2. `AddLooseGameplayTag`는 복제되지 않고, 히트리액트 몽타주는 각 머신이 GameplayCue를 받아 **로컬로** 재생한다.
   차단 구간이 권위 상태가 아니라 코스메틱 타이밍에 좌우된다 — 서버는 판정 시점에, 피격자 클라이언트는
   RTT 뒤에 차단이 걸린다.
3. 다음 피격이나 공격이 `Montage_Stop`을 부르면 Begin/End 페어가 깨질 여지가 남는다.
   §6.3의 근접 공격 경로가 `ClearRelativeTag()` 안전망을 따로 두고 있는 것이 그 위험의 방증이다.

### 6.5 경직 — ANS가 아니라 어빌리티가 구간을 소유한다

§6.3이 "근접 공격 GA는 `Montage_Play` 직후 끝나므로 `ActivationOwnedTags`를 쓸 수 없다"고 적은 것의
**정확히 반대 케이스**다. 경직은 지속되는 것이 본질이므로 어빌리티를 살려 두는 쪽이 맞다.

`ULNPAbility_Stagger`가 `UAbilityTask_WaitDelay` 동안 살아 있으면서 `ActivationOwnedTags`로
`Block.AttackInput` · `Block.MovementInput` · `State.Staggered`를 소유한다. 어빌리티 수명 = 차단 구간이므로
태그 페어가 깨질 수 없고, 서버 권위이며, 소유 클라이언트에는 GAS 활성화 복제로 그대로 전달된다.

경직 몽타주 에셋에는 ANS를 배치하지 않는다 — 몽타주는 코스메틱이고 잠금 시간은 데이터(`ULNPSettings`)가 정한다.

### 6.6 근접 공격 타겟 보정

조준이 살짝 빗나갔을 때 허공을 베지 않도록 공격 초반에 타겟 쪽으로 미세하게 끌어당긴다.
**위치는 Motion Warping 또는 LayeredMove가, 회전은 항상 `OrientationIntent`가 담당한다.**

| 상황 | 위치 보정 | 회전 보정 |
|:---|:---:|:---:|
| 락온 ON | O (락온 타겟) | X (카메라가 이미 추적) |
| 락온 OFF · 정지 | O (전방 탐색 타겟) | O |
| 이동 인풋 중 | **X** | O |

이동 인풋이 있으면 위치 보정을 아예 걸지 않는다 — 이동 인풋이 무조건 우선이다.
회전 보정은 `MoveInput`을 건드리지 않고 `OrientationIntent`만 덮어쓰므로 이동 중에도 공존한다.
판정은 둘 다 InputCmd에서 읽는다 — 컴포넌트의 로컬 상태를 읽으면 서버가 원격 클라이언트의 값을 못 본다.

**타겟 선정** (`ULNPAbility_MeleeAttack::ApplyMeleeAssist`)

락온 중이면 그 타겟을 그대로 쓴다 — **`FLNPModifierInputs::LockOnTarget`으로 InputCmd에 실어** 서버와
리시뮬레이션이 같은 대상을 보게 한다(§7.5). 꺼져 있으면
`SphereOverlapActors(ECC_Pawn, ALNPEnemyCharacter)` 브로드페이즈(락온의 `FindBestTarget`과 같은 방식)로
후보를 추린 뒤 **캐릭터 전방 기준** 점수로 하나를 고른다 — 락온은 화면(카메라) 중앙 기준이라는 점만 다르다.

```
점수 = AngleWeight x (1 - 각도/최대각) + DistanceWeight x (1 - 거리/탐색반경)
```

두 가중치를 `ULNPSettings`에 노출해 "정면 우선 ↔ 근접 우선"을 튜닝한다.
거리·각도는 모두 **접평면 성분으로만** 잰다(§7.4와 같은 규약).

> **기본값(각도 0.6 / 거리 0.4)의 구조적 성질:** 경계각에 선 적은 각도항이 0이라 거리가 0이어도 최대 0.4점,
> 정면에 선 적은 각도항이 0.6이라 사거리 끝에 있어도 최소 0.6점이다. 즉 **정중앙의 적은 아무리 멀어도,
> 경계의 적은 아무리 가까워도 항상 정중앙이 이긴다.** 각도가 사실상 우선 기준이고 거리는 비슷한 각도들
> 사이의 타이브레이커다. 탐색 각도를 넓혀도 옆에 붙은 적이 정면 적을 가로채지 않는다는 뜻이라,
> 이 성질을 전제로 각도를 여유 있게 잡았다(2026-08-31 기준 75도 = 좌우 각각 75도).
> ⚠️ `MeleeAssistMaxSearchAngleDeg`는 좌우 각각이 아니라 **정면으로부터의 이탈각**이다.

⚠️ **시야 차단 검사가 없다.** 벽 뒤에 있어도 콘과 반경 안이면 후보가 된다. 위치 보정은 막히면 그냥
안 움직이지만 **회전 보정은 벽 너머 적을 향해 몸을 돌린다.** 거슬리면 라인 트레이스 한 번으로 막을 수 있다.

**보정량** — 이미 가까우면 0, 멀수록 커지되 상한에서 잘린다.

```
Gap  = max(0, 접평면거리 - 무기의 MeleeIdealDistance)
보정 = min(Gap x Strength, MaxCorrectionDistance)
```

여기까지는 두 경로가 공유한다. 갈리는 것은 **그 이동을 무엇으로 만들어내느냐**다.

#### 경로 선택 — 애니메이션에 루트모션 이동량이 있는가

`ULNPSettings::MeleeAssistMinWindowRootMotion`(기본 10cm)이 기준이다. 섹션 안에서 루트모션 이동량이
가장 큰 구간을 찾아 그 값이 기준 이상이면 Motion Warping, 미만이면 LayeredMove로 간다.
`LNP.Melee.Assist.ForceMode`(-1 자동 / 0 Pull / 1 Warp)로 강제 전환할 수 있다.

**"어느 쪽이 나은가"가 아니라 "워프할 원본이 있는가"가 기준이다.** 루트모션은 애니메이터가 의도한
캐릭터 이동 그 자체다. 이동량이 실려 있으면 그것을 스케일하는 Motion Warping이 가장 자연스럽고,
in-place로 만든 애니메이션(= "움직이지 않는 것이 자연스럽다"는 의도)에는 스케일할 원본이 없으므로
코드가 만든 속도를 얹는다.

> 2026-08-30 실측: 현재 검술 세트(`AM_SW_Attack_01` → `A_SW_Attack_01`·`A_SW_Attack_03`)는
> `bEnableRootMotion = true`이지만 **두 섹션 모두 이동량이 0.00cm**다. 플래그는 "루트모션을 쓴다"는
> 뜻일 뿐이고 루트 본이 제자리에 고정된 in-place 애니메이션이다. 그래서 지금은 항상 LayeredMove로 간다.

#### 경로 A — LayeredMove (in-place 애니메이션)

보정 구간은 **[섹션 시작, 그 섹션의 첫 `ANS_LNPMeleeHitWindow` 시작]**. 선딜 동안 끌려가서
칼날이 살아나는 순간 정렬이 끝난다. 히트 윈도우가 없으면 `MeleeAssistWarpWindowSeconds`로 폴백한다.

`FLayeredMove_LinearVelocity`(대시와 같은 패턴)로 `타겟 방향 x 속도`를 그 시간만큼 더한다.
요구 속도가 `MeleeAssistMaxPullSpeed`를 넘으면 **속도를 지키고 덜 당긴다.**
총 이동량이 구조적으로 보정 거리를 넘을 수 없고, 장애물에 막히면 Mover가 충돌을 풀며 그냥 안 움직인다.

#### 경로 B — Motion Warping (루트모션이 실린 애니메이션)

보정 구간은 **섹션 안에서 루트모션 순 이동량이 최대인 구간**을 1/30초 스텝 슬라이딩으로 찾는다.
스케일할 원본이 거기에만 있기 때문이다. 결과는 (몽타주, 섹션, 창 길이)로 캐시한다 — 전부 정적 데이터다.

몽타주 에셋은 수정하지 않는다. `UAnimNotifyState_MotionWarping`을 배치하는 대신
`URootMotionModifier_SkewWarp`를 `AddModifier`로 런타임 등록한다. 엔진의 `UpdateWithContext`는
노티파이로 모디파이어를 *생성*할 뿐이고 수동 추가분도 같은 배열에서 동일하게 처리된다.
`MaxSpeedClampRatio`가 protected라 값을 넣기 위한 최소 파생 클래스가 `ULNPMeleeAssistWarpModifier`다.

워프가 동작하려면 루트모션이 Mover를 통과해야 하므로 `FLayeredMove_AnimRootMotion`을 함께 큐잉해
`ConvertLocalRootMotionToWorld` → `UMotionWarpingComponent` 파이프라인을 태운다(대시와 같은 경로).
**시작점은 창이 아니라 섹션 시작이다** — 레이어드 무브는 `StartingMontagePosition`에서 자체 시계로
진행하는데 몽타주는 섹션 시작부터 재생되므로, 창 시작을 넣으면 스윙 구간의 루트모션이 선딜에 적용된다.
창 밖에서는 모디파이어가 비활성이라 애니메이션 원본 루트모션이 그대로 통과한다 — 애니메이터의 의도대로다.

두 경로 모두 `MixMode`는 `AdditiveVelocity`다. Override로 두면 보정이 이동을 통째로 대체해 버린다.
보정 대상이 없으면 어느 레이어드 무브도 큐잉하지 않으므로 **평소 공격 이동은 종전과 완전히 동일하다.**

**컴포넌트 배선은 한 줄이다.** `ALNPCharacterBase`가 `UMotionWarpingComponent`를 생성하면
`UMoverComponent::InitializeComponent`가 이를 찾아 `UMotionWarpingMoverAdapter`를 자동으로 붙인다.
`ACharacter`/`UCharacterMovementComponent`가 아니어도 UE 5.8 Mover 2.0은 모션 워핑을 지원한다.

**튜닝**: `ULNPSettings`의 `Combat|Melee Assist` 섹션.
CVar `LNP.Melee.Assist.Strength`(음수면 설정값), `LNP.Melee.Assist.ForceMode`, `LNP.Melee.Assist.Debug`.
사용자 환경설정 UI가 생기면 `LNPMeleeAssist::GetStrength()`의 출처만 바꾸면 된다.

---

---

## 7. 어필 포인트 (트러블슈팅 & 엔진 분석)

### 7.1 UAnimNotifyState는 싱글턴이다 — 멀티플레이 근접 판정 실패 버그

`UAnimNotifyState`는 몽타주 에셋에 배치된 **단일 오브젝트**를, 그 몽타주를 재생하는 모든 AnimInstance(서버·클라이언트·여러 캐릭터)가 공유한다. 진행 중인 스윙의 Mass 엔티티 핸들을 평범한 멤버 변수에 저장한 초기 구현은 PIE 2인 근접 PvP에서 두 캐릭터의 스윙이 겹치는 순간 서로의 상태를 덮어써 히트 판정이 실패했다.

**해결:** `TMap<TWeakObjectPtr<USkeletalMeshComponent>, FActiveSwing>`으로 호출 컨텍스트(MeshComp)별 상태를 분리. AnimNotify 계열에 상태를 둘 때의 일반 원칙으로 코드에 명문화했다.

### 7.2 Deferred BuildEntity + AddTag의 아키타입 전환 함정

히트 윈도우 엔티티를 `BuildEntity`와 `AddTag`로 같은 디퍼드 배치에 넣으면, 아키타입 전환 타이밍 때문에 같은 프레임 쿼리가 엔티티를 놓치는 문제가 있었다. **Fragment의 존재 자체를 "활성 공격 윈도우" 신호로 사용**하고 Tag를 쓰지 않는 설계로 우회.

### 7.3 몽타주 취소 안전망 — TimeToLive

`NotifyEnd`가 어떤 이유로든 누락될 때(액터 파괴 등)를 대비해 Fragment에 `TimeToLive = TotalDuration + 0.2s`를 심어두고, 별도 Lifetime 프로세서가 만료 엔티티를 자동 파괴한다. ANS 수명과 Mass 수명의 이중 안전장치.

### 7.4 구형 월드에서의 애니메이션 수치 보정

- `GroundSpeed`: `Velocity.Size2D()`는 월드 XY 평면 전제 → 캐릭터 로컬 Up에 수직인 평면으로 `VectorPlaneProject`하여 계산.
- Aim Offset: §4.1의 로컬 좌표계 변환.
- 방향 계산(`CalculateDirection`)은 캐릭터 회전 기준이라 그대로 유효.

### 7.5 Motion Warping을 구면 월드·Mover·멀티플레이에 얹을 때의 함정 4건

**루트모션이 없는 구간에 워프를 걸면 캐릭터가 날아간다.**
`URootMotionModifier_SkewWarp::ProcessRootMotion`은 두 갈래다. 창 구간의 루트모션 이동량이
`2e-4` 이상이면 정상 경로 — 메시 로컬 공간에서만 계산하고 끝에 `MaxSpeedClampRatio`로
"애니메이션 원본 속도 x N"에 클램프한다. 미만이면 `StartTransform`에서 타겟까지 Lerp하는
전혀 다른 경로로 빠지는데, **그쪽에는 클램프가 없다.** 장애물에 막혀 못 나아가면
`NextLocation - CurrentLocation`이 한 프레임에 보정 거리 전체까지 벌어져 그대로 속도가 되고
(150cm / 0.016s 약 9,000cm/s) 지면을 떠나 Falling으로 전환되며 멀리 튕겨나간다(2026-08-30 실측).
→ **워프 창을 "루트모션 이동량이 최대인 구간"으로 고르고, 그래도 기준 미달이면 워프를 걸지 않는다**
(§6.6 경로 선택). 처음에는 창을 [섹션 시작, 첫 히트 윈도우]로 잡았는데, 검 공격은 선딜에 자세만 잡고
휘두르는 순간 밀고 나가므로 **하필 이동량이 가장 없는 구간**을 고르고 있었다.

**현재 위치의 기준점은 `GetVisualRootLocation()`(발밑)이다.** 워프 타겟을 `GetActorLocation()`
(캡슐 중심)으로 넘기면 캡슐 반높이만큼의 수직 성분이 상시로 껴서 캐릭터를 위로 밀어 올린다.
→ 타겟을 `MoverComponent::GetPrimaryVisualComponent()`의 위치 기준으로 잡고
**`bWarpToFeetLocation = true`** 로 짝을 맞춘다. 그래야 두 경로가 같은 기준점을 본다.
한편 **`bIgnoreZAxis`는 월드 Z 기준**이라(`TargetLocation.Z = CurrentLocation.Z`) 구형 중력에서는
쓸 수 없다 — 적도 근처에서 월드 Z는 오히려 수평이다. → **끄고 접평면 투영은 직접 한다.**

**회전 워프(`bWarpRotation`)는 이동 모드가 즉시 되감는다.** 레이어드 무브가 낸
`FProposedMove::AngularVelocityDegrees`는 `MovementMixer`에서 이동 모드의 제안에 **더해질 뿐**인데,
이동 모드는 매 프레임 `OrientationIntent`를 향해 `TurningRate`(엔진 기본 500도/초)로 캐릭터를 되돌린다.
결국 워프가 만든 회전은 같은 프레임에 상쇄된다.
→ **회전 보정은 워프가 아니라 `FCharacterDefaultInputs::OrientationIntent`를 덮어써서 한다**
(`ULNPInputHandlerComponent::SetMeleeAssistOrientation`). 이동 모드가 스스로 돌아 주고,
`OrientationIntent`는 InputCmd 필드라 복제·롤백이 공짜로 따라온다 —
"시뮬레이션에 영향을 주는 값은 InputCmd를 탄다"는 규약(`TechDesign_CharacterMovement.md` §7.1)도 자동으로 지켜진다.

**락온 타겟은 반드시 InputCmd로 보낸다.** `ULNPLockOnComponent`의 타겟은 로컬 상태라 서버가
원격 클라이언트의 락온을 알 수 없다. 그대로 두면 서버만 자동 탐색 분기를 타서 **다른 적**을 보정 대상으로 고른다.
여러 적을 상대할 때 락온을 쓴다는 것은 "자동 탐색이 고른 것 말고 이 적을 치겠다"는 명시적 의사표현이므로,
서버가 그걸 모르면 보정이 정확히 반대로 작동한다.
→ `FLNPModifierInputs::LockOnTarget`으로 전달한다(대시의 `DashInputIntent`와 같은 계열).
락온하지 않은 평상시에는 비트 하나만 쓰도록 조건부 직렬화한다.

⚠️ 단 **`ShouldReconcile`에는 넣지 않는다.** 이 값은 Mover 시뮬레이션이 읽지 않고
(이동 모드·모디파이어 어느 것도 참조하지 않는다) 어빌리티가 `GetLastInputCmd()`로 꺼내 쓰는
전달 수단일 뿐이다. 넣으면 NetGUID가 아직 풀리지 않은 프레임마다 불필요한 이동 리시뮬레이션이 돈다.
`Interpolate`에는 넣어야 한다 — 빠뜨리면 기본 구현이 `check(false)`로 죽는다.

**검증 결과 (2026-08-31, `-game` 2P).** 게스트가 락온하고 근접 공격한 50건을 호스트 로그와 시각으로
짝지어 대조했을 때 **`lockOn` 불일치 0건**이다. 동기화가 의도대로 동작한다.
(⚠️ 대조는 액터 이름이 아니라 `dist`·`correction` 수치로 해야 한다 — 복제 액터의 인스턴스 이름은
머신마다 독립적으로 붙어서 같은 적이라도 호스트와 게스트에서 이름이 다르다.)

**남은 차이 — 적의 위치는 여전히 지연만큼 어긋난다.** 타겟 액터가 같아도 게스트가 보는 적 위치와
서버가 보는 위치가 달라, 같은 공격에서 보정량이 갈린다(50건 중 20건, `dist` 최대 44.9cm 차이 =
보정량 22.4cm 차이). `ResimulationErrorPositionThreshold`(10cm)를 넘으므로 Mover 리컨실리에이션이 메우며,
2P 실측에서 체감되지 않아 **수용한다.** 지연이 큰 환경에서 거슬리면 대시의 `DashInputIntent`처럼
**보정 벡터 자체를 InputCmd에 실으면** 차이가 0이 된다 — 다만 클라이언트가 변위를 지시하는 형태가 된다.


---

## 8. 미구현 / 잔여 작업

- **근접 공격 보정 — 보정 강도의 사용자 환경설정 이전:** 현재는 `ULNPSettings`(개발자 설정)에 있다.
  환경설정 UI가 생기면 `LNPMeleeAssist::GetStrength()`의 출처만 그쪽으로 바꾸면 된다.
- **근접 공격 보정 — 루트모션 공격 모션 확보 시:** 현 검술 세트는 in-place라 항상 LayeredMove 경로다.
  이동량이 실린 모션을 넣으면 §6.6의 경로 선택이 자동으로 Motion Warping을 태운다. 코드는 이미 있다.
- **근접 공격 보정 — 시야 차단 검사(선택):** 벽 너머 적에게 회전 보정이 걸린다(§6.6). 라인 트레이스 한 번으로 막을 수 있다.
- **`ABP_Sub_LongSword` Look At:** 락온 중 neck_01·head가 타겟을 미세 추적하는 Bone Control. 현재는 캐릭터 전체 회전(§5.2)으로 대체 중 — 필요성 재평가 후 적용.
- **상하체 슬롯 분리 확장:** 현재 `FullBody` 슬롯만 운용. 이동 중 상체만 공격하는 `UpperBody` 슬롯 분리는 필요 시 도입.
- **Guard/Parry GameplayCue 에셋:** 태그·코드 경로는 완성, VFX/SFX 에셋 연결이 에디터 잔여 작업.
- **경직 몽타주 Chooser 행:** `Situation.Stagger` × `Value.Stagger.Light/Heavy/Parried` 3종 배선 완료.
  현재 배정은 Light `AM_SW_Damage_Fast` / Parried `AM_SW_Damage_Backward` / Heavy `AM_MM_HitReact_Front_Hvy_01`.
  **넉다운·기상 몽타주를 확보하면 Heavy 행의 에셋만 교체**하면 된다 (→ [TechDesign_Poise.md](TechDesign_Poise.md)).
- **그로기 루프 포즈:** 그로기는 게이지에 종속된 가변 길이 상태인데 몽타주는 원샷이라,
  긴 그로기에서는 남은 시간이 idle 포즈로 굳는다. 루프 가능한 포즈가 필요하다.
