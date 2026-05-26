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
| AttackMultiplier | FGameplayAttributeData | — |

- `PreAttributeChange`: MaxHealth ≥ 1, AttackSpeed/MoveSpeed ≥ 0.01 클램핑.
- `PostGameplayEffectExecute`: Health를 [0, MaxHealth] 범위로 클램핑.

### 2.2 Ability 클래스 계층

```
UGameplayAbility
  └── ULNPGameplayAbility              ← 공통 기반 (캐릭터/PlayerState 레퍼런스 헬퍼)
        └── ULNPAbility_BasicAttack    ← 무기 기본 공격 추상 기반 (GetEquippedWeaponDef, ComputeDamage 제공)
              └── ULNPAbility_RangedAttack   ← 원거리 공격 (Mass Entity 생성 후 즉시 종료)
```

`ULNPGameplayAbility`는 `GetOwningCharacter()`, `GetOwningLNPPlayerState()` 헬퍼를 제공.

`ULNPAbility_BasicAttack`의 `ComputeDamage()`는 `(AttackPower + WeaponDamage) * AttackMultiplier` 공식으로 기본 피해량을 계산. 서브클래스에서 오버라이드 가능.

> `ULNPAbility_MeleeAttack`은 파일 미생성. HitDetection 이후 구현 예정.

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
+ AttackMontage          : UAnimMontage*
+ FireCooldown           : float  (default 0.2)         ← 연사 쿨타임 (GE로 적용)
+ ProjectileType         : ELNPProjectileType  (Linear / Guided / Lobbed)
+ ProjectileSpeed        : float  (default 5000)
+ ProjectileHitRadius    : float  (default 5)
+ ProjectileDamage       : float  (default 10)
+ ProjectileLifetime     : float  (default 5)
+ MuzzleOffset           : FVector (default (100,0,0))
+ ProjectileDamageEffect : TSubclassOf<UGameplayEffect>  ← 피격 대상에 적용할 GE
+ ProjectileVFXData      : ULNPVFXData*                  ← 발사체 스폰/비행/충돌 VFX
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

무기별 공통 데이터와 발사체별 고유 상태를 분리해 메모리 효율을 높인다.

```cpp
UENUM() enum class ELNPProjectileType  : uint8 { Linear, Guided, Lobbed };
UENUM() enum class ELNPInstigatorTeam  : uint8 { Enemy, Player };

// 같은 무기에서 발사된 모든 발사체가 공유하는 읽기 전용 상수 (FMassConstSharedFragment)
USTRUCT() struct FLNPProjectileSharedFragment : public FMassConstSharedFragment {
    TObjectPtr<ULNPVFXData>          VFXData;
    TSubclassOf<UGameplayEffect>     DamageEffectClass;
    ELNPProjectileType               Type;
    float Damage      = 10.0f;
    float HitRadiusSq = 25.0f;
};

// 발사체별 시뮬레이션 상태 (FMassFragment)
USTRUCT() struct FLNPProjectileFragment : public FMassFragment {
    FVector PreviousPos;     // 전 프레임 위치 → HitDetection 선분 시작점
    FVector Velocity;
    FVector SpawnLocation;   // VFX 스폰에 한 번 사용
    float   LifetimeRemaining;
    FMassEntityHandle Instigator;
    ELNPInstigatorTeam InstigatorTeam;
};

// VFX(Niagara trail) 할당 여부 추적
USTRUCT() struct FLNPProjectileVisualFragment : public FMassFragment {
    bool bInitialized = false;
};

// 파괴 대기 마킹용 태그 (PostPhysics에서 일괄 제거)
USTRUCT() struct FLNPProjectileDeadTag : public FMassTag {};
```

> 이전 설계의 `CurrentPos`는 제거됨. 각 프레임 `PreviousPos` 갱신 후 `Velocity * DeltaTime`으로 현재 위치를 산출.

### 4.3 발사체 프로세서 (`HitDetection/LNPProjectileProcessors.h/.cpp`)

4개의 프로세서가 순서대로 실행된다.

| 프로세서 | 실행 단계 | 역할 |
|:---|:---|:---|
| `ULNPProjectileMovementProcessor` | PrePhysics | 위치 갱신, 수명 만료/지형 충돌 처리, `FLNPProjectileDeadTag` 추가 |
| `ULNPProjectileHitDetectionProcessor` | StartPhysics (Movement 이후) | 선분 캡슐 충돌 검사, 피해 적용, `FLNPProjectileDeadTag` 추가 |
| `ULNPProjectileVisualizationProcessor` | 게임 스레드 (HitDetection 이후) | Niagara trail 컴포넌트 스폰/갱신, 충돌 VFX 출력 |
| `ULNPProjectileDestructionProcessor` | PostPhysics | `FLNPProjectileDeadTag` 엔티티 일괄 제거 |

에디터 전용 (`#if WITH_EDITOR`): `ULNPProjectileDebugDrawProcessor` — 발사체 위치/속도 시각화 및 충돌 마커 표시.

**이동 루프 (MovementProcessor):**
```
PreviousPos = 현재 위치
CurrentPos  = PreviousPos + Velocity * DeltaTime  (TransformFragment 갱신)
LifetimeRemaining -= DeltaTime
LifetimeRemaining <= 0 → FLNPProjectileDeadTag 추가 (Defer)
```

### 4.4 스폰 흐름 (`ULNPAbility_RangedAttack::SpawnProjectile`)

```
1. UMassEntitySubsystem::GetMutableEntityManager()

2. FLNPProjectileSharedFragment 구성 (무기별 공통 데이터)
   - VFXData, DamageEffectClass, Type, HitRadiusSq ← ULNPWeaponData
   - Damage ← ComputeDamage()  (AttackPower + WeaponDamage) * AttackMultiplier
   EntityManager.GetOrCreateConstSharedFragment(SharedData)

3. EntityManager.ReserveEntity()  ← ID만 선점, 처리 중 안전

4. FLNPProjectileFragment 초기화
   - PreviousPos = SpawnLocation = ActorLocation + Forward * MuzzleOffset.X
   - Velocity = Forward * ProjectileSpeed
   - Instigator = MassAgentComponent->GetEntityHandle()
   - InstigatorTeam = (Enemy / Player)

5. EntityManager.Defer().PushCommand<FMassCommandBuildEntityWithSharedFragments<
       FLNPProjectileFragment,
       FLNPProjectileVisualFragment,
       FTransformFragment>>(Entity, SharedValues, ...)
   ← 현재 처리 스코프 종료 후 실제 엔티티 빌드
```

---

## 5. 어빌리티 발동 방식

### 5.1 Input Action 기반 (무기 기본 공격, Active Skill)

기본 공격 입력은 `ULNPInputHandlerComponent`에서 `AttackAction`으로 수신, `ALNPCharacterBase::TryActivateAttack()`을 호출한다. `ALNPPlayerCharacter`의 구현은 아래와 같다:

```cpp
// ALNPPlayerCharacter::TryActivateAttack()
return ASC->TryActivateAbility(WeaponSlot.GrantedAbilities[0]);
```

입력 버퍼링 지원: 쿨타임 중 공격 입력이 오면 0.05초간 버퍼에 보관, 쿨타임 해제 직후 재시도.

무기 교체 시 `GrantedAbilities[0]`이 새 무기의 GA 핸들로 교체되므로 재바인딩 불필요.

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
      LNPAbility_BasicAttack.h/.cpp      ← 추상 기반 (GetEquippedWeaponDef, ComputeDamage 제공)
      LNPAbility_RangedAttack.h/.cpp     ← 구현 완료
    Effects/
      LNPGameplayEffect_Cooldown.h/.cpp  ← 연사 쿨타임 GE (duration은 ability가 주입)
      LNPGameplayEffect_Damage.h/.cpp    ← 즉발 피해 GE (TAG_GE_Data_Damage SetByCaller)
  Item/
    LNPItemDefinitionBase.h/.cpp         ← 추상 기반 DataAsset
    LNPWeaponData.h                      ← 무기 DataAsset
    LNPSkillData.h                       ← 스킬 DataAsset
    LNPBuffData.h                        ← 버프 DataAsset
    LNPItemInstance.h                    ← FLNPWeaponInstance / FLNPSkillInstance / FLNPBuffInstance
    LNPEquipmentComponent.h/.cpp
    LNPInventoryComponent.h/.cpp
  HitDetection/
    LNPProjectileMassTypes.h             ← 타입 정의 (FLNPProjectileSharedFragment / Fragment / VisualFragment / DeadTag)
    LNPProjectileProcessors.h/.cpp       ← 4개 프로세서 (Movement / HitDetection / Visualization / Destruction)
    LNPProjectileVisualSubsystem.h/.cpp  ← Niagara trail 컴포넌트 풀 관리
```

---

## 8. 구현 현황 및 다음 단계

### 8.1 완료된 항목

**GAS 코어**

| 항목 | 세부 내용 |
|:---|:---|
| ASC + AttributeSet | `ALNPPlayerState`에 `UAbilitySystemComponent`, `ULNPBaseAttributeSet` 부착. 7개 어트리뷰트 정의 |
| `InitAbilityActorInfo` | 서버: `PossessedBy`, 클라이언트: `OnRep_PlayerState`에서 각각 호출 |
| 아이템 DataAsset 계층 | `ULNPItemDefinitionBase`(Abstract) → Weapon / Skill / Buff 분리, `GetPrimaryAssetId` 구현 |
| 아이템 인스턴스 구조체 | `FLNPWeaponInstance` / `FLNPSkillInstance` / `FLNPBuffInstance` (`IsValid`, `Reset` 포함) |
| `ULNPEquipmentComponent` | 무기 슬롯(1), Active Skill 슬롯(N, `ULNPSettings`에서 로드), Passive 목록. `GrantItemImpl`/`RevokeItemImpl` 공통 처리 |
| `ULNPInventoryComponent` | 보관함 배열 + 버프 아이템 Tick 추적 → 만료 시 `ExpireBuffItem()` 자동 호출 |

**어빌리티 클래스 및 GE**

| 항목 | 세부 내용 |
|:---|:---|
| `ULNPGameplayAbility` | `GetOwningCharacter()`, `GetOwningLNPPlayerState()` 헬퍼 |
| `ULNPAbility_BasicAttack` | `GetEquippedWeaponDef()`, `ComputeDamage()` (`(AttackPower + WeaponDamage) × AttackMultiplier`) |
| `ULNPAbility_RangedAttack` | `CommitAbility` → `SpawnProjectile()` → `EndAbility()`. `FireCooldown` GE per-spec 주입 |
| `ULNPGameplayEffect_Cooldown` | Duration은 ability가 per-spec 주입하여 단일 클래스로 모든 무기 커버 |
| `ULNPGameplayEffect_Damage` | Instant GE, `TAG_GE_Data_Damage` SetByCaller로 피해량 전달 |

**발사체 시스템**

| 항목 | 세부 내용 |
|:---|:---|
| `FLNPProjectileSharedFragment` | 같은 무기의 발사체가 공유하는 ConstShared 상수 (VFX, DamageGE, Type, Damage, HitRadiusSq) |
| `FLNPProjectileFragment` | 발사체별 상태 (PreviousPos, Velocity, SpawnLocation, Lifetime, Instigator, InstigatorTeam) |
| `FLNPProjectileVisualFragment` | Niagara trail 초기화 여부 추적 |
| `FLNPProjectileDeadTag` | 파괴 대기 마킹. PostPhysics에서 일괄 제거 |
| `ULNPProjectileMovementProcessor` | PrePhysics: 위치 갱신, 수명 만료, 지형 충돌 처리 |
| `ULNPProjectileHitDetectionProcessor` | StartPhysics: 선분-캡슐 충돌, 피해 GE 적용. `ELNPInstigatorTeam`으로 팀 피격 제어 |
| `ULNPProjectileVisualizationProcessor` | 게임 스레드: Niagara trail 스폰/갱신, 충돌 VFX 출력 |
| `ULNPProjectileDestructionProcessor` | PostPhysics: `FLNPProjectileDeadTag` 엔티티 일괄 제거 |
| `ULNPProjectileVisualSubsystem` | Niagara trail 컴포넌트 풀 관리 (VFX 에셋 미연결 상태, 에디터 디버그드로우로 동작 확인 중) |

**입력 연결**

| 항목 | 세부 내용 |
|:---|:---|
| 기본 공격 입력 | `InputHandlerComponent` `AttackAction` → `TryActivateAttack()` → `ASC->TryActivateAbility(GrantedAbilities[0])` |
| 공격 입력 버퍼링 | 쿨타임 중 0.05초 버퍼 보관, 쿨타임 해제 직후 재시도 |

---

### 8.2 미구현 항목

| 항목 | 선행 조건 | 세부 내용 |
|:---|:---|:---|
| 발사체 Niagara VFX | 없음 | `ULNPVFXData` 에셋 생성 후 `ULNPWeaponData.ProjectileVFXData`에 할당. `ULNPProjectileVisualizationProcessor`가 trail/impact 처리 |
| Active Skill 입력 바인딩 | 없음 | `InputHandlerComponent`에 슬롯별 `SkillAction` 추가 및 `ASC->TryActivateAbility(ActiveSkillSlots[i].GrantedAbilities[0])` 연결 |
| `ULNPAbility_MeleeAttack` | 근접 HitDetection | 파일 미생성. 공격 몽타주 특정 프레임에서 캡슐 스윕 실행하는 방식으로 설계 필요 |
| 근접 HitDetection | 없음 | 몽타주 Notify 기반 스윕 또는 별도 Mass Processor 처리 방식 결정 필요 |
| Passive Skill GameplayEvent 연결 | HitDetection | `HitDetectionProcessor`에서 피격 시 대상 ASC에 `SendGameplayEvent` 호출하는 로직 추가 필요 |
| LootPod → 인벤토리 연동 | LootPod 시스템 | [TechDesign_LootPod.md](TechDesign_LootPod.md) §4.2 참조 |
