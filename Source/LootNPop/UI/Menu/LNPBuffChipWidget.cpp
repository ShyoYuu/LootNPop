// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPBuffChipWidget.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Item/LNPItemDefinitionBase.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

void ULNPBuffChipWidget::SetBuffInstance(ULNPInventoryItemInstance* InInstance)
{
	BoundInstance = InInstance;

	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(CountdownTimerHandle);

	if (BoundInstance == nullptr)
	{
		if (IconImage)
			IconImage->SetBrushFromTexture(nullptr);
		if (TimeText)
			TimeText->SetText(FText::GetEmpty());
		return;
	}

	if (IconImage)
	{
		ULNPItemDefinitionBase* Definition = BoundInstance->GetDefinition();
		IconImage->SetBrushFromTexture(Definition ? Definition->Icon : nullptr);
	}

	UpdateTimeText();

	// 지속시간이 있는 버프만 1초마다 카운트다운을 다시 쓴다.
	if (BoundInstance->GetRemainingDurationLive() > 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				CountdownTimerHandle, this, &ULNPBuffChipWidget::UpdateTimeText, 1.0f, /*bLoop=*/true);
		}
	}
}

void ULNPBuffChipWidget::UpdateTimeText()
{
	if (TimeText == nullptr)
		return;

	if (BoundInstance == nullptr)
	{
		TimeText->SetText(FText::GetEmpty());
		return;
	}

	const float Remaining = BoundInstance->GetRemainingDurationLive();

	// RemainingDuration <= 0 은 무한 지속을 뜻한다 (TechDesign_Inventory §5) — 시간 표기를 생략한다.
	if (Remaining <= 0.f)
	{
		TimeText->SetText(FText::GetEmpty());
		if (UWorld* World = GetWorld())
			World->GetTimerManager().ClearTimer(CountdownTimerHandle);
		return;
	}

	TimeText->SetText(FText::AsNumber(FMath::CeilToInt(Remaining)));
}

void ULNPBuffChipWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(CountdownTimerHandle);

	Super::NativeDestruct();
}
