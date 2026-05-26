// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/LNPHpBarWidget.h"
#include "Components/ProgressBar.h"

void ULNPHpBarWidget::UpdateHpBar(float Current, float Max)
{
	if (HpBar)
		HpBar->SetPercent(Max > 0.f ? Current / Max : 0.f);
}
