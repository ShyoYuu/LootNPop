// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPMenuButtonWidget.h"
#include "Components/TextBlock.h"

void ULNPMenuButtonWidget::SetButtonLabel(const FText& InText)
{
	if (ButtonLabel)
		ButtonLabel->SetText(InText);
}
