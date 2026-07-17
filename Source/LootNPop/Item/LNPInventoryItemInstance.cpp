// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPInventoryItemInstance.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPInventoryComponent.h"
#include "Net/UnrealNetwork.h"

ULNPInventoryItemInstance::ULNPInventoryItemInstance()
{
}

void ULNPInventoryItemInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ULNPInventoryItemInstance, Definition);
	DOREPLIFETIME(ULNPInventoryItemInstance, ItemId);
	DOREPLIFETIME(ULNPInventoryItemInstance, StatTags);
	DOREPLIFETIME(ULNPInventoryItemInstance, bEquipped);
	DOREPLIFETIME(ULNPInventoryItemInstance, RemainingDuration);
}

void ULNPInventoryItemInstance::Init(ULNPItemDefinitionBase* InDefinition)
{
	Definition = InDefinition;
	if (!ItemId.IsValid())
	{
		ItemId = FGuid::NewGuid();
	}
}

void ULNPInventoryItemInstance::SetEquipped(bool bInEquipped)
{
	if (bEquipped == bInEquipped)
		return;

	bEquipped = bInEquipped;
	// 서버(리슨 호스트) UI 즉시 갱신 — 원격 클라는 OnRep_InstanceChanged가 담당.
	NotifyOwnerInventoryChanged();
}

void ULNPInventoryItemInstance::OnRep_InstanceChanged()
{
	NotifyOwnerInventoryChanged();
}

void ULNPInventoryItemInstance::NotifyOwnerInventoryChanged() const
{
	if (ULNPInventoryComponent* Comp = Cast<ULNPInventoryComponent>(GetOuter()))
		Comp->OnInventoryChanged.Broadcast();
}
