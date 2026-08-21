// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPInventoryItemInstance.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPInventoryComponent.h"
#include "LNPGameplayTags.h"
#include "Engine/World.h"
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
	DOREPLIFETIME(ULNPInventoryItemInstance, ChangeCounter);
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

int32 ULNPInventoryItemInstance::GetItemLevel() const
{
	return FMath::Max(1, GetStatTagStackCount(TAG_Item_Level));
}

void ULNPInventoryItemInstance::SetItemLevel(int32 InLevel)
{
	const int32 NewLevel = FMath::Max(1, InLevel);
	const int32 Delta = NewLevel - GetStatTagStackCount(TAG_Item_Level);
	if (Delta == 0)
		return;

	if (Delta > 0)
		AddStatTagStack(TAG_Item_Level, Delta);
	else
		RemoveStatTagStack(TAG_Item_Level, -Delta);

	// 원격 클라에는 이 카운터 복제가 OnRep_InstanceChanged를 울려 통지한다 (StatTags 도착 순서와 무관).
	++ChangeCounter;
	// 서버(리슨 호스트)는 자기 자신에게 복제가 없으므로 여기서 직접 통지한다.
	NotifyOwnerInventoryChanged();
}

void ULNPInventoryItemInstance::SetRemainingDuration(float InSeconds)
{
	RemainingDuration = InSeconds;
	MarkDurationStart();
}

void ULNPInventoryItemInstance::MarkDurationStart()
{
	const UWorld* World = GetWorld();
	DurationStartTime = (World != nullptr) ? World->GetTimeSeconds() : 0.0;
}

float ULNPInventoryItemInstance::GetRemainingDurationLive() const
{
	const UWorld* World = GetWorld();
	if (RemainingDuration <= 0.0f || DurationStartTime <= 0.0 || World == nullptr)
		return RemainingDuration;

	const double Elapsed = World->GetTimeSeconds() - DurationStartTime;
	return FMath::Max(0.0f, RemainingDuration - static_cast<float>(Elapsed));
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
