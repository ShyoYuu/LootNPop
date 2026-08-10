// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPInventoryTabWidget.h"
#include "UI/Menu/LNPItemDetailPanelWidget.h"
#include "UI/Menu/LNPMenuItemCellWidget.h"
#include "Item/LNPInventoryComponent.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Player/LNPPlayerState.h"

#include "CommonTileView.h"

void ULNPInventoryTabWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ItemGrid)
	{
		ItemGrid->OnItemSelectionChanged().AddUObject(this, &ULNPInventoryTabWidget::HandleGridSelectionChanged);
		ItemGrid->OnEntryWidgetGenerated().AddUObject(this, &ULNPInventoryTabWidget::HandleEntryGenerated);
	}
}

void ULNPInventoryTabWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 탭에 들어올 때는 항상 Grid 포커스에서 시작한다.
	bDetailFocused = false;

	if (const ALNPPlayerState* PS = GetOwningPlayerState<ALNPPlayerState>())
	{
		BoundInventory = PS->GetInventoryComponent();
		if (ULNPInventoryComponent* Inventory = BoundInventory.Get())
			Inventory->OnInventoryChanged.AddDynamic(this, &ULNPInventoryTabWidget::RefreshGrid);
	}

	RefreshGrid();
}

void ULNPInventoryTabWidget::NativeOnDeactivated()
{
	if (ULNPInventoryComponent* Inventory = BoundInventory.Get())
		Inventory->OnInventoryChanged.RemoveDynamic(this, &ULNPInventoryTabWidget::RefreshGrid);
	BoundInventory.Reset();

	Super::NativeOnDeactivated();
}

void ULNPInventoryTabWidget::RefreshGrid()
{
	if (ItemGrid == nullptr)
		return;

	ULNPInventoryComponent* Inventory = BoundInventory.Get();
	if (Inventory == nullptr)
	{
		ItemGrid->ClearListItems();
		if (DetailPanel)
			DetailPanel->SetItem(nullptr);
		return;
	}

	// 가방(장착본 포함) + 활성 버프를 한 Grid에 합친다 — 기획 §6-1.
	TArray<UObject*> Items;
	for (ULNPInventoryItemInstance* Instance : Inventory->GetBagInstances())
	{
		if (Instance)
			Items.Add(Instance);
	}
	for (ULNPInventoryItemInstance* Instance : Inventory->GetActiveBuffInstances())
	{
		if (Instance)
			Items.Add(Instance);
	}

	// 갱신 후에도 같은 아이템을 계속 보고 있도록 선택을 복원한다.
	UObject* PreviousSelection = ItemGrid->GetSelectedItem<UObject>();

	ItemGrid->SetListItems(Items);

	if (PreviousSelection && Items.Contains(PreviousSelection))
		ItemGrid->SetSelectedItem(PreviousSelection);
	else if (Items.Num() > 0)
		ItemGrid->SetSelectedItem(Items[0]);
	else if (DetailPanel)
		DetailPanel->SetItem(nullptr);

	// ⚠️ SetListItems만으로는 **항목 내부 변화**가 셀에 반영되지 않는다.
	// 장착/해제는 목록의 추가·제거가 아니라 같은 인스턴스의 플래그 변경이라, 목록이
	// 포인터·순서까지 동일해 SListView가 기존 행을 그대로 재사용하고
	// NativeOnListItemObjectSet을 다시 호출하지 않는다 → 장착 배지("E")가 갱신되지 않는다.
	// 엔진도 이 경우 RegenerateAllEntries를 권한다 (ListViewBase.h의 RequestRefresh 주석).
	ItemGrid->RegenerateAllEntries();
}

void ULNPInventoryTabWidget::HandleGridSelectionChanged(UObject* SelectedItem)
{
	if (DetailPanel)
		DetailPanel->SetItem(Cast<ULNPInventoryItemInstance>(SelectedItem));
}

void ULNPInventoryTabWidget::HandleEntryGenerated(UUserWidget& EntryWidget)
{
	if (ULNPMenuItemCellWidget* Cell = Cast<ULNPMenuItemCellWidget>(&EntryWidget))
	{
		// 셀은 재사용되므로 중복 등록을 피하기 위해 먼저 해제한다.
		Cell->OnCellActivated.RemoveAll(this);
		Cell->OnCellActivated.AddUObject(this, &ULNPInventoryTabWidget::HandleCellActivated);
	}
}

void ULNPInventoryTabWidget::HandleCellActivated(ULNPInventoryItemInstance* Instance)
{
	if (DetailPanel)
		DetailPanel->SetItem(Instance);

	FocusDetailPanel();
}

void ULNPInventoryTabWidget::FocusDetailPanel()
{
	if (DetailPanel == nullptr)
		return;

	UWidget* Target = DetailPanel->GetFirstFocusTarget();
	if (Target == nullptr)
		return;   // 누를 수 있는 버튼이 없으면 Grid 포커스를 유지한다.

	bDetailFocused = true;
	Target->SetFocus();
}

void ULNPInventoryTabWidget::FocusGrid()
{
	bDetailFocused = false;

	if (ItemGrid)
		ItemGrid->SetFocus();
}

bool ULNPInventoryTabWidget::HandleMenuBack()
{
	if (bDetailFocused)
	{
		FocusGrid();
		return true;   // 소비 — 메뉴는 닫히지 않는다.
	}

	return false;      // Grid 포커스 상태의 ○는 메뉴 닫기로 넘긴다.
}

UWidget* ULNPInventoryTabWidget::NativeGetDesiredFocusTarget() const
{
	if (bDetailFocused && DetailPanel)
	{
		if (UWidget* Target = DetailPanel->GetFirstFocusTarget())
			return Target;
	}

	return ItemGrid;
}
