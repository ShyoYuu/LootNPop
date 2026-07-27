// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPInventoryList.h"
#include "Item/LNPInventoryComponent.h"
#include "Item/LNPInventoryItemInstance.h"
#include "LootNPop.h"

void FLNPInventoryList::AddEntry(ULNPInventoryItemInstance* Instance)
{
	if (Instance == nullptr)
	{
		return;
	}

	FLNPInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Instance = Instance;
	MarkItemDirty(NewEntry);
}

void FLNPInventoryList::RemoveEntry(ULNPInventoryItemInstance* Instance)
{
	for (auto It = Entries.CreateIterator(); It; ++It)
	{
		if (It->Instance == Instance)
		{
			It.RemoveCurrent();
			MarkArrayDirty();
			return;
		}
	}
}

void FLNPInventoryList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	// Part B 수직 슬라이스 검증: 소유 클라이언트가 인스턴스를 수신했음을 로그로 확인.
	UE_LOG(LogLootNPop, Log, TEXT("[Inventory] PostReplicatedAdd: %d entries added, bag size=%d (client receipt of item instance)"),
		AddedIndices.Num(), FinalSize);

	// 잔여 시간 스냅샷의 카운트다운 기준을 수신 시점으로 잡는다 (UI 라이브 표시용).
	for (const int32 Index : AddedIndices)
	{
		if (Entries.IsValidIndex(Index) && Entries[Index].Instance != nullptr)
			Entries[Index].Instance->MarkDurationStart();
	}
	NotifyOwnerChanged();
}

void FLNPInventoryList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	NotifyOwnerChanged();
}

void FLNPInventoryList::PostReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	NotifyOwnerChanged();
}

void FLNPInventoryList::NotifyOwnerChanged()
{
	if (OwnerComponent != nullptr)
	{
		OwnerComponent->OnInventoryChanged.Broadcast();
	}
}
