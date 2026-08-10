// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "LNPItemDetailPanelWidget.generated.h"

class ULNPInventoryItemInstance;
class ULNPMenuButtonWidget;
class UImage;
class UTextBlock;

/**
 * 인벤토리 탭 우측 디테일 패널 (기획 §6-2).
 * 선택된 아이템의 상세와 Equip/Drop 버튼을 담는다.
 *
 * 무기 슬롯이 1개라 다른 무기를 장착하면 자동 교체되므로 Unequip 버튼은 두지 않는다 —
 * 이미 장착 중인 아이템은 Equip 버튼을 비활성화한다.
 *
 * BP 서브클래스(WBP_ItemDetailPanel) 요구 사항 — Is Variable 켜기:
 *  - "IconImage"(UImage), "NameText"·"DetailText"(UTextBlock)
 *  - "EquipButton"·"DropButton" (UCommonButtonBase 파생)
 */
UCLASS()
class LOOTNPOP_API ULNPItemDetailPanelWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 아이템을 지정한다. nullptr이면 "아이템 없음" 상태가 된다. */
	void SetItem(ULNPInventoryItemInstance* InInstance);

	/** 포커스를 이 패널로 옮길 때 잡을 위젯 — 현재 활성인 첫 버튼. 없으면 nullptr. */
	UWidget* GetFirstFocusTarget() const;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** 종류·레벨·잔여 시간 등 부가 정보 한 줄. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<ULNPMenuButtonWidget> EquipButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<ULNPMenuButtonWidget> DropButton;

	/** 선택된 아이템이 없을 때 표시할 안내 문구. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	FText EmptyMessage = NSLOCTEXT("LNPMenu", "NoItemSelected", "No item");

	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	FText EquipLabel = NSLOCTEXT("LNPMenu", "Equip", "Equip");

	/** 이미 장착 중일 때의 Equip 버튼 문구 (버튼은 비활성). */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	FText EquippedLabel = NSLOCTEXT("LNPMenu", "Equipped", "Equipped");

	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	FText DropLabel = NSLOCTEXT("LNPMenu", "Drop", "Drop");

private:
	void HandleEquipClicked();
	void HandleDropClicked();

	/** 현재 아이템 종류·장착 상태에 맞춰 두 버튼의 표시/활성을 정한다. */
	void UpdateButtons();

	UPROPERTY(Transient)
	TObjectPtr<ULNPInventoryItemInstance> BoundInstance;
};
