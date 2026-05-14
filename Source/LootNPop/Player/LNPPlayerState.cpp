// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Player/LNPPlayerState.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPInventoryComponent.h"

#include "AbilitySystemComponent.h"

ALNPPlayerState::ALNPPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// AttributeSet is auto-registered with the ASC because it is a subobject of the same Actor.
	BaseAttributeSet = CreateDefaultSubobject<ULNPBaseAttributeSet>(TEXT("BaseAttributeSet"));

	EquipmentComponent = CreateDefaultSubobject<ULNPEquipmentComponent>(TEXT("EquipmentComponent"));
	InventoryComponent = CreateDefaultSubobject<ULNPInventoryComponent>(TEXT("InventoryComponent"));

	SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* ALNPPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
