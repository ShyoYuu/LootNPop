// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPMenuTabListWidget.h"
#include "UI/Menu/LNPMenuRootWidget.h"
#include "UI/Menu/LNPMenuTabButtonWidget.h"

#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"

ULNPMenuTabListWidget::ULNPMenuTabListWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// ⚠️ 베이스 기본값은 false다. 켜지 않으면 NativeConstruct가 SetListeningForInput을 호출하지 않아
	// Next/PreviousTabInputActionData가 아예 바인딩되지 않는다 — L1/R1도 Q/E도 무반응이 된다.
	bAutoListenForInput = true;

	TabDisplayNames.Add(ULNPMenuRootWidget::TabId_Stats(),     NSLOCTEXT("LNPMenu", "TabStats", "CHARACTER"));
	TabDisplayNames.Add(ULNPMenuRootWidget::TabId_Inventory(), NSLOCTEXT("LNPMenu", "TabInventory", "INVENTORY"));
	TabDisplayNames.Add(ULNPMenuRootWidget::TabId_Settings(),  NSLOCTEXT("LNPMenu", "TabSettings", "SETTINGS"));
}

void ULNPMenuTabListWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	OnTabSelected.AddDynamic(this, &ULNPMenuTabListWidget::HandleTabSelectedForEmphasis);
}

void ULNPMenuTabListWidget::HandleTabCreation_Implementation(FName TabNameID, UCommonButtonBase* TabButton)
{
	if (TabButton == nullptr)
		return;

	if (const FText* DisplayName = TabDisplayNames.Find(TabNameID))
	{
		if (ULNPMenuTabButtonWidget* LabeledButton = Cast<ULNPMenuTabButtonWidget>(TabButton))
			LabeledButton->SetTabLabel(*DisplayName);
	}

	// 베이스 구현이 비어 있으므로 여기서 붙이지 않으면 버튼이 화면에 나타나지 않는다.
	if (TabButtonContainer)
		TabButtonContainer->AddChild(TabButton);
}

void ULNPMenuTabListWidget::HandleTabRemoval_Implementation(FName /*TabNameID*/, UCommonButtonBase* TabButton)
{
	if (TabButtonContainer && TabButton)
		TabButtonContainer->RemoveChild(TabButton);
}

void ULNPMenuTabListWidget::HandleTabSelectedForEmphasis(FName TabId)
{
	const int32 TabCount = GetTabCount();
	for (int32 Index = 0; Index < TabCount; ++Index)
	{
		const FName Id = GetTabIdAtIndex(Index);
		if (UCommonButtonBase* Button = GetTabButtonBaseByID(Id))
		{
			const float Scale = (Id == TabId) ? SelectedTabScale : 1.0f;
			Button->SetRenderScale(FVector2D(Scale, Scale));
		}
	}
}
