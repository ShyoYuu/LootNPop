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
       │   └─ ABP_Sub_Unarmed / Pistol / Rifle / LongSword 가 런타임 교체
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
| 무기 | `LNP.Weapon.Unarmed / Pistol / Rifle / LongSword` | 장착 무기. `EquipWeapon()`이 전환 |
| 조준 | `LNP.AimMode.None / FreeAim / LockOn` | `ULNPWeaponData::DefaultAimMode`로 지정. LockOn 전환은 None일 때만 허용 |
| 액션 | `LNP.Action.Attacking` | 공격 애니메이션 재생 중 |
| 차단 | `LNP.Block.MovementInput` / `LNP.Block.AttackInput` | ANS가 추가/제거하는 입력 차단 |
| 콤보 | `LNP.State.ComboWindow` | 콤보 입력 수용 구간 (ANS_ComboWindow가 열고 TryActivateAttack이 소비) |
| Guard | `LNP.State.Guarding` / `LNP.State.ParryWindow` | InputHandler가 ASC에 유지/타이머 관리 |
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
| `ABP_Sub_LongSword` | Sub AnimBP | 롱소드 Stance + `bIsGuarding` Guard 자세 분기 + 왼손 Two Bone IK |

**설계 원칙:** 어떤 본에 얼마나 블렌딩할지는 각 서브 ABP가 직접 결정한다. 메인 ABP는 무기를 모른다.

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

---

## 8. 미구현 / 잔여 작업

- **`ABP_Sub_LongSword` Look At:** 락온 중 neck_01·head가 타겟을 미세 추적하는 Bone Control. 현재는 캐릭터 전체 회전(§5.2)으로 대체 중 — 필요성 재평가 후 적용.
- **상하체 슬롯 분리 확장:** 현재 `FullBody` 슬롯만 운용. 이동 중 상체만 공격하는 `UpperBody` 슬롯 분리는 필요 시 도입.
- **Guard/Parry GameplayCue 에셋:** 태그·코드 경로는 완성, VFX/SFX 에셋 연결이 에디터 잔여 작업.
