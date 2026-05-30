# 컴뱃 애니메이션 시스템 기술 명세서

> **구현 상태 요약**
> - `ULNPAnimInstance`: 모션 매칭 로코모션 변수(속도, 접지 여부)만 보유. 무기 상태 조회·GAS 태그 참조·레이어 전환 로직 미구현.
> - `ULNPInputHandlerComponent`: `bFaceMoveDirection` 플래그 기반 회전 제어는 구현 완료. GAS 태그 기반 입력 차단 미구현.
> - AnimBP 레이어 구조, Chooser 테이블, GAS 연동 전체 미구현.

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
| `State.Weapon.Unarmed` | 맨손 (기본값) |
| `State.Weapon.Pistol` | 피스톨 장착 |
| `State.Weapon.Rifle` | 라이플 장착 |
| `State.Weapon.LongSword` | 롱소드 장착 |

### 3.2 조준 모드 태그

| 태그 | 설명 | 활성화 조건 |
|------|------|------------|
| `State.AimMode.None` | 조준 없음 | 맨손, 롱소드 비락온 |
| `State.AimMode.FreeAim` | 마우스 기준 조준 | 피스톨/라이플 장착 시 상시 |
| `State.AimMode.LockOn` | 타깃 고정 조준 | 롱소드 장착 + 락온 입력 시 |

### 3.3 액션·시스템 제어 태그

| 태그 | 설명 |
|------|------|
| `State.Action.Attacking` | 공격 애니메이션 재생 중 |
| `State.Movement.Jumping` | 점프 및 공중 체공 중 |
| `State.Block.MovementInput` | 플레이어 이동 입력 차단 |

### 3.4 태그 자동 연동 설정

`State.Action.Attacking` 및 `State.Movement.Jumping`이 활성화되면 `State.Block.MovementInput`이 자동으로 부여되도록, 각 GA 에셋의 `ActivationOwnedTags` 항목에 `State.Block.MovementInput` 추가.

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

메인 AnimBP의 비대화를 막기 위해 Anim Layer Interface로 무기별 그래프를 동적으로 바인딩.

정의할 레이어 함수:
- `GetUpperBodyPose` — 현재 무기의 파지/Stance 상체 포즈

각 `ABP_Sub_*`는 이 인터페이스를 구현하여 해당 무기에 맞는 포즈를 반환.

### 4.3 메인 AnimBP 파이프라인 (ABP_BaseCharacter)

```text
[Motion Matching]  ← 맨손 PSD 기반 전신 이동 포즈 생성 (Strafe 포즈 포함)
       │
[Linked Anim Layer (ALI_WeaponStyles)]  ← Chooser_WeaponLayers → 무기 서브 AnimBP 자동 선택
       │  (상체에 무기 파지 포즈 오버레이)
[Aim Offset]  ← Chooser_AimOffsets → AO 에셋 + Alpha, AimYaw/AimPitch C++ 공급
       │  (상체를 조준 방향으로 변형)
[Save Cached Pose : 'StanceAndAim']
       │
─── 상체 전투 몽타주 레이어 ──────────────────────────────────────
       │
[Use 'StanceAndAim'] ─────────────────────────────────────────────┐  (Base Pose)
                                                                  │
[Use 'StanceAndAim'] → [Slot 'UpperBody'] ─────────────────────▶ [Layered Blend per bone (spine_01)]
                          ↑ GAS PlayMontageAndWait 결과                    │
                                                                  ▼
─── 최종 출력 ────────────────────────────────────────────────────
       │
[Inertialization]  ← 레이어 전환·무기 스왑 시 틱 현상 방지
       │
[Output Pose]
```

> **FullBody Slot:** 현재 스펙 아웃. 롱소드 전신 루트모션 공격, 구르기 등이 필요해질 때 `[Slot 'FullBody']`를 `[Inertialization]` 직전에 추가.

---

## 5. Chooser 테이블

### 5.1 Chooser_WeaponLayers

- **입력:** `State.Weapon.*` 태그
- **출력:** Anim Layer Interface 클래스 (서브 AnimBP)

| 입력 태그 | 출력 서브 AnimBP |
|-----------|----------------|
| `State.Weapon.Unarmed` | `ABP_Sub_Unarmed` |
| `State.Weapon.Pistol` | `ABP_Sub_Pistol` |
| `State.Weapon.Rifle` | `ABP_Sub_Rifle` |
| `State.Weapon.LongSword` | `ABP_Sub_LongSword` |

### 5.2 Chooser_AimOffsets

- **입력:** `State.Weapon.*` + `State.AimMode.*` 태그
- **출력:** Aim Offset 에셋 + 적용 Alpha

| 무기 | AimMode | Aim Offset 에셋 | Alpha |
|------|---------|----------------|-------|
| Pistol | FreeAim | `AO_Pistol` | 1.0 |
| Rifle | FreeAim | `AO_Rifle` | 1.0 |
| LongSword | LockOn | `AO_LongSword` | 0.3~0.5 |
| LongSword | None | (없음) | 0.0 |
| Unarmed | None | (없음) | 0.0 |

---

## 6. 캐릭터 이동·회전 제어 (Mover 2.0)

이 프로젝트는 `APawn + ULNPCharacterMoverComponent (Mover 2.0)` 구조. 회전 제어는 `ULNPInputHandlerComponent`의 `OrientationIntent` 메커니즘을 통해 동작.

### 6.1 bFaceMoveDirection 플래그

`ULNPInputHandlerComponent::bFaceMoveDirection` (구현 완료, `Source/LootNPop/Character/LNPInputHandlerComponent.h:68`)

```cpp
// OnProduceInput() 내부 로직 (LNPInputHandlerComponent.cpp:144)
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
| `AimMode.None` | `true` | 입력 방향으로 캐릭터 회전. 앞으로만 달림 |
| `AimMode.FreeAim` | `false` | 카메라 정면 고정. WASD Strafe |
| `AimMode.LockOn` | `false` | 카메라 정면 고정. WASD Strafe |

무기 장착/해제 시 캐릭터 C++에서 `InputHandlerComponent->bFaceMoveDirection`를 적절히 설정.

### 6.3 LockOn 모드 회전 제어 (조사 필요)

**목표:** 롱소드 락온 활성화 시 캐릭터 정면이 타깃 방향으로 고정.

**접근 방법 A (카메라 의존):** 카메라가 타깃을 자동으로 추적하면 `HorizonForward`가 타깃 방향과 일치 → `bFaceMoveDirection = false`만으로 동작. 별도 코드 불필요.

**접근 방법 B (직접 오버라이드):** 카메라 방향과 무관하게 타깃 방향으로 정확히 고정하려면 `OnProduceInput()` 내부에서 `OrientationIntent`를 타깃 방향 벡터로 오버라이드:
```cpp
// LockOn 활성화 시
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

> **🔲 미구현** — AnimBP 슬롯 구조(섹션 4.3) 완성 후 적용.

---

## 8. 이동 입력 차단 및 루트 모션 (Mover 2.0)

### 8.1 입력 차단 구현 목표

`ULNPInputHandlerComponent::OnProduceInput()`에서 ASC가 `State.Block.MovementInput` 태그를 보유하면 이동 벡터를 생산하지 않음:

```cpp
// LNPInputHandlerComponent.cpp — OnProduceInput() 최상단에 추가
if (ASC && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Block.MovementInput"))))
{
    return; // 입력 0 → Mover는 관성만으로 이동 → 루트 모션과 동기화
}
```

> **🔲 미구현:** `ULNPInputHandlerComponent`에 `AbilitySystemComponent` 참조 추가 및 위 로직 삽입 필요.

### 8.2 공중 제어 감쇠 (선택 옵션)

완전 차단 대신 일부 Air Control을 허용할 경우 조건을 확장:
```cpp
if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Jumping"))))
{
    CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FinalDirectionalIntent * 0.2f);
}
```

### 8.3 상태별 물리 동작

| 상태 | 이동 입력 | 실제 이동 원인 |
|------|----------|--------------|
| `State.Action.Attacking` (루트 모션 공격) | 차단 (0) | 몽타주 Root Motion 데이터 |
| `State.Movement.Jumping` | 차단 또는 20% 감쇠 | 점프 초기 관성 |

---

## 9. 런타임 레이어 바인딩 (C++)

무기 장착/교체 시:
```cpp
// Chooser_WeaponLayers 결과로 선택된 서브 AnimBP 클래스를 밀어 넣음
GetMesh()->LinkAnimClassLayers(SelectedWeaponAnimSubClass);
```

동시에 AimMode 태그와 `bFaceMoveDirection`도 함께 전환:
```cpp
InputHandler->bFaceMoveDirection = (NewWeaponTag == TAG_Weapon_Unarmed || !bIsLockOn);
// Trajectory Component Orientation 모드도 여기서 동기화
```

---

## 10. 구현 체크리스트

### Phase 1 — AnimBP 기반 구조 (🔲 미구현)
- [ ] `ALI_WeaponStyles` Anim Layer Interface 생성 (`GetUpperBodyPose` 레이어 정의)
- [ ] `ABP_BaseCharacter` 파이프라인 구성 (Motion Matching → Linked Anim Layer → Aim Offset → Cached Pose → Layered Blend → UpperBody Slot → Inertialization → Output)
- [ ] `ABP_Sub_Unarmed`, `ABP_Sub_Pistol`, `ABP_Sub_Rifle`, `ABP_Sub_LongSword` 생성 및 기본 Stance 구현
- [ ] `Chooser_WeaponLayers` 테이블 생성 및 태그-서브AnimBP 매핑
- [ ] `Chooser_AimOffsets` 테이블 생성 및 태그-AO 에셋 매핑
- [ ] `Save Cached Pose` 노드로 Motion Matching 결과 중복 평가 방지

### Phase 2 — C++ 런타임 제어 (🔲 미구현)
- [ ] 무기 장착 시 `LinkAnimClassLayers()` 호출 로직 구현
- [ ] 무기/AimMode 변경 시 `bFaceMoveDirection` 동적 설정 로직 구현
- [ ] LockOn 카메라 자동추적 구현 여부 결정 후 6.3의 접근법 A 또는 B 적용

### Phase 3 — GAS 연동 (🔲 미구현)
- [ ] `ULNPInputHandlerComponent`에 ASC 참조 추가 + `State.Block.MovementInput` 태그 기반 입력 차단 구현
- [ ] 각 전투 GA의 `ActivationOwnedTags`에 `State.Block.MovementInput` 추가
- [ ] `GA_Attack_*` 몽타주에 `UpperBody` 슬롯 지정 확인

### Phase 4 — 고도화 (스펙 아웃, 추후 검토)
- [ ] FullBody Slot 추가 (롱소드 전신 루트모션 공격, 구르기)
- [ ] `AO_LongSword` 에임 오프셋 가중치 수치 튜닝 (0.3~0.5 범위 내)
