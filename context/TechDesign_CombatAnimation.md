# Unreal Engine 5 Combat Animation Architecture Specification

본 문서는 모션 매칭(Motion Matching) 기반의 로코모션 시스템 위에 동적 무기 장착(Dynamic Weapon Swapping), 상하체 블랜딩(Upper-body Blending), 그리고 GAS(Gameplay Ability System) 및 루트 모션(Root Motion) 기반의 상태 제어를 결합한 컴뱃 애니메이션 시스템 아키텍처 명세서입니다.

---

## 1. 시스템 개요 (System Overview)

본 시스템은 데이터 지향적(Data-driven) 설계와 모듈화를 극대화하여, 무기 및 캐릭터 상태가 추가되더라도 메인 애니메이션 블루프린트(AnimBP)의 수정 없이 유연하게 확장할 수 있는 **3개 레이어(Gameplay - Interface - AnimBP)** 구조를 채택합니다.

```
[Character / Weapon Component]
       │
       ▼ (Gameplay Tags 발급 & 무기 액터 동적 스폰)
[Anim Layer Interface]
       │
       ▼ (동적 Graph 스왑 인터페이스 바인딩)
[Base AnimBP + Weapon Specific Layers] (모션 매칭 로코모션 및 다중 슬롯 블랜딩 제어)
```

---

## 2. 데이터 및 상태 관리: Gameplay Tags

상태 관리는 하드코딩된 `Enum` 대신 **Gameplay Tags**를 사용하여 시스템 간 결합도를 낮추고 GAS 및 Chooper 플러그인과의 호환성을 확보합니다.

### 2.1 주요 태그 설계
* **무기 상태 (Weapon States):**
    * `State.Weapon.Unarmed` (맨손)
    * `State.Weapon.OneHandSword` (한손검)
    * `State.Weapon.Pistol` (피스톨)
* **액션 및 이동 상태 (Action & Movement States):**
    * `State.Action.Attacking` (공격 애니메이션 재생 중)
    * `State.Movement.Jumping` (점프 및 공중 체공 중)
* **시스템 제어 태그 (System Block Tags):**
    * `State.Block.MovementInput` (플레이어 조작 입력 차단)

### 2.2 태그 관계 설정 (Tag Relationship)
`State.Action.Attacking`이나 `State.Movement.Jumping`이 활성화되면 `State.Block.MovementInput`이 자동으로 함께 부여되도록 설계합니다. 이를 통해 입력 레이어의 조건문을 간소화합니다.

구현은 각 Gameplay Ability(GA) 에셋의 **`ActivationOwnedTags`** 항목에 `State.Block.MovementInput`을 추가하는 방식을 사용합니다. 이 태그는 어빌리티가 활성화된 동안 ASC에 자동으로 부여되고 종료 시 자동으로 제거됩니다.

> **주의:** `Gameplay Tag Response Table`은 **서로 다른 두 Actor 간의 ASC 태그 상호작용** 전용이므로, 동일 캐릭터 내 태그 종속 관계에는 사용할 수 없습니다.

---

## 3. 애니메이션 블루프린트 구조 (AnimBP Architecture)

### 3.1 레이어화된 애니메이션 그래프 (Linked Anim Graphs)
메인 AnimBP의 비대화를 막기 위해 **Anim Layer Interface**를 사용하여 무기별 독립된 애니메이션 그래프를 동적으로 바인딩합니다.

1.  **인터페이스 정의 (`ALI_WeaponStyles`):**
    * `GetLocomotionPose`, `GetUpperBodyPose` 등의 애니메이션 레이어(함수) 정의
2.  **메인 AnimBP 구성:**
    * 기본 스켈레톤 구조를 소유하며, 로코모션 및 상체 합성 지점에 인터페이스 레이어 노드 배치
3.  **무기별 서브 AnimBP 구현:**
    * 인터페이스를 상속받아 `ABP_Unarmed`, `ABP_Sword`, `ABP_Pistol` 제작
    * 각 무기에 맞는 전용 Idle, 파지 포즈, 조준(Aim) 상태 기술
4.  **동적 바인딩 (C++):**
    * 무기 swap 시 런타임에서 `LinkAnimClassLayers` 호출
    ```cpp
    GetMesh()->LinkAnimClassLayers(NewWeaponAnimClass);
    ```

### 3.2 다중 슬롯 아키텍처 (Multi-Slot Architecture)
무기 리소스 한계(무기별 전용 이동 데이터 부재)를 극복하고, 공격의 무게감과 조준 사격의 독립성을 동시에 확보하기 위해 **UpperBody**와 **FullBody** 슬롯을 분리하여 블랜딩 전후에 배치합니다.

* **`UpperBody` 슬롯:** 하체의 모션 매칭(이동)을 유지하면서 피스톨 조준 사격, 재장전, 가벼운 견제기 등 상체 위주의 액션을 재생할 때 사용
* **`FullBody` 슬롯:** 묵직한 근접 공격, 구르기(Dodge), 피격(Hit Reaction) 등 모션 매칭 결과를 일시적으로 완전히 무시하고 전신 동작을 표현할 때 사용

#### 메인 애님 그래프 파이프라인 구조
```text
[Motion Matching (하체 이동)] 
       │
[Linked Anim Layer (무기 Stance/Aim)] 
       │
[Save Cached Pose : 'LocoAndStance'] 

─── (분기점: 상체 레이어 블랜딩) ─────────────────────────

[Use 'LocoAndStance'] (Base Pose 핀)
       │
       ├─▶ [Layered Blend per bone (spine_01)] ──┐
       │                                         │
[Use 'LocoAndStance'] (Blend Pose 핀)            │
       │                                         │
[Slot 'UpperBody'] ──────────────────────────────┘

─── (최종 통합 및 전신 오버라이드) ───────────────────────

       │ (UpperBody 합성이 끝난 결과물)
       ▼
[Slot 'FullBody']  <--- 대검 및 전신 루트 모션 공격 시 전신 덮어쓰기
       │
       ▼
[Inertialization]  <--- 포즈 전환 시 틱 현상 방지 관성 블랜딩
       │
       ▼
[Output Pose]
```

---

## 4. GAS (Gameplay Ability System) 연동 및 실행

모든 컴뱃 액션은 비용(Cost) 및 쿨타임(Cooldown), 어빌리티 상태를 관리하는 GAS 내부에서 제어됩니다.

1.  **어빌리티 발동 및 몽타주 재생:**
    * 어빌리티(`GA_Attack_Sword`)의 `ActivateAbility()` 내부에서 GAS 기본 제공 태스크인 **`PlayMontageAndWait`**를 호출하여 공격 몽타주를 실행합니다.
2.  **데이터 지향적 슬롯 분기:**
    * C++ 코드나 블루프린트 로직에서 몽타주 분기를 처리하지 않습니다. 
    * 기획자/애니메이터가 애니메이션 몽타주 에셋 내부에 지정한 슬롯(`UpperBody` 또는 `FullBody`)을 GAS가 런타임에 직접 읽어 AnimBP의 해당 창구로 자동 전달합니다.

---

## 5. 입력 차단 및 루트 모션 (Input Blocking & Root Motion)

공격 시 캐릭터가 바닥을 미끄러지는 현상(Sliding)을 방지하고 묵직한 보폭을 표현하며, 점프 중 공중 제어(Air Control)를 제한하기 위해 입력 레이어에서 태그를 검사합니다.

### 5.1 입력 처리 차단 레이어 (Mover 2.0)
이 프로젝트는 `ACharacter` + `UCharacterMovementComponent`가 아닌 **`APawn` + `ULNPCharacterMoverComponent`(Mover 2.0)** 구조입니다. Mover 2.0에서는 이동 입력이 `ULNPInputHandlerComponent`의 `ProduceInput()` 단에서 생산됩니다. 따라서 입력 차단도 이 위치에서 처리합니다.

```cpp
// ULNPInputHandlerComponent::ProduceInput() 또는 이에 상응하는 오버라이드 위치
void ULNPInputHandlerComponent::ProduceInput(float DeltaMs, FMoverInputCmdContext& OutInputCmd)
{
    if (ASC && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Block.MovementInput"))))
    {
        return; // MoveInput을 0으로 비워두면 Mover가 관성만으로 이동 → 미끄러짐 없이 루트 모션과 동기화
    }

    // 기본 입력 생산 로직 (입력 벡터를 OutInputCmd에 기록)
    // ...
}
```

### 5.2 상태별 물리 작동 방식 차이

1.  **루트 모션 공격 (`State.Action.Attacking`):**
    * 플레이어의 입력 벡터는 `0`이 되지만, 애니메이션 몽타주 내부의 **Root Motion** 데이터가 추출되어 캐릭터의 캡슐 컴포넌트(Collision)를 직접 물리적으로 전진시킵니다. 결과적으로 타격 의도에 맞는 체중 이동이 완벽하게 동기화됩니다.
2.  **점프 및 체공 상태 (`State.Movement.Jumping`):**
    * 점프 발동 시 입력을 완전히 차단하면, 캐릭터는 점프 시점의 초기 관성(Inertia)으로만 궤적을 그리며 날아가게 됩니다.
    * **커스텀 확장:** 만약 공중 제어력을 완전히 막지 않고 일부 허용하고 싶다면, 조건문을 확장하여 입력 벡터를 감쇠시킬 수 있습니다.
    ```cpp
    if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Jumping"))))
    {
        MovementVector *= 0.2f; // 공중 제어(Air Control) 20%만 인정
    }
    ```

---
## 6. 구현 및 최적화 핵심 체크리스트

* [ ] **Inertialization 노드 배치:** 애니메이션 레이어가 동적으로 교체되거나 전신 슬롯이 켜지고 꺼질 때 모션이 튀는 것을 방지하기 위해 Anim Graph 최종단 직전에 반드시 배치하고 블랜딩 타입을 `Inertial`로 설정할 것.
* [ ] **Save Cached Pose 활용:** 모션 매칭 노드의 결과를 중복 평가하지 않도록 캐시 포즈로 저장한 뒤 하체와 상체 레이어 입력 핀에 각각 분기하여 연결할 것.
* [ ] **Trajectory 컴포넌트 연동:** 상체 블랜딩(방법 A) 적용 중 피스톨 지향 사격 등으로 이동할 때 게걸음(Strafe)이 정상 작동하도록 무기 장착 태그에 맞춰 모션 매칭 Trajectory의 Orientation 세팅을 동적으로 전환해 줄 것.