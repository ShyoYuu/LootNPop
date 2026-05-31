# 컴뱃 애니메이션 시스템 기술 명세서

> **구현 상태 요약**
> - `LNPGameplayTags.h/cpp`: `LNP.*` 네이티브 태그 10개 정의 완료.
> - `ULNPWeaponData`: `WeaponTag`, `AnimLayerClass`, `DefaultAimMode`, `WeaponMesh`, `AttachSocketName` 필드 추가 및 각 데이터 에셋 입력 완료.
> - `ULNPInputHandlerComponent`: `bFaceMoveDirection` 회전 제어 완료. ASC 캐시 + `LNP.Block.MovementInput` 이동 차단 구현 완료.
> - `ALNPCharacterBase`: `EquipWeapon()` 완료 (태그 전환 + `LinkAnimClassLayers()` + `bFaceMoveDirection` + 무기 메시 어태치). `EquipTestWeapon()` 테스트 함수 추가.
> - `LNPAbility_RangedAttack`: `Muzzle` 소켓 기반 프로젝타일 스폰 위치 구현 완료.
> - `ALI_WeaponStyles`, `ABP_Sub_*` 4종 생성 완료. `ABP_BaseCharacter` Linked Anim Layer 연결 완료.
> - `ABP_Sub_Pistol`·`ABP_Sub_Rifle` spine_01 이상 파지 Idle 포즈 블렌딩 완료. `ABP_Sub_Unarmed`·`ABP_Sub_LongSword` 베이스 포즈 통과.
> - 미구현: `ABP_BaseCharacter` Slot·Pose History, AO/Look At, `Chooser_WeaponLayers`, GA `ActivationOwnedTags`.

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
- 모든 상태 분기는 Gameplay Tags + Chooser Table로 데이터 지향적 처리
- C++ 코드에서 AnimBP 그래프 분기 로직을 최소화

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
| `LNP.Movement.Jumping` | 점프 및 공중 체공 중 |
| `LNP.Block.MovementInput` | 플레이어 이동 입력 차단 |

### 3.4 태그 자동 연동 설정

`LNP.Action.Attacking` 및 `LNP.Movement.Jumping`이 활성화되면 `LNP.Block.MovementInput`이 자동으로 부여되도록, 각 GA 에셋의 `ActivationOwnedTags` 항목에 `LNP.Block.MovementInput` 추가.

> **주의:** `Gameplay Tag Response Table`은 서로 다른 Actor 간의 ASC 상호작용 전용이므로, 동일 캐릭터 내 태그 종속 관계에는 사용 불가.

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
       │
[Slot 'UpperBody']  ← GAS PlayMontageAndWait 몽타주 재생  🔲 미구현
       │  공격·가드 등 상체 액션이 여기서 오버라이드
       │
[Pose History]  ← Motion Matching 다음 프레임 쿼리용 포즈 기록  🔲 미구현
       │
[Output Pose]
```

> **Aim Offset:** 메인 ABP에 없음. 원거리 무기(Pistol·Rifle)의 AO는 각 서브 ABP 내부에서 처리 (섹션 5.2).
>
> **FullBody 액션:** 롱소드 전신 루트모션 공격·구르기 등이 필요해질 때 `Slot 'UpperBody'` 앞에 `Slot 'FullBody'`를 추가 검토.

---

## 5. Chooser 테이블

### 5.1 Chooser_WeaponLayers

- **입력:** `LNP.Weapon.*` 태그
- **출력:** Anim Layer Interface 클래스 (서브 AnimBP)

| 입력 태그 | 출력 서브 AnimBP |
|-----------|----------------|
| `LNP.Weapon.Unarmed` | `ABP_Sub_Unarmed` |
| `LNP.Weapon.Pistol` | `ABP_Sub_Pistol` |
| `LNP.Weapon.Rifle` | `ABP_Sub_Rifle` |
| `LNP.Weapon.LongSword` | `ABP_Sub_LongSword` |

### 5.2 원거리 무기 Aim Offset (서브 ABP 내부)

Aim Offset은 메인 ABP에 없고, **원거리 무기 서브 ABP의 `ApplyWeaponStyleOverlay` 구현 내부**에서 직접 적용한다.

| 서브 ABP | AO 에셋 | Alpha | 적용 본 |
|-----------|---------|-------|---------|
| `ABP_Sub_Pistol` | `AO_Pistol` | 1.0 | spine_01 이상 |
| `ABP_Sub_Rifle` | `AO_Rifle` | 1.0 | spine_01 이상 |

> `AimYaw`·`AimPitch` 값은 `ULNPAnimInstance`에서 계산하여 서브 ABP에 공급한다 (미구현).

### 5.3 근접 무기 락온 Look At (ABP_Sub_LongSword 내부)

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
- **`UpperBody`**: 피스톨/라이플 사격·장전, 롱소드 공격, 가드/패리 등 상체 액션

> **🔲 미구현** — AnimBP Slot 노드(섹션 4.3) 완성 후 적용.

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

### 8.2 공중 제어 감쇠 (선택 옵션)

완전 차단 대신 일부 Air Control을 허용할 경우 조건을 확장:
```cpp
if (ASC->HasMatchingGameplayTag(TAG_Movement_Jumping))
{
    CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FinalDirectionalIntent * 0.2f);
}
```

### 8.3 상태별 물리 동작

| 상태 | 이동 입력 | 실제 이동 원인 |
|------|----------|--------------|
| `LNP.Action.Attacking` (루트 모션 공격) | 차단 (0) | 몽타주 Root Motion 데이터 |
| `LNP.Movement.Jumping` | 차단 또는 20% 감쇠 | 점프 초기 관성 |

---

## 9. 런타임 레이어 바인딩 (C++)

`ALNPCharacterBase::EquipWeapon(ULNPWeaponData*)` 구현 완료. 호출 시 아래 세 가지를 동시에 처리:

```cpp
// 1. ASC 태그 전환 (기존 제거 → 신규 부여)
ASC->RemoveLooseGameplayTag(CurrentWeaponTag);
ASC->AddLooseGameplayTag(NewWeaponTag);       // LNP.Weapon.*
ASC->AddLooseGameplayTag(NewAimModeTag);      // WeaponData->DefaultAimMode

// 2. 서브 AnimBP 레이어 연결
AnimSourceMesh->LinkAnimClassLayers(WeaponData->AnimLayerClass);

// 3. 회전 모드 전환
InputHandlerComponent->SetFaceMoveDirection(!bFreeAim);

// 4. 무기 스켈레탈 메시 어태치
WeaponMeshComponent->SetSkeletalMeshAsset(WeaponData->WeaponMesh);
WeaponMeshComponent->AttachToComponent(AnimSourceMesh, ..., WeaponData->AttachSocketName);
```

---

## 10. 구현 체크리스트

### Phase 1 — AnimBP 기반 구조 (진행 중)
- [x] `ALI_WeaponStyles` Anim Layer Interface 생성 (`ApplyWeaponStyleOverlay` 레이어 함수 정의)
- [x] `ABP_BaseCharacter`에 Linked Anim Layer (ApplyWeaponStyleOverlay) 노드 연결
- [x] `ABP_BaseCharacter` 나머지 파이프라인: Slot 'UpperBody' → Pose History → Output Pose 연결
- [x] `ABP_Sub_Unarmed` 생성: 베이스 포즈 통과
- [x] `ABP_Sub_LongSword` 생성: 베이스 포즈 통과 (Stance 오버레이·Look At 미구현)
- [x] `ABP_Sub_Pistol` 생성: spine_01 이상 파지 Idle 포즈 블렌딩 (`AO_Pistol` 미구현)
- [x] `ABP_Sub_Rifle` 생성: spine_01 이상 파지 Idle 포즈 블렌딩 (`AO_Rifle` 미구현)
- [ ] `ABP_Sub_Pistol` / `ABP_Sub_Rifle`: Aim Offset (`AO_Pistol`, `AO_Rifle`) 추가
- [ ] `ABP_Sub_LongSword`: 롱소드 Stance 오버레이 추가; 락온 시 neck_01·head Look At 적용
- [ ] `Chooser_WeaponLayers` 테이블 생성 및 `LNP.Weapon.*` 태그 → 서브 AnimBP 클래스 매핑

### Phase 2 — C++ 런타임 제어 (✅ 완료)
- [x] `LNPGameplayTags.h/cpp`: `LNP.*` 네이티브 태그 정의 (10개)
- [x] `ULNPWeaponData`에 `WeaponTag`, `AnimLayerClass`, `DefaultAimMode`, `WeaponMesh`, `AttachSocketName` 필드 추가
- [x] `ALNPCharacterBase::EquipWeapon()`: `LinkAnimClassLayers()` + `bFaceMoveDirection` + ASC 태그 전환 + 무기 메시 어태치
- [x] `LNPAbility_RangedAttack::SpawnProjectile()`: `Muzzle` 소켓 기반 스폰 위치 + 액터 로컬 `MuzzleOffset` 적용
- [ ] LockOn 카메라 자동추적 구현 여부 결정 후 6.3의 접근법 A 또는 B 적용

### Phase 3 — GAS 연동 (부분 완료)
- [x] `ULNPInputHandlerComponent`에 ASC 참조 캐시 + `LNP.Block.MovementInput` 이동 입력 차단 구현
- [ ] 각 전투 GA의 `ActivationOwnedTags`에 `LNP.Block.MovementInput` 추가 (에디터)
- [ ] `GA_Attack_*` 몽타주 슬롯 이름 `UpperBody` 확인/설정 (에디터)

### Phase 4 — 고도화 (스펙 아웃, 추후 검토)
- [ ] FullBody Slot 추가 (롱소드 전신 루트모션 공격, 구르기)
- [ ] `ABP_Sub_LongSword` Look At 가중치 수치 튜닝
