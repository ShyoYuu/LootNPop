// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPInventoryList.h"
#include "Item/LNPInventoryComponent.h"
#include "Item/LNPInventoryItemInstance.h"
#include "LootNPop.h"

#include "Engine/World.h"
#include "TimerManager.h"

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

void FLNPInventoryList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	// 항목이 아직 배열에 있으므로 지금 통지하면 UI가 사라진 항목을 그대로 다시 그린다.
	// 실제 제거가 끝난 뒤(같은 프레임 후반)를 보장하려고 다음 틱으로 미룬다.
	// 여러 항목이 한 프레임에 빠지면 통지가 몇 번 겹칠 수 있으나, 목록 재구성은 멱등이라 무해하다.
	if (OwnerComponent == nullptr)
		return;

	UWorld* World = OwnerComponent->GetWorld();
	if (World == nullptr)
		return;

	ULNPInventoryComponent* Component = OwnerComponent;
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(Component, [Component]()
		{
			Component->OnInventoryChanged.Broadcast();
		}));
}

// Iris를 끈 클래식 복제 경로 전용 — Iris에서는 호출되지 않는다 (헤더 주석 참조).
// 이쪽은 배열이 이미 줄어든 뒤라 즉시 통지해도 된다.
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
