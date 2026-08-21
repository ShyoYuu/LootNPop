// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPInventoryTabWidget.h"
#include "UI/Menu/LNPItemDetailPanelWidget.h"
#include "UI/Menu/LNPMenuItemCellWidget.h"
#include "Item/LNPInventoryComponent.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Player/LNPPlayerState.h"

#include "CommonTileView.h"
#include "ICommonInputModule.h"

#define LOCTEXT_NAMESPACE "LNPMenu"

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
	// 탭에 들어올 때는 항상 Grid 포커스에서 시작한다.
	bDetailFocused = false;

	if (const ALNPPlayerState* PS = GetOwningPlayerState<ALNPPlayerState>())
	{
		BoundInventory = PS->GetInventoryComponent();
		if (ULNPInventoryComponent* Inventory = BoundInventory.Get())
			Inventory->OnInventoryChanged.AddDynamic(this, &ULNPInventoryTabWidget::RefreshGrid);
	}

	// ⚠️ Grid 채우기는 반드시 Super::NativeOnActivated()보다 **먼저** 한다.
	// CommonUI는 활성화 과정에서 GetDesiredFocusTarget()(= ItemGrid)에 포커스를 주는데,
	// SCommonListView::OnFocusReceived는 **항목이 이미 있을 때만** 포커스를 셀로 넘겨준다
	// (CommonListView.h:27 — `GetItems().Num() > 0`, 선택이 없으면 0번을 스스로 고른다).
	// 순서가 뒤바뀌면 그 순간 목록이 비어 있어 포커스가 TileView 컨테이너에 머물고,
	// 포커스 링이 첫 셀이 아니라 **그리드 전체**를 감싼다.
	// 재오픈 때는 풀링된 위젯에 이전 목록이 남아 있어 증상이 안 보이므로 첫 오픈에서만 드러난다.
	RefreshGrid();

	Super::NativeOnActivated();
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

		OnMenuHintsChanged.Broadcast();
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

	UObject* NewSelection = nullptr;
	if (PreviousSelection && Items.Contains(PreviousSelection))
		NewSelection = PreviousSelection;
	else if (Items.Num() > 0)
		NewSelection = Items[0];

	if (NewSelection)
		ItemGrid->SetSelectedItem(NewSelection);

	// ⚠️ 선택 델리게이트에 기대지 말고 **항상** 패널을 다시 그린다.
	// 합성은 목록의 추가·제거가 아니라 선택된 인스턴스의 **내부 값**(레벨)을 바꾸므로,
	// 선택 대상이 그대로면 SetSelectedItem이 OnItemSelectionChanged를 다시 쏘지 않는다
	// → 패널이 옛 레벨·옛 스탯을 계속 보여준다. 셀 쪽 RegenerateAllEntries와 같은 함정의 패널 판이다.
	if (DetailPanel)
		DetailPanel->SetItem(Cast<ULNPInventoryItemInstance>(NewSelection));

	// ⚠️ SetListItems만으로는 **항목 내부 변화**가 셀에 반영되지 않는다.
	// 장착/해제는 목록의 추가·제거가 아니라 같은 인스턴스의 플래그 변경이라, 목록이
	// 포인터·순서까지 동일해 SListView가 기존 행을 그대로 재사용하고
	// NativeOnListItemObjectSet을 다시 호출하지 않는다 → 장착 배지("E")가 갱신되지 않는다.
	// 엔진도 이 경우 RegenerateAllEntries를 권한다 (ListViewBase.h의 RequestRefresh 주석).
	ItemGrid->RegenerateAllEntries();

	// Grid가 비었는지에 따라 힌트 구성이 달라지므로(GetMenuHints) 목록이 바뀌면 다시 만든다.
	// 포커스 링은 TAttribute라 알아서 재평가되지만, 힌트는 밀어 넣는 방식이라 여기서 알려야 한다.
	OnMenuHintsChanged.Broadcast();
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
	if (DetailPanel == nullptr || DetailPanel->GetFirstFocusTarget() == nullptr)
		return;   // 누를 수 있는 버튼이 없으면 Grid 포커스를 유지한다.

	// 포커스 링은 SLNPFocusRingScope가 챙긴다 (LNPMenuRootWidget.cpp 주석 참조) —
	// SetFocus()가 쓰는 EFocusCause::SetDirectly만으로는 Slate가 링을 그리지 않기 때문이다.
	bDetailFocused = true;
	DetailPanel->GetFirstFocusTarget()->SetFocus();
	OnMenuHintsChanged.Broadcast();
}

void ULNPInventoryTabWidget::FocusGrid()
{
	bDetailFocused = false;
	OnMenuHintsChanged.Broadcast();

	if (ItemGrid)
		ItemGrid->SetFocus();
}

void ULNPInventoryTabWidget::GetMenuHints(TArray<FLNPMenuHint>& OutHints) const
{
	// 빈 Grid에서는 아래 두 힌트 모두 가리킬 대상이 없다 — ✕를 눌러도, 방향키를 눌러도 아무 일이 없다.
	// 탭 이동과 닫기 힌트는 루트가 공통으로 얹으므로 여기서 빠져도 조작 안내가 끊기지 않는다.
	if (!HasGridItems())
		return;

	FLNPMenuHint ClickHint;
	ClickHint.ActionRows = { ICommonInputModule::GetSettings().GetDefaultClickAction() };
	ClickHint.Label = bDetailFocused ? LOCTEXT("HintConfirm", "Confirm") : LOCTEXT("HintDetails", "Details");
	OutHints.Add(MoveTemp(ClickHint));

	// 방향 이동은 CommonUI 액션이 아니라 Slate 네비게이션이라 액션 행이 없다 — 고정 글리프를 쓴다.
	FLNPMenuHint MoveHint;
	MoveHint.FixedKeyboardGlyph = FText::FromString(TEXT("WASD"));
	MoveHint.FixedGamepadGlyph = FText::FromString(TEXT("L3"));
	MoveHint.Label = LOCTEXT("HintMove", "Move");
	OutHints.Add(MoveTemp(MoveHint));
}

bool ULNPInventoryTabWidget::HasGridItems() const
{
	return ItemGrid != nullptr && ItemGrid->GetNumItems() > 0;
}

bool ULNPInventoryTabWidget::ShouldForceFocusRing() const
{
	// 빈 Grid에서는 SCommonListView가 포커스를 셀로 넘겨줄 수 없어(넘길 셀이 없다)
	// 포커스가 TileView 컨테이너에 머문다. 그 상태로 링을 강제하면 그리드 전체가 파랗게 둘러싸인다.
	return HasGridItems();
}

FText ULNPInventoryTabWidget::GetMenuBackHintLabel() const
{
	// 디테일 포커스에서 ○는 메뉴를 닫지 않고 Grid로 돌아간다 (기획 §6-2).
	return bDetailFocused ? LOCTEXT("HintBackToGrid", "Back") : Super::GetMenuBackHintLabel();
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

#undef LOCTEXT_NAMESPACE
