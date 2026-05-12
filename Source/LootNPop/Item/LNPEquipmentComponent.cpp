// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPWeaponData.h"
#include "Item/LNPSkillData.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "Player/LNPPlayerState.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Config/LNPSettings.h"

ULNPEquipmentComponent::ULNPEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULNPEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ULNPSettings* Settings = GetDefault<ULNPSettings>())
		MaxActiveSkillSlots = Settings->MaxActiveSkillSlots;

	ActiveSkillSlots.SetNum(MaxActiveSkillSlots);

	if (DefaultWeapon)
		EquipWeapon(DefaultWeapon);
}

void ULNPEquipmentComponent::EquipWeapon(ULNPWeaponData* WeaponDef)
{
	UnequipWeapon();
	if (WeaponDef)
	{
		WeaponSlot.Definition = WeaponDef;
		GrantItemImpl(WeaponDef, WeaponSlot.GrantedAbilities, WeaponSlot.AppliedEffects);
	}
}

void ULNPEquipmentComponent::UnequipWeapon()
{
	if (!WeaponSlot.IsValid()) return;
	RevokeItemImpl(WeaponSlot.GrantedAbilities, WeaponSlot.AppliedEffects);
	WeaponSlot.Reset();
}

void ULNPEquipmentComponent::EquipActiveSkill(int32 SlotIndex, ULNPSkillData* SkillDef)
{
	if (!ActiveSkillSlots.IsValidIndex(SlotIndex)) return;
	UnequipActiveSkill(SlotIndex);
	if (SkillDef)
	{
		ActiveSkillSlots[SlotIndex].Definition = SkillDef;
		GrantItemImpl(SkillDef, ActiveSkillSlots[SlotIndex].GrantedAbilities, ActiveSkillSlots[SlotIndex].AppliedEffects);
	}
}

void ULNPEquipmentComponent::UnequipActiveSkill(int32 SlotIndex)
{
	if (!ActiveSkillSlots.IsValidIndex(SlotIndex)) return;
	FLNPSkillInstance& Slot = ActiveSkillSlots[SlotIndex];
	if (!Slot.IsValid()) return;
	RevokeItemImpl(Slot.GrantedAbilities, Slot.AppliedEffects);
	Slot.Reset();
}

void ULNPEquipmentComponent::AddPassiveSkill(ULNPSkillData* SkillDef)
{
	if (!SkillDef) return;
	FLNPSkillInstance& NewInstance = PassiveSkillInstances.AddDefaulted_GetRef();
	NewInstance.Definition = SkillDef;
	GrantItemImpl(SkillDef, NewInstance.GrantedAbilities, NewInstance.AppliedEffects);
}

void ULNPEquipmentComponent::RemovePassiveSkill(ULNPSkillData* SkillDef)
{
	for (int32 i = 0; i < PassiveSkillInstances.Num(); ++i)
	{
		if (PassiveSkillInstances[i].Definition == SkillDef)
		{
			RevokeItemImpl(PassiveSkillInstances[i].GrantedAbilities, PassiveSkillInstances[i].AppliedEffects);
			PassiveSkillInstances.RemoveAt(i);
			return;
		}
	}
}

UAbilitySystemComponent* ULNPEquipmentComponent::GetASC() const
{
	if (const ALNPPlayerState* PS = Cast<ALNPPlayerState>(GetOwner()))
		return PS->GetAbilitySystemComponent();
	return nullptr;
}

void ULNPEquipmentComponent::GrantItemImpl(ULNPItemDefinitionBase* Def,
                                            TArray<FGameplayAbilitySpecHandle>& OutAbilities,
                                            TArray<FActiveGameplayEffectHandle>& OutEffects)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC || !Def) return;

	for (const TSubclassOf<ULNPGameplayAbility>& AbilityClass : Def->AbilitiesToGrant)
	{
		if (!AbilityClass) continue;
		OutAbilities.Add(ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass)));
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	for (const TSubclassOf<UGameplayEffect>& EffectClass : Def->EffectsToApply)
	{
		if (!EffectClass) continue;
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
		if (!Spec.IsValid()) continue;
		OutEffects.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
	}
}

void ULNPEquipmentComponent::RevokeItemImpl(TArray<FGameplayAbilitySpecHandle>& Abilities,
                                             TArray<FActiveGameplayEffectHandle>& Effects)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC) return;

	for (const FGameplayAbilitySpecHandle& Handle : Abilities)
		ASC->ClearAbility(Handle);
	for (const FActiveGameplayEffectHandle& Handle : Effects)
		ASC->RemoveActiveGameplayEffect(Handle);
}
