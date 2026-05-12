// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPInventoryComponent.h"
#include "Item/LNPBuffData.h"
#include "Player/LNPPlayerState.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

ULNPInventoryComponent::ULNPInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULNPInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

void ULNPInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (BuffItems.Num() > 0)
		TickBuffItems(DeltaTime);
}

void ULNPInventoryComponent::AddToStorage(ULNPItemDefinitionBase* ItemDef)
{
	if (ItemDef)
		StoredItems.Add(ItemDef);
}

bool ULNPInventoryComponent::RemoveFromStorage(ULNPItemDefinitionBase* ItemDef)
{
	return StoredItems.Remove(ItemDef) > 0;
}

void ULNPInventoryComponent::AddBuffItem(ULNPBuffData* ItemDef, float InRemainingDuration)
{
	if (!ItemDef) return;

	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC) return;

	FLNPBuffInstance& Instance = BuffItems.AddDefaulted_GetRef();
	Instance.Definition = ItemDef;

	const float Duration = (InRemainingDuration > 0.0f) ? InRemainingDuration : ItemDef->MaxDuration;
	Instance.RemainingDuration = Duration;

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	for (const TSubclassOf<UGameplayEffect>& EffectClass : ItemDef->EffectsToApply)
	{
		if (!EffectClass) continue;
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
		if (!Spec.IsValid()) continue;
		Instance.AppliedEffects.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
	}
}

float ULNPInventoryComponent::RemoveBuffItem(ULNPBuffData* ItemDef)
{
	for (int32 i = 0; i < BuffItems.Num(); ++i)
	{
		if (BuffItems[i].Definition == ItemDef)
		{
			const float Remaining = BuffItems[i].RemainingDuration;
			ExpireBuffItem(i);
			return Remaining;
		}
	}
	return 0.0f;
}

UAbilitySystemComponent* ULNPInventoryComponent::GetASC() const
{
	if (const ALNPPlayerState* PS = Cast<ALNPPlayerState>(GetOwner()))
		return PS->GetAbilitySystemComponent();
	return nullptr;
}

void ULNPInventoryComponent::TickBuffItems(float DeltaTime)
{
	for (int32 i = BuffItems.Num() - 1; i >= 0; --i)
	{
		FLNPBuffInstance& Instance = BuffItems[i];
		if (Instance.RemainingDuration <= 0.0f) continue;

		Instance.RemainingDuration -= DeltaTime;
		if (Instance.RemainingDuration <= 0.0f)
			ExpireBuffItem(i);
	}
}

void ULNPInventoryComponent::ExpireBuffItem(int32 Index)
{
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		for (const FActiveGameplayEffectHandle& Handle : BuffItems[Index].AppliedEffects)
			ASC->RemoveActiveGameplayEffect(Handle);
	}
	BuffItems.RemoveAt(Index);
}
