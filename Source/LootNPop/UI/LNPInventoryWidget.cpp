// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/LNPInventoryWidget.h"
#include "Item/LNPInventoryComponent.h"
#include "Item/LNPInventoryItemInstance.h"

#include "Components/ListView.h"

void ULNPInventoryWidget::InitViewModel(ULNPInventoryComponent* InInventory)
{
	// 재초기화 안전망 — 기존 구독을 먼저 해제한다.
	DeinitViewModel();

	BoundInventory = InInventory;
	if (InInventory != nullptr)
		InInventory->OnInventoryChanged.AddDynamic(this, &ULNPInventoryWidget::RefreshLists);

	RefreshLists();
}

void ULNPInventoryWidget::DeinitViewModel()
{
	if (ULNPInventoryComponent* Inventory = BoundInventory.Get())
		Inventory->OnInventoryChanged.RemoveDynamic(this, &ULNPInventoryWidget::RefreshLists);
	BoundInventory.Reset();
}

void ULNPInventoryWidget::RefreshLists()
{
	ULNPInventoryComponent* Inventory = BoundInventory.Get();
	if (Inventory == nullptr)
		return;

	// 보관 — 아이템 인스턴스(UObject) 중 미장착만 노출 (장착본은 장비 슬롯이 표시).
	if (StorageList != nullptr)
	{
		StorageList->ClearListItems();
		for (ULNPInventoryItemInstance* Instance : Inventory->GetBagInstances())
		{
			if (Instance != nullptr && !Instance->IsEquipped())
				StorageList->AddItem(Instance);
		}
	}

	// 버프 — 활성 버프 인스턴스를 직접 노출 (별도 래퍼 불필요).
	if (BuffList != nullptr)
	{
		BuffList->ClearListItems();
		for (ULNPInventoryItemInstance* Instance : Inventory->GetActiveBuffInstances())
		{
			if (Instance != nullptr)
				BuffList->AddItem(Instance);
		}
	}
}

void ULNPInventoryWidget::NativeDestruct()
{
	DeinitViewModel();
	Super::NativeDestruct();
}
