// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "Player/LNPPlayerState.h"
#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPItemInstance.h"
#include "Item/LNPWeaponData.h"

ULNPAbility_BasicAttack::ULNPAbility_BasicAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

const ULNPWeaponData* ULNPAbility_BasicAttack::GetEquippedWeaponDef() const
{
	const ALNPPlayerState* PS = GetOwningLNPPlayerState();
	if (nullptr == PS)
		return nullptr;

	const ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent();
	if (nullptr == EqComp)
		return nullptr;

	return EqComp->GetWeaponSlot().Definition;
}
