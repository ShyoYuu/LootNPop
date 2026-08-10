// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPMenuTabButtonWidget.h"
#include "CommonTextBlock.h"

void ULNPMenuTabButtonWidget::SetTabLabel(const FText& InText)
{
	if (TabLabel)
		TabLabel->SetText(InText);
}

void ULNPMenuTabButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너 미리보기와 런타임 초기 상태를 현재 선택 상태에 맞춘다.
	UpdateUnderline(GetSelected());
}

void ULNPMenuTabButtonWidget::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	UpdateUnderline(true);
}

void ULNPMenuTabButtonWidget::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	UpdateUnderline(false);
}

void ULNPMenuTabButtonWidget::UpdateUnderline(bool bIsTabSelected)
{
	if (SelectionUnderline)
	{
		// Hidden(Collapsed 아님) — 레이아웃 공간을 유지해 선택 전환 시 탭이 들썩이지 않는다.
		SelectionUnderline->SetVisibility(bIsTabSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
