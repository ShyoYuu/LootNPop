# 컴뱃 애니메이션 시스템 기술 명세서

> **미구현 항목**
> - LockOn 카메라 자동추적 구현 방식 미결정 (섹션 6.3 참조).
> - `ABP_Sub_LongSword` Look At 기능 미구현.
> - 무기 IK 미구현 (Phase 4 참조).

---

## 1. 설계 목표

현재 모션 매칭 기반 로코모션(Unarmed) ABP 1개가 존재. 이 공용 로코모션 위에 **[피스톨, 라이플, 롱소드]** 3종의 무기 레이어를 동적으로 블랜딩한다.

### 무기별 이동·조작 방식

| 무기 | 조준 모드 | 회전 방식 | 이동 방식 |
|------|----------|----------|----------|
| 맨손 | None | 입력 방향으로 캐릭터 전체 회전 | 항상 앞으로 이동 |
| 롱소드 (비락온) | None | 입력 방향으로 캐릭터 전체 회전 | 항상 앞으로 이동 |
| 피스톨 | FreeAim | 카메라 정면 고정, 조준점 상시 표시 | 전후좌우 Strafe |
| 라이플 | FreeAim | 카메라 정면 고정, 조준점 상시 표시 | 전후좌우 Strafe |
| 롱소드 (락온 중) | LockOn | 타깃 적 방향 고정 | 전후좌우 Strafe |

**핵심 설계 원칙:**
- 무기 장착 및 락온 상태는 런타임에 동적으로 전환 가능

---

## 2. 시스템 아키텍처

```
[Character / Weapon Component]
       │
       ▼  (Gameplay Tags 발급 + 무기 액터 동적 스폰)
[ALI_WeaponStyles — Anim Layer Interface]
       │
       ▼  (런타임 LinkAnimClassLayers 바인딩)
[ABP_BaseCharacter — 공용 메인 AnimBP]
  └── [ABP_Sub_Pistol / ABP_Sub_Rifle / ABP_Sub_LongSword / ABP_Sub_Unarmed]
```

---

## 3. Gameplay Tags

### 3.1 무기 장착 태그

| 태그 | 설명 |
|------|------|
| `LNP.Weapon.Unarmed` | 맨손 (기본값) |
| `LNP.Weapon.Pistol` | 피스톨 장착 |
| `LNP.Weapon.Rifle` | 라이플 장착 |
| `LNP.Weapon.LongSword` | 롱소드 장착 |

### 3.2 조준 모드 태그

| 태그 | 설명 | 활성화 조건 |
|------|------|------------|
| `LNP.AimMode.None` | 조준 없음 | 맨손, 롱소드 비락온 |
| `LNP.AimMode.FreeAim` | 마우스 기준 조준 | 피스톨/라이플 장착 시 상시 |
| `LNP.AimMode.LockOn` | 타깃 고정 조준 | 롱소드 장착 + 락온 입력 시 |

`DefaultAimMode`는 `ULNPWeaponData` 에셋에서 직접 지정 (원거리 무기 → `LNP.AimMode.FreeAim`, 나머지 → 비워둠).
LockOn 전환은 `DefaultAimMode == None`일 때만 허용 (코드 하드코딩).

### 3.3 액션·시스템 제어 태그

| 태그 | 설명 |
|------|------|
| `LNP.Action.Attacking` | 공격 애니메이션 재생 중 |
| `LNP.Block.MovementInput` | 플레이어 이동 입력 차단 |

### 3.4 Guard / Parry 상태 태그

| 태그 | 관리 주체 | 설명 |
|------|----------|------|
| `LNP.State.Guarding` | `ULNPInputHandlerComponent` | Guard 버튼 누르는 동안 ASC에 유지. Guard ABP 분기 기준. |
| `LNP.State.ParryWindow` | `ULNPInputHandlerComponent` + Timer | Guard 입력 직후 `ParryWindowDuration`(기본 0.15초) 동안만 활성. |
| `GameplayCue.LNP.Guard.Block` | `FLNPApplyDamageGECommand` | Guard 성공 시 VFX/SFX 큐. |
| `GameplayCue.LNP.Parry.Success` | `FLNPApplyDamageGECommand` | Parry 성공 시 VFX/SFX 큐. |
| `LNP.Mover.IsGuarding` | `ULNPCharacterMoverComponent` | Guard Modifier 활성 여부. `ULNPAnimInstance::bIsGuarding` 소스. |

---

## 4. AnimBP 구조

### 4.1 에셋 목록

| 에셋명 | 종류 | 설명 |
|--------|------|------|
| `ABP_BaseCharacter` | AnimBP (Main) | 공용. 모션 매칭 + 레이어 합성 파이프라인 |
| `ALI_WeaponStyles` | Anim Layer Interface | 무기별 서브 AnimBP 계약 정의 |
| `ABP_Sub_Unarmed` | AnimBP (Sub) | 맨손 Idle/Stance |
| `ABP_Sub_Pistol` | AnimBP (Sub) | 피스톨 파지 Pose + Aim |
| `ABP_Sub_Rifle` | AnimBP (Sub) | 라이플 파지 Pose + Aim |
| `ABP_Sub_LongSword` | AnimBP (Sub) | 롱소드 기본 Stance |

### 4.2 ALI_WeaponStyles 인터페이스

메인 AnimBP 그래프를 단순하게 유지하기 위해 Anim Layer Interface로 무기별 오버레이 로직을 서브 ABP에 위임.
**어떤 본에 얼마나 블렌딩할지는 각 서브 ABP가 직접 결정한다.**

정의할 레이어 함수:
- `ApplyWeaponStyleOverlay` — 베이스 포즈(모션 매칭 결과)를 받아 무기별 Stance·Aim 오버레이를 적용한 포즈 반환

각 `ABP_Sub_*`가 담당하는 처리:

| 서브 ABP | 내부 처리 |
|-----------|-----------|
| `ABP_Sub_Unarmed` | 오버레이 없이 베이스 포즈 그대로 통과 |
| `ABP_Sub_Pistol` | spine_01 이상 상체에 피스톨 파지 포즈 + `AO_Pistol` 적용 |
| `ABP_Sub_Rifle` | spine_01 이상 상체에 라이플 파지 포즈 + `AO_Rifle` 적용 |
| `ABP_Sub_LongSword` | 롱소드 Stance 오버레이; 락온 시 Look At 추가 (섹션 5.3 참조) |

### 4.3 메인 AnimBP 파이프라인 (ABP_BaseCharacter)

메인 ABP는 5개 노드의 직선 연결만 유지한다. 무기별 블렌딩 세부 로직은 모두 서브 ABP 내부로 위임.

```text
[Motion Matching]  ← 맨손 PSD 기반 전신 이동·Strafe 포즈 생성
       │
[Linked Anim Layer (ALI_WeaponStyles::ApplyWeaponStyleOverlay)]  ✅ 연결 완료
       │  C++의 LinkAnimClassLayers()로 무기 서브 ABP가 런타임 교체됨
       │  각 서브 ABP가 자체 본·가중치로 포즈 오버레이 처리
       │  bIsGuarding 기반 Guard 자세 분기도 이 레이어 내부에서 처리  ✅ 구현 완료
       │
[Slot 'FullBody']  ← GAS PlayMontageAndWait 몽타주 재생  ✅ 구현 완료
       │  공격·가드 진입/해제 등 전신 액션이 여기서 오버라이드
       │
[Pose History]  ← Motion Matching 다음 프레임 쿼리용 포즈 기록  ✅ 구현 완료
       │
[Output Pose]
```

> **Guard ABP 분기:** `ABP_Sub_LongSword`(또는 별도 Guard 레이어)에서 `bIsGuarding` bool로 Guard 자세와 기본 자세를 블렌딩. ✅ 구현 완료.

> **Aim Offset:** 메인 ABP에 없음. 원거리 무기(Pistol·Rifle)의 AO는 각 서브 ABP 내부에서 처리 (섹션 5.2).
>

---

## 5. 오버레이 상세

### 5.1 원거리 무기 Aim Offset (서브 ABP 내부)

Aim Offset은 메인 ABP에 없고, **원거리 무기 서브 ABP의 `ApplyWeaponStyleOverlay` 구현 내부**에서 직접 적용한다.

| 서브 ABP | AO 에셋 | Alpha | 적용 본 |
|-----------|---------|-------|---------|
| `ABP_Sub_Pistol` | `AO_Pistol` | 1.0 | spine_01 이상 |
| `ABP_Sub_Rifle` | `AO_Rifle` | 1.0 | spine_01 이상 |

> `AimYaw`·`AimPitch` 값은 `ULNPAnimInstance`에서 계산하여 서브 ABP에 공급한다 (구현 완료).

### 5.2 근접 무기 락온 Look At (ABP_Sub_LongSword 내부) 🔲 미구현

롱소드 락온 모드에서는 Aim Offset 대신 **Bone Control: Look At** 노드로 처리한다.
`ABP_Sub_LongSword`의 `ApplyWeaponStyleOverlay` 내부에서 `LNP.AimMode.LockOn` 태그 보유 여부를 확인해 활성화.

| 본 | 가중치 | 설명 |
|----|--------|------|
| `neck_01` | 1.0 | 주 Look At — 락온 타겟 방향 추적 |
| `head` | 1.0 | 동반 추적 |
| `spine_03` | ~0.1 | 가슴 미세 추적 (선택 적용) |

> 카메라가 타겟을 자동 추적한다면 Look At 없이 캐릭터 전체 방향 전환(`OrientationIntent`)만으로도 충분할 수 있다. 구현 후 판단.

---

## 6. 캐릭터 이동·회전 제어 (Mover 2.0)

이 프로젝트는 `APawn + ULNPCharacterMoverComponent (Mover 2.0)` 구조. 회전 제어는 `ULNPInputHandlerComponent`의 `OrientationIntent` 메커니즘을 통해 동작.

### 6.1 bFaceMoveDirection 플래그

`ULNPInputHandlerComponent::bFaceMoveDirection` (구현 완료)

```cpp
// OnProduceInput() 내부 로직
if (bHasAffirmativeMoveInput)
{
    if (bFaceMoveDirection)
        CharacterInputs.OrientationIntent = MoveInput.GetSafeNormal();  // 입력 방향으로 회전
    else
        CharacterInputs.OrientationIntent = HorizonForward;             // 카메라 정면으로 고정
}
else if (bMaintainLastInputOrientation)
{
    CharacterInputs.OrientationIntent = LastAffirmativeMoveInput;
}
else if (!bFaceMoveDirection)
{
    CharacterInputs.OrientationIntent = HorizonForward;  // 정지 중 카메라 회전 시에도 정면 동기화
}
```

### 6.2 AimMode별 플래그 설정

| AimMode | bFaceMoveDirection | 결과 |
|---------|--------------------------|------|
| `LNP.AimMode.None` | `true` | 입력 방향으로 캐릭터 회전. 앞으로만 달림 |
| `LNP.AimMode.FreeAim` | `false` | 카메라 정면 고정. WASD Strafe |
| `LNP.AimMode.LockOn` | `false` | 카메라 정면 고정. WASD Strafe |

`EquipWeapon()` 내부에서 `WeaponData->DefaultAimMode`를 읽어 `SetFaceMoveDirection()` 자동 설정. (구현 완료)

### 6.3 LockOn 모드 회전 제어 (조사 필요)

**목표:** 롱소드 락온 활성화 시 캐릭터 정면이 타깃 방향으로 고정.

**접근 방법 A (카메라 의존):** 카메라가 타깃을 자동으로 추적하면 `HorizonForward`가 타깃 방향과 일치 → `bFaceMoveDirection = false`만으로 동작. 별도 코드 불필요.

**접근 방법 B (직접 오버라이드):** 카메라 방향과 무관하게 타깃 방향으로 정확히 고정하려면 `OnProduceInput()` 내부에서 `OrientationIntent`를 타깃 방향 벡터로 오버라이드:
```cpp
if (bIsLockOnActive && LockOnTarget)
{
    FVector ToTarget = (LockOnTarget->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal();
    CharacterInputs.OrientationIntent = ToTarget;
}
```

> **🔲 미결정:** LockOn 카메라 자동추적 구현 여부에 따라 A 또는 B 선택. 카메라 자동추적을 구현한다면 A로 충분.

### 6.4 모션 매칭 Strafe 처리

`UCharacterTrajectoryComponent`는 `ACharacter` 전용 컴포넌트로 이 프로젝트에서 사용하지 않음. 대신 Mover 2.0의 `UMoverTrajectoryPredictor`가 동작하며, `OrientationIntent` 기반으로 미래 경로를 예측하여 PSD에서 게걸음/뒷걸음 포즈를 이미 정상 재생 중. **별도 구현 불필요.**

---

## 7. GAS 연동

### 7.1 어빌리티 발동 및 몽타주 재생

각 전투 GA (`GA_Attack_Sword` 등)의 `ActivateAbility()` 내부에서 `PlayMontageAndWait` 태스크로 몽타주 실행.

### 7.2 슬롯 분기 (데이터 지향)

C++/BP에서 슬롯을 분기하지 않음. 기획자/애니메이터가 몽타주 에셋 내부에 지정한 슬롯 이름을 GAS가 런타임에 읽어 AnimBP의 해당 창구로 전달.

현재 운용 슬롯:
- **`FullBody`**: 피스톨/라이플 사격·장전, 롱소드 공격, 가드/패리 등 전신 액션

### 7.3 ANS 기반 공격 제어

근접 공격 Ability는 `Montage_Play` 직후 즉시 종료되므로 `ActivationOwnedTags` 방식 사용 불가. 몽타주 타임라인에 **5종의 ANS**를 구간별로 배치하여 프레임 단위 정밀 제어를 구현.

| ANS 클래스 | 주요 동작 | 배치 구간 |
|-----------|----------|----------|
| `ANS_LNPBlockMovementInput` | `TAG_Block_MovementInput` 추가/제거 | 공격 선딜 — 이동 입력 차단 |
| `ANS_LNPMeleeHitWindow` | MassEntity 생성·Tick·파괴로 칼날 본 위치 기록 | 히트 판정 활성 구간 |
| `ANS_LNPComboWindow` | `TAG_State_ComboWindow` 추가/제거 | 콤보 입력 수용 가능 구간 |
| `ANS_LNPAttackInputBlock` | `TAG_Block_AttackInput` 추가; 종료 시 콤보 버퍼 소비 판정 | 공격 입력 차단 + 콤보 연결 판단 |
| `ANS_LNPCancelMontageOnMovement` | NotifyTick: 이동 입력 감지 시 `Montage_Stop` | 후딜 — 이동 선입력으로 몽타주 취소 허용 |

5종 ANS가 동일한 몽타주 타임라인에 중첩되어, GAS Ability 코드 변경 없이 에디터 작업만으로 각 구간의 길이와 동작을 독립 제어할 수 있다.

구현 상세: 섹션 8.2 참조.

### 7.4 콤보 시스템 (몽타주 섹션 분기)

단일 몽타주 내 섹션 분기로 다단 콤보 구현. 콤보 창 구간에 공격 입력이 감지되면 다음 섹션으로 이어지고, 입력이 없으면 마지막 섹션 완료 후 자연 종료.

```text
Section_1 ──→ Section_2 ──→ Section_3 (마지막)
              ↑ 공격 입력    ↑ 공격 입력
              (콤보 창)      (콤보 창)
```

- `ULNPInputHandlerComponent`의 0.05초 입력 버퍼링으로 체인 입력 수신.

### 7.5 Chooser 기반 몽타주 선택

Chooser 테이블로 무기 종류·공격 유형 등 런타임 조건에 따라 재생 몽타주를 자동 선택. C++/GAS 코드 변경 없이 에디터에서 조건-몽타주 매핑 관리.

---

## 8. 이동 입력 차단 및 루트 모션 (Mover 2.0)

### 8.1 입력 차단 (✅ 구현 완료)

`ULNPInputHandlerComponent::OnProduceInput()`에서 ASC가 `LNP.Block.MovementInput` 태그를 보유하면 이동 벡터만 0으로 차단. OrientationIntent·ControlRotation은 유지하여 캐릭터 회전·조준은 계속 동작.

```cpp
// BeginPlay(): IAbilitySystemInterface 캐스트로 ASC 캐시
// OnProduceInput(): 네이티브 태그로 직접 비교
const bool bBlockMovement = ASC != nullptr && ASC->HasMatchingGameplayTag(TAG_Block_MovementInput);
if (!bBlockMovement)
{
    FinalDirectionalIntent = (HorizonForward * X) + (RightDir * Y);
}
CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FinalDirectionalIntent);
```

### 8.2 근접 공격 이동 제어 (ANS 기반, ✅ 구현 완료)

`LNPAbility_MeleeAttack`은 `Montage_Play` 직후 `EndAbility`를 호출하므로 Ability가 살아있는 동안 태그를 유지하는 `ActivationOwnedTags` 방식을 쓸 수 없다. 대신 몽타주 타임라인에 5종의 ANS를 중첩 배치해 각 구간을 독립 제어한다.

| 구간 | ANS | 동작 |
|------|-----|------|
| 선딜 | `ANS_LNPBlockMovementInput` | `TAG_Block_MovementInput` 추가 → 이동 입력 차단 / 종료 시 제거 |
| 히트 | `ANS_LNPMeleeHitWindow` | MassEntity 생성, 매 Tick 칼날 본 위치 기록, 종료 시 파괴 |
| 입력 차단 | `ANS_LNPAttackInputBlock` | `TAG_Block_AttackInput` 추가; 종료 시 콤보 버퍼 소비 (버퍼 있음 → 다음 섹션, 없음 → 콤보 리셋) |
| 콤보 창 | `ANS_LNPComboWindow` | `TAG_State_ComboWindow` 추가 → 이 구간의 공격 입력을 콤보 버퍼에 예약 |
| 후딜 | `ANS_LNPCancelMontageOnMovement` | NotifyTick: 이동 입력 감지 시 `Montage_Stop(BlendOutTime)` |

`ANS_LNPCancelMontageOnMovement`의 `BlendOutTime` (기본 `0.3f`)은 UPROPERTY로 노출되어 에디터에서 조절 가능.

> UE는 `Montage_Stop` 방식과 무관하게 활성 ANS 전체에 `NotifyEnd`를 보장하므로, 도중 취소 시에도 태그 Add/Remove 페어가 항상 유지된다.

---

## 10. 구현 체크리스트

### Phase 1 — AnimBP 기반 구조 (진행 중)
- [x] `ALI_WeaponStyles` Anim Layer Interface 생성 (`ApplyWeaponStyleOverlay` 레이어 함수 정의)
- [x] `ABP_BaseCharacter`에 Linked Anim Layer (ApplyWeaponStyleOverlay) 노드 연결
- [x] `ABP_BaseCharacter` 나머지 파이프라인: Slot 'FullBody' → Pose History → Output Pose 연결
- [x] `ABP_Sub_Unarmed` 생성: 베이스 포즈 통과
- [x] `ABP_Sub_LongSword` 생성: 베이스 포즈 통과 (Look At 미구현)
- [x] `ABP_Sub_Pistol` 생성: spine_01 이상 파지 Idle 포즈 블렌딩 + `AO_Pistol`
- [x] `ABP_Sub_Rifle` 생성: spine_01 이상 파지 Idle 포즈 블렌딩 + `AO_Rifle`
- [ ] `ABP_Sub_LongSword`: 락온 시 neck_01·head Look At 적용
- [x] Guard ABP 분기: `bIsGuarding` 기반 Guard 자세 블렌딩 (Linked Anim Layer 내부에 Bool Blend 노드 추가)

### Phase 2 — C++ 런타임 제어 (✅ 완료)
- [x] `LNPGameplayTags.h/cpp`: `LNP.*` 네이티브 태그 정의 (14개, Guard/Parry 포함)
- [x] `ULNPWeaponData`에 `WeaponTag`, `AnimLayerClass`, `DefaultAimMode`, `WeaponMesh`, `AttachSocketName` 필드 추가
- [x] `ALNPCharacterBase::EquipWeapon()`: `LinkAnimClassLayers()` + `bFaceMoveDirection` + ASC 태그 전환 + 무기 메시 어태치
- [x] `LNPAbility_RangedAttack::SpawnProjectile()`: `Muzzle` 소켓 기반 스폰 위치 + 액터 로컬 `MuzzleOffset` 적용
- [x] `ULNPAnimInstance::bIsGuarding`: `MoverComponent->IsGuarding()` 기반, ABP Guard 분기용
- [ ] LockOn 카메라 자동추적 구현 여부 결정 후 6.3의 접근법 A 또는 B 적용

### Phase 3 — GAS 연동 (부분 완료)
- [x] `ULNPInputHandlerComponent`에 ASC 참조 캐시 + `LNP.Block.MovementInput` 이동 입력 차단 구현
- [x] 근접 공격 이동 차단: `ANS_LNPBlockMovementInput` + `ANS_LNPCancelMontageOnMovement` 구현 완료 (섹션 8.2 참조)
- [x] 에디터: 근접 공격 몽타주에 ANS 배치
- [x] Guard 입력 → ASC `TAG_State_Guarding` / `TAG_State_ParryWindow` 태그 연동 완료
- [x] `AM_Attack_*` 몽타주 슬롯 이름 `FullBody` 확인/설정 (에디터)
- [ ] Guard/Parry 성공 GameplayCue 에셋 생성 (`GameplayCue.LNP.Guard.Block`, `GameplayCue.LNP.Parry.Success`)

### Phase 4 — 고도화 (스펙 아웃, 추후 검토)
- [ ] `ABP_Sub_LongSword` Look At 가중치 수치 튜닝
- [ ] 패링 성공 공격자 Stagger/Launch (`FLNPParrySuccessCommand` Phase 2)
- [ ] **무기 IK 보정** — 양손 무기(롱소드·라이플)의 왼손 위치 어긋남 보정.
    - 오른손은 무기 메시에 소켓으로 어태치되어 애니메이션 그대로 이동.
    - 무기 메시에 왼손 그립 소켓(`LeftHandGrip`)을 배치하고, AnimBP에서 **Two Bone IK** 노드로 왼손 본을 해당 소켓 위치로 끌어당김.
    - Two Bone IK 체인: `lowerarm_l` → `hand_l` (IK 타겟: `LeftHandGrip` 소켓 월드 위치).
