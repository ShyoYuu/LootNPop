# 어빌리티 시스템 기술 설계

## 1. 아키텍처 개요

```
[ALNPPlayerState]
  ├── UAbilitySystemComponent     ← ASC: 어빌리티 실행·GE 적용의 주체
  ├── ULNPBaseAttributeSet        ← HP, 공격력, 공격 속도, 방어력, 이동속도
  ├── ULNPEquipmentComponent      ← 무기 슬롯 + Active/Passive 스킬 슬롯 관리
  └── ULNPInventoryComponent      ← 아이템 보관함, 버프 아이템 목록

[ALNPCharacterBase (Pawn)]
  └── IAbilitySystemInterface     ← PlayerState의 ASC를 위임 반환
```

**ASC를 PlayerState에 두는 이유:** 기간제 폰 교체 아이템 등 일시적으로 다른 Pawn으로 교체되는 상황에서도 장착 중인 스킬·버프 효과가 유지되어야 하기 때문. UE 표준 패턴(Lyra 등)과 동일.

Pawn의 `GetAbilitySystemComponent()`는 PlayerState의 ASC를 반환하도록 위임 구현:

```cpp
// ALNPCharacterBase.cpp
UAbilitySystemComponent* ALNPCharacterBase::GetAbilitySystemComponent() const
{
    if (const ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
        return PS->GetAbilitySystemComponent();
    return nullptr;
}
```

`InitAbilityActorInfo(PlayerState, Pawn)`은 서버의 `PossessedBy`, 클라이언트의 `OnRep_PlayerState`에서 각각 호출.

---

## 2. 클래스 구조

### 2.1 AttributeSet

**`ULNPBaseAttributeSet`** (`GAS/Attributes/LNPBaseAttributeSet.h`) — PlayerState에 등록

| Attribute | 타입 | 기본값 |
|:---|:---|:---|
| Health | FGameplayAttributeData | 100 |
| MaxHealth | FGameplayAttributeData | 100 |
| AttackPower | FGameplayAttributeData | 10 |
| AttackSpeed | FGameplayAttributeData | 1 |
| DefensePower | FGameplayAttributeData | 0 |
| MoveSpeed | FGameplayAttributeData | 1 |

- `PreAttributeChange`: MaxHealth ≥ 1, AttackSpeed/MoveSpeed ≥ 0.01 클램핑.
- `PostGameplayEffectExecute`: Health를 [0, MaxHealth] 범위로 클램핑.

### 2.2 Ability 클래스 계층

```
UGameplayAbility
  └── ULNPGameplayAbility              ← 공통 기반 (캐릭터/PlayerState 레퍼런스 헬퍼)
        └── ULNPAbility_BasicAttack    ← 무기 기본 공격 추상 기반 (GetEquippedWeaponDef 제공)
              ├── ULNPAbility_RangedAttack   ← 원거리 공격 (Mass Entity 생성 후 즉시 종료)
              └── ULNPAbility_MeleeAttack    ← 근접 공격 (미구현 — HitDetection 이후)
```

`ULNPGameplayAbility`는 `GetOwningCharacter()`, `GetOwningLNPPlayerState()` 헬퍼를 제공.

### 2.3 아이템 정의 DataAsset

무기·스킬·버프가 메커니즘 측면에서 완전히 다르므로 공통 추상 기반 클래스를 두고 분리.

**`ULNPItemDefinitionBase`** (`Item/LNPItemDefinitionBase.h`) — 추상 기반
```
UPrimaryDataAsset
  └── ULNPItemDefinitionBase  (Abstract)
        ├── DisplayName
        ├── AbilitiesToGrant  : TArray<TSubclassOf<ULNPGameplayAbility>>
        └── EffectsToApply    : TArray<TSubclassOf<UGameplayEffect>>
```

**`ULNPWeaponData`** (`Item/LNPWeaponData.h`)
```
+ AttackMontage     : UAnimMontage*
+ ProjectileType    : ELNPProjectileType  (Linear / Guided / Lobbed)
+ ProjectileSpeed   : float  (default 5000)
+ ProjectileHitRadius : float  (default 5)
+ ProjectileDamage  : float  (default 10)
+ ProjectileLifetime: float  (default 5)
+ MuzzleOffset      : FVector (default (100,0,0))
```

**`ULNPSkillData`** (`Item/LNPSkillData.h`)
- 추가 필드 없음. Active / Passive 구분은 EquipmentComponent의 슬롯 위치로 결정.

**`ULNPBuffData`** (`Item/LNPBuffData.h`)
```
+ MaxDuration : float  (0 = 무기한)
```

### 2.4 아이템 인스턴스

런타임 상태를 보관하는 구조체. 타입별로 분리하여 불필요한 캐스트 제거.

```cpp
// 무기 슬롯용
USTRUCT() struct FLNPWeaponInstance {
    TObjectPtr<ULNPWeaponData>            Definition;
    TArray<FGameplayAbilitySpecHandle>    GrantedAbilities;
    TArray<FActiveGameplayEffectHandle>   AppliedEffects;
};

// Active / Passive 스킬용
USTRUCT() struct FLNPSkillInstance {
    TObjectPtr<ULNPSkillData>             Definition;
    TArray<FGameplayAbilitySpecHandle>    GrantedAbilities;
    TArray<FActiveGameplayEffectHandle>   AppliedEffects;
};

// 버프 아이템용 (InventoryComponent)
USTRUCT() struct FLNPBuffInstance {
    TObjectPtr<ULNPBuffData>              Definition;
    TArray<FActiveGameplayEffectHandle>   AppliedEffects;
    float                                 RemainingDuration; // 0 = 무기한
};
```

### 2.5 EquipmentComponent

**`ULNPEquipmentComponent`** — `ALNPPlayerState`에 부착

역할:
- 무기 슬롯(1개), Active Skill 슬롯(N개), Passive Skill 목록 관리.
- 장착/해제 시 PlayerState의 ASC에 GA Grant/Revoke, GE Apply/Remove 수행.
- Active Skill 슬롯 수(`MaxActiveSkillSlots`)는 `ULNPSettings`에서 읽어옴.
- `DefaultWeapon` (ULNPWeaponData*): `BeginPlay`에서 자동 장착.

```cpp
void EquipWeapon(ULNPWeaponData* WeaponDef);
void UnequipWeapon();
void EquipActiveSkill(int32 SlotIndex, ULNPSkillData* SkillDef);
void UnequipActiveSkill(int32 SlotIndex);
void AddPassiveSkill(ULNPSkillData* SkillDef);
void RemovePassiveSkill(ULNPSkillData* SkillDef);
```

내부 `GrantItemImpl` / `RevokeItemImpl`은 `ULNPItemDefinitionBase*`와 핸들 배열 참조를 받아 공통 로직을 처리.

### 2.6 InventoryComponent

**`ULNPInventoryComponent`** — `ALNPPlayerState`에 부착

역할:
- 아이템 보관함(`ULNPItemDefinitionBase*` 배열) 관리.
- 버프 아이템 추가 시 ASC에 GE 적용 + `FLNPBuffInstance` 생성 및 Tick 타이머 시작.
- 버프 아이템 드랍 시 GE 제거 + 현재 `RemainingDuration` 반환 (월드 액터 초기화에 사용).
- 버프 아이템 재획득 시 저장된 `RemainingDuration`으로 재적용.

```cpp
void AddBuffItem(ULNPBuffData* ItemDef, float InRemainingDuration = 0.0f);
float RemoveBuffItem(ULNPBuffData* ItemDef);  // 남은 시간 반환
```

---

## 3. 어빌리티 라이프사이클

### 3.1 게임 시작 — 기본 무기 장착

```
ALNPPlayerState::BeginPlay()
  → EquipmentComponent->EquipWeapon(DefaultWeapon)   // DA_Pistol 등
      → ASC->GiveAbility(무기 GA)
      → ASC->ApplyGE(무기 스텟 GE)
```

"맨손"도 `ULNPWeaponData`로 정의하여 시스템 특수 처리 없음.

### 3.2 무기 교체

```
EquipWeapon(NewWeaponDef):
  1. UnequipWeapon()
       → 기존 무기 GA ClearAbility × N
       → 기존 무기 GE RemoveActiveEffect × M
       → WeaponSlot.Reset()
  2. WeaponSlot.Definition = NewWeaponDef
  3. GrantItemImpl(NewWeaponDef, ...) → GA Grant × N, GE Apply × M
```

### 3.3 버프 아이템 라이프사이클

**획득:**
```
AddBuffItem(ItemDef, InRemainingDuration):
  → GE를 Infinite 지속으로 적용 (지속 시간은 RemainingDuration으로 직접 추적)
  → Tick에서 RemainingDuration 차감 → 0 도달 시 ExpireBuffItem() 자동 호출
```

**드랍:**
```
RemoveBuffItem(ItemDef):
  → ASC->RemoveActiveGameplayEffect × N  // 스텟 효과 즉시 제거
  → RemainingDuration 반환               // 월드 드랍 액터에 전달
```

**재획득:**
```
  → 바닥 아이템 액터의 RemainingDuration을 읽어 AddBuffItem() 호출
```

---

## 4. 원거리 공격 — Mass Entity 기반 발사체

### 4.1 설계 원칙

- 발사체는 일반 Actor가 아닌 **Mass Entity**로 관리. 대량의 발사체를 낮은 오버헤드로 처리.
- `ULNPAbility_RangedAttack`은 Mass Entity 생성 후 **즉시 `EndAbility`**. 이후 이동·판정은 Processor 책임.

### 4.2 데이터 타입 (`HitDetection/LNPProjectileMassTypes.h`)

```cpp
UENUM() enum class ELNPProjectileType : uint8 { Linear, Guided, Lobbed };

USTRUCT() struct FLNPProjectileFragment : public FMassFragment {
    FVector PreviousPos;   // HitDetection이 사용하는 선분의 시작점
    FVector CurrentPos;    // 선분의 끝점
    FVector Velocity;
    ELNPProjectileType Type;
    float HitRadiusSq;
    float Damage;
    float LifetimeRemaining;
};
```

### 4.3 발사체 이동 (`HitDetection/LNPProjectileProcessor.h/.cpp`)

**`ULNPProjectileMovementProcessor`** — `PrePhysics` 단계에서 실행

```
매 프레임:
  PreviousPos = CurrentPos
  CurrentPos += Velocity * DeltaTime
  LifetimeRemaining -= DeltaTime
  LifetimeRemaining <= 0 → Context.Defer().DestroyEntities()
```

### 4.4 스폰 흐름 (`ULNPAbility_RangedAttack::SpawnProjectile`)

```
1. UMassEntitySubsystem::GetMutableEntityManager()
2. CreateArchetype({ FLNPProjectileFragment })
3. CreateEntity(Archetype)
4. GetFragmentDataChecked<FLNPProjectileFragment>() → 필드 초기화
   - SpawnPos = ActorLocation + Forward * MuzzleOffset.X
   - Velocity  = Forward * ProjectileSpeed
   - Type, HitRadiusSq, Damage, LifetimeRemaining ← ULNPWeaponData에서 읽음
```

---

## 5. 어빌리티 발동 방식

### 5.1 Input Action 기반 (무기 기본 공격, Active Skill)

`ALNPPlayerCharacter::SetupPlayerInputComponent`에서 EnhancedInput과 연결 예정:

```cpp
// 기본 공격 — 무기 슬롯의 GA와 TAG_Input_Attack 연결
// Active Skill 슬롯 0~N — 각 슬롯 GA와 TAG_Input_Skill_0 ~ N 연결
```

무기 GA는 `AbilityTags`에 `LNP.Ability.Weapon.BasicAttack` 태그를 보유. 무기 교체 시 Input 재바인딩 불필요.

### 5.2 자동 발동 (Passive Skill)

| 발동 방식 | 예시 |
|:---|:---|
| GameplayEvent 수신 | 피격 이벤트 → 순간 무적 GA 발동 |
| GameplayTag 감지 | `LNP.State.LowHealth` 태그 부여 시 → 속도 증가 GA 발동 |
| Attribute Threshold GE | Health < MaxHealth * 0.3 → Tag 자동 부여 GE 상시 적용 |

---

## 6. Config 설정

`ULNPSettings` (`Config/LNPSettings.h`):

```cpp
UPROPERTY(Config, EditAnywhere, Category = "Ability System", meta=(ClampMin="1", ClampMax="8"))
int32 MaxActiveSkillSlots = 4;
```

---

## 7. 디렉토리 구조

```
Source/LootNPop/
  GAS/
    Attributes/
      LNPBaseAttributeSet.h/.cpp
    Abilities/
      LNPGameplayAbility.h/.cpp
      LNPAbility_BasicAttack.h/.cpp      ← 추상 기반 (GetEquippedWeaponDef 제공)
      LNPAbility_RangedAttack.h/.cpp     ← 구현 완료
      LNPAbility_MeleeAttack.h/.cpp      ← 미구현
  Item/
    LNPItemDefinitionBase.h/.cpp         ← 추상 기반 DataAsset
    LNPWeaponData.h                      ← 무기 DataAsset
    LNPSkillData.h                       ← 스킬 DataAsset
    LNPBuffData.h                        ← 버프 DataAsset
    LNPItemInstance.h                    ← FLNPWeaponInstance / FLNPSkillInstance / FLNPBuffInstance
    LNPEquipmentComponent.h/.cpp
    LNPInventoryComponent.h/.cpp
  HitDetection/
    LNPProjectileMassTypes.h             ← ELNPProjectileType, FLNPProjectileFragment
    LNPProjectileProcessor.h/.cpp        ← ULNPProjectileMovementProcessor
```

---

## 8. 미구현 항목

| 기능 | 비고 |
|:---|:---|
| DA_Pistol DataAsset 생성 및 설정 | DefaultWeapon에 할당 필요 |
| 기본 공격 Input Action 바인딩 | `SetupPlayerInputComponent` 연결 |
| `ULNPAbility_MeleeAttack` | HitDetection 구현 이후 |
| HitDetection (충돌 판정) | [TechDesign_HitDetection.md](TechDesign_HitDetection.md) 참조 |
| Passive Skill GameplayEvent 연결 | HitDetection 구현 이후 |
| LootPod 보상 드랍 → 인벤토리 연동 | [TechDesign_LootPod.md](TechDesign_LootPod.md) §4.2 참조 |
