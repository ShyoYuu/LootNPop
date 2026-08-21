// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPWeaponData.h"
#include "Item/LNPSkillData.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Item/LNPInventoryComponent.h"
#include "Character/LNPCharacterBase.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "GAS/LNPStatModifier.h"
#include "Player/LNPPlayerState.h"
#include "Config/LNPSettings.h"
#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

ULNPEquipmentComponent::ULNPEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// WeaponSlot이 장비 상태의 단일 원본이므로 컴포넌트 자체가 복제되어야 한다.
	// PlayerState는 bAlwaysRelevant이라 시뮬레이티드 프록시를 포함한 전 클라이언트가 받는다.
	SetIsReplicatedByDefault(true);
}

void ULNPEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 조건 없음 — 프록시도 받아야 한다. 원격 관전 시 근접 판정이 무기 정의를 읽기 때문.
	// 실제 복제되는 것은 Definition 하나뿐이다 (나머지 필드는 NotReplicated, FLNPWeaponInstance 참조).
	DOREPLIFETIME(ULNPEquipmentComponent, WeaponSlot);
}

void ULNPEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ULNPSettings* Settings = GetDefault<ULNPSettings>())
		MaxActiveSkillSlots = Settings->MaxActiveSkillSlots;

	ActiveSkillSlots.SetNum(MaxActiveSkillSlots);

	// DefaultWeapon 지급은 여기서 하지 않는다 — EnsureDefaultWeapon() 주석 참조.
}

void ULNPEquipmentComponent::EnsureDefaultWeapon()
{
	if (!EnsureAuthority(TEXT("EnsureDefaultWeapon")))
		return;

	if (DefaultWeapon == nullptr)
		return;

	// 기본 무기도 가방 인스턴스를 거친다 — 그래야 인벤토리 UI·bEquipped·드랍 가드가
	// 루팅으로 얻은 무기와 완전히 같은 상태 기계를 탄다 (innate 특수 케이스 없음).
	const ALNPPlayerState* PS = Cast<ALNPPlayerState>(GetOwner());
	ULNPInventoryComponent* Inventory = PS ? PS->GetInventoryComponent() : nullptr;
	if (Inventory == nullptr)
	{
		// 인벤토리가 없는 소유자 — 정의만으로 장착한다 (SourceInstance 없음).
		if (!WeaponSlot.IsValid())
			EquipWeapon(DefaultWeapon);
		return;
	}

	// PlayerState는 폰 리스폰을 넘어 유지되므로, 재호출 시 사본이 쌓이지 않도록 조회를 먼저 한다.
	ULNPInventoryItemInstance* Instance = Inventory->FindBagInstanceByDefinition(DefaultWeapon);
	if (Instance == nullptr)
		Instance = Inventory->AddItemInstance(DefaultWeapon);  // 슬롯이 비었으면 TryAutoEquipWeapon이 장착한다

	// 이미 다른 무기를 들고 있으면 건드리지 않는다 — 리스폰마다 기본 무기로 되돌리면 안 된다.
	if (Instance && !WeaponSlot.IsValid())
		EquipWeaponInstance(Instance);
}

bool ULNPEquipmentComponent::EnsureAuthority(const TCHAR* Context) const
{
	return ensureMsgf(GetOwnerRole() == ROLE_Authority,
		TEXT("ULNPEquipmentComponent::%s called without authority — route through ALNPPlayerCharacter::Server_Equip*"),
		Context);
}

void ULNPEquipmentComponent::EquipWeapon(ULNPWeaponData* WeaponDef)
{
	if (!EnsureAuthority(TEXT("EquipWeapon")))
		return;

	ClearWeaponSlot();
	if (WeaponDef)
	{
		WeaponSlot.Definition = WeaponDef;
		GrantItemImpl(WeaponDef, /*Level=*/1, WeaponSlot.GrantedAbilities, WeaponSlot.AppliedEffects);
	}

	OnWeaponSlotApplied();
}

void ULNPEquipmentComponent::EquipWeaponInstance(ULNPInventoryItemInstance* Instance)
{
	if (!EnsureAuthority(TEXT("EquipWeaponInstance")))
		return;

	if (Instance == nullptr)
		return;

	// 같은 인스턴스를 다시 장착하는 요청은 무시한다 — 그대로 진행하면 ClearWeaponSlot이
	// bEquipped를 내렸다 올리고 GAS 어빌리티를 회수했다 재부여하는 헛돌기가 된다.
	if (WeaponSlot.SourceInstance == Instance)
		return;

	ULNPWeaponData* WeaponDef = Cast<ULNPWeaponData>(Instance->GetDefinition());
	if (WeaponDef == nullptr)
		return;

	ClearWeaponSlot();

	WeaponSlot.Definition = WeaponDef;
	WeaponSlot.SourceInstance = Instance;
	Instance->SetEquipped(true);  // 복제되어 소유 클라 UI가 가방에서 숨긴다
	GrantItemImpl(WeaponDef, Instance->GetItemLevel(), WeaponSlot.GrantedAbilities, WeaponSlot.AppliedEffects);

	OnWeaponSlotApplied();
}

void ULNPEquipmentComponent::UnequipWeapon()
{
	if (!EnsureAuthority(TEXT("UnequipWeapon")))
		return;

	if (!WeaponSlot.IsValid())
		return;

	ClearWeaponSlot();
	OnWeaponSlotApplied();
}

void ULNPEquipmentComponent::RefreshWeaponSlotGrants()
{
	if (!EnsureAuthority(TEXT("RefreshWeaponSlotGrants")))
		return;

	if (!WeaponSlot.IsValid() || WeaponSlot.SourceInstance == nullptr)
		return;

	// 회수 → 배열 비우기 → 새 레벨로 재부여. WeaponSlot.Definition·SourceInstance·bEquipped는 그대로다.
	RevokeItemImpl(WeaponSlot.GrantedAbilities, WeaponSlot.AppliedEffects);
	WeaponSlot.GrantedAbilities.Reset();
	WeaponSlot.AppliedEffects.Reset();

	GrantItemImpl(WeaponSlot.Definition, WeaponSlot.SourceInstance->GetItemLevel(),
		WeaponSlot.GrantedAbilities, WeaponSlot.AppliedEffects);

	// 비주얼은 변하지 않으므로(같은 정의) PushWeaponToPawn은 부르지 않는다. 스탯 탭만 다시 읽으면 된다.
	OnEquipmentChanged.Broadcast();
}

void ULNPEquipmentComponent::TryAutoEquipWeapon(ULNPInventoryItemInstance* Instance)
{
	if (!EnsureAuthority(TEXT("TryAutoEquipWeapon")))
		return;

	// 이미 무기를 들고 있으면 획득만으로 갈아입히지 않는다.
	if (WeaponSlot.IsValid())
		return;

	// 무기가 아닌 획득(스킬 등)은 무시한다.
	const ULNPWeaponData* WeaponDef = Cast<ULNPWeaponData>(Instance ? Instance->GetDefinition() : nullptr);
	if (WeaponDef == nullptr)
		return;

	EquipWeaponInstance(Instance);

	UE_LOG(LogLootNPop, Log, TEXT("[Equip] %s auto-equipped %s — weapon slot was empty"),
		*GetNameSafe(GetOwner()), *GetNameSafe(WeaponDef));
}

// 통지하지 않는다 — 교체(EquipWeapon)의 중간 단계로도 쓰이기 때문이다.
// 여기서 통지하면 무기를 바꿀 때마다 "맨손 → 새 무기" 두 번 적용되어 애님 레이어가 한 번 튄다.
void ULNPEquipmentComponent::ClearWeaponSlot()
{
	if (!WeaponSlot.IsValid())
		return;

	if (WeaponSlot.SourceInstance)
		WeaponSlot.SourceInstance->SetEquipped(false);
	RevokeItemImpl(WeaponSlot.GrantedAbilities, WeaponSlot.AppliedEffects);
	WeaponSlot.Reset();
}

void ULNPEquipmentComponent::OnRep_WeaponSlot()
{
	OnWeaponSlotApplied();
}

void ULNPEquipmentComponent::OnWeaponSlotApplied()
{
	PushWeaponToPawn();
	OnEquipmentChanged.Broadcast();
}

void ULNPEquipmentComponent::PushWeaponToPawn() const
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (PS == nullptr)
		return;

	// Pawn이 아직 없거나 PlayerState와 아직 연결되지 않았으면 아무것도 하지 않는다 —
	// Pawn 쪽이 BeginPlay/OnRep_PlayerState/PossessedBy에서 스스로 끌어간다 (풀 방향).
	if (ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(PS->GetPawn()))
		Character->ApplyWeaponVisuals(WeaponSlot.Definition);
}

void ULNPEquipmentComponent::EquipActiveSkill(int32 SlotIndex, ULNPSkillData* SkillDef)
{
	if (!ActiveSkillSlots.IsValidIndex(SlotIndex))
		return;

	UnequipActiveSkill(SlotIndex);
	if (SkillDef)
	{
		ActiveSkillSlots[SlotIndex].Definition = SkillDef;
		if (GetOwner() && GetOwner()->HasAuthority())
			GrantItemImpl(SkillDef, /*Level=*/1, ActiveSkillSlots[SlotIndex].GrantedAbilities, ActiveSkillSlots[SlotIndex].AppliedEffects);
	}
}

void ULNPEquipmentComponent::UnequipActiveSkill(int32 SlotIndex)
{
	if (!ActiveSkillSlots.IsValidIndex(SlotIndex))
		return;

	FLNPSkillInstance& Slot = ActiveSkillSlots[SlotIndex];
	if (!Slot.IsValid())
		return;

	if (GetOwner() && GetOwner()->HasAuthority())
		RevokeItemImpl(Slot.GrantedAbilities, Slot.AppliedEffects);
	Slot.Reset();
}

void ULNPEquipmentComponent::AddPassiveSkill(ULNPSkillData* SkillDef)
{
	if (!SkillDef)
		return;

	FLNPSkillInstance& NewInstance = PassiveSkillInstances.AddDefaulted_GetRef();
	NewInstance.Definition = SkillDef;
	if (GetOwner() && GetOwner()->HasAuthority())
		GrantItemImpl(SkillDef, /*Level=*/1, NewInstance.GrantedAbilities, NewInstance.AppliedEffects);
}

void ULNPEquipmentComponent::RemovePassiveSkill(ULNPSkillData* SkillDef)
{
	for (int32 i = 0; i < PassiveSkillInstances.Num(); ++i)
	{
		if (PassiveSkillInstances[i].Definition == SkillDef)
		{
			if (GetOwner() && GetOwner()->HasAuthority())
				RevokeItemImpl(PassiveSkillInstances[i].GrantedAbilities, PassiveSkillInstances[i].AppliedEffects);
			PassiveSkillInstances.RemoveAt(i);
			return;
		}
	}
}

bool ULNPEquipmentComponent::IsEquipped(const ULNPItemDefinitionBase* ItemDef) const
{
	if (ItemDef == nullptr)
		return false;

	if (WeaponSlot.Definition == ItemDef)
		return true;

	for (const FLNPSkillInstance& Slot : ActiveSkillSlots)
	{
		if (Slot.Definition == ItemDef)
			return true;
	}

	for (const FLNPSkillInstance& Passive : PassiveSkillInstances)
	{
		if (Passive.Definition == ItemDef)
			return true;
	}

	return false;
}

bool ULNPEquipmentComponent::IsEquippedInstance(const FGuid& ItemId) const
{
	if (!ItemId.IsValid())
		return false;

	if (WeaponSlot.SourceInstance && WeaponSlot.SourceInstance->GetItemId() == ItemId)
		return true;

	for (const FLNPSkillInstance& Slot : ActiveSkillSlots)
	{
		if (Slot.SourceInstance && Slot.SourceInstance->GetItemId() == ItemId)
			return true;
	}

	for (const FLNPSkillInstance& Passive : PassiveSkillInstances)
	{
		if (Passive.SourceInstance && Passive.SourceInstance->GetItemId() == ItemId)
			return true;
	}

	return false;
}

UAbilitySystemComponent* ULNPEquipmentComponent::GetASC() const
{
	if (const ALNPPlayerState* PS = Cast<ALNPPlayerState>(GetOwner()))
		return PS->GetAbilitySystemComponent();
	return nullptr;
}

void ULNPEquipmentComponent::GrantItemImpl(ULNPItemDefinitionBase* Def,
                                            int32 Level,
                                            TArray<FGameplayAbilitySpecHandle>& OutAbilities,
                                            TArray<FActiveGameplayEffectHandle>& OutEffects)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC || !Def)
		return;

	const int32 GrantLevel = FMath::Max(1, Level);

	for (const TSubclassOf<ULNPGameplayAbility>& AbilityClass : Def->AbilitiesToGrant)
	{
		if (!AbilityClass)
			continue;

		// 스펙 레벨 = 아이템 레벨. 어빌리티는 GetAbilityLevel()로 이 값을 읽어 무기 레벨 테이블의
		// 피해 계수를 찾는다 (ULNPAbility_BasicAttack::GetDamageCoefficient).
		OutAbilities.Add(ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, GrantLevel)));
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	for (const TSubclassOf<UGameplayEffect>& EffectClass : Def->EffectsToApply)
	{
		if (!EffectClass)
			continue;

		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
		if (!Spec.IsValid())
			continue;

		OutEffects.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
	}

	// 무기 스텟 등 선언형 스탯 변경. 해제는 RevokeItemImpl이 핸들로 처리한다.
	// 무기는 레벨 테이블의 해당 행이 스탯의 원본이다 (테이블이 없으면 베이스 StatModifiers로 폴백).
	TConstArrayView<FLNPStatModifier> Modifiers = Def->StatModifiers;
	if (const ULNPWeaponData* WeaponDef = Cast<ULNPWeaponData>(Def))
		Modifiers = WeaponDef->GetStatModifiersForLevel(GrantLevel);

	LNPStat::ApplyModifiers(*ASC, Modifiers, OutEffects);
}

void ULNPEquipmentComponent::RevokeItemImpl(TArray<FGameplayAbilitySpecHandle>& Abilities,
                                             TArray<FActiveGameplayEffectHandle>& Effects)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
		return;

	for (const FGameplayAbilitySpecHandle& Handle : Abilities)
		ASC->ClearAbility(Handle);
	for (const FActiveGameplayEffectHandle& Handle : Effects)
		ASC->RemoveActiveGameplayEffect(Handle);
}
