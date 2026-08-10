// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menu/LNPMenuTabContentWidget.h"
#include "LNPInventoryTabWidget.generated.h"

class UCommonTileView;
class ULNPInventoryComponent;
class ULNPInventoryItemInstance;
class ULNPItemDetailPanelWidget;
class ULNPMenuItemCellWidget;

/**
 * 인벤토리 탭 (기획 §6). 좌측 Grid(TileView) + 우측 디테일 패널.
 *
 * 포커스 규칙:
 *  - Grid에서 ✕(Click) → 디테일 패널로 포커스 이동
 *  - 디테일에서 ○(Back) → Grid로 복귀 (메뉴는 닫히지 않음)
 *  - Grid에서 ○(Back) → 소비하지 않음 → 메뉴 루트가 닫는다
 *
 * Grid에는 가방 인스턴스와 활성 버프를 모두 넣는다. 기존 인벤토리 패널과 달리
 * **장착 중인 무기도 숨기지 않고** 배지로 장착 여부를 표시한다 (기획 §6-1).
 *
 * BP 서브클래스(WBP_MenuTab_Inventory) 요구 사항 — Is Variable 켜기:
 *  - "ItemGrid" (UCommonTileView), EntryWidgetClass = ULNPMenuItemCellWidget 파생 WBP
 *  - "DetailPanel" (ULNPItemDetailPanelWidget 파생 WBP)
 */
UCLASS()
class LOOTNPOP_API ULNPInventoryTabWidget : public ULNPMenuTabContentWidget
{
	GENERATED_BODY()

public:
	virtual bool HandleMenuBack() override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTileView> ItemGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULNPItemDetailPanelWidget> DetailPanel;

private:
	/** OnInventoryChanged 핸들러 — Grid를 가방 + 활성 버프로 다시 채운다. */
	UFUNCTION()
	void RefreshGrid();

	/** TileView 선택 변경 → 디테일 패널 갱신. */
	void HandleGridSelectionChanged(UObject* SelectedItem);

	/** 셀이 ✕로 눌렸을 때 → 디테일 패널로 포커스 이동. */
	void HandleCellActivated(ULNPInventoryItemInstance* Instance);

	/** 새로 생성된 셀에 클릭 델리게이트를 연결한다. */
	void HandleEntryGenerated(UUserWidget& EntryWidget);

	void FocusDetailPanel();
	void FocusGrid();

	TWeakObjectPtr<ULNPInventoryComponent> BoundInventory;

	/** 포커스가 디테일 패널에 있는지 — Back 처리 분기의 기준. */
	bool bDetailFocused = false;
};
