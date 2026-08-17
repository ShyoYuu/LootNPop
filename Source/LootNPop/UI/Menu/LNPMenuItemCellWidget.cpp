// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPMenuItemCellWidget.h"
#include "Item/LNPBuffData.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Item/LNPItemDefinitionBase.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

void ULNPMenuItemCellWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	BoundInstance = Cast<ULNPInventoryItemInstance>(ListItemObject);

	// 셀은 스크롤에 따라 재사용되므로 이전 항목의 타이머를 반드시 끊는다.
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(CountdownTimerHandle);

	if (IconImage)
	{
		ULNPItemDefinitionBase* Definition = BoundInstance ? BoundInstance->GetDefinition() : nullptr;
		IconImage->SetBrushFromTexture(Definition ? Definition->Icon : nullptr);
	}

	UpdateBadge();

	// 버프 셀만 잔여 시간을 매초 다시 쓴다.
	if (BoundInstance && Cast<ULNPBuffData>(BoundInstance->GetDefinition()) != nullptr)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				CountdownTimerHandle, this, &ULNPMenuItemCellWidget::UpdateBadge, 1.0f, /*bLoop=*/true);
		}
	}
}

void ULNPMenuItemCellWidget::UpdateBadge()
{
	if (BadgeText == nullptr)
		return;

	if (BoundInstance == nullptr)
	{
		BadgeText->SetText(FText::GetEmpty());
		return;
	}

	if (Cast<ULNPBuffData>(BoundInstance->GetDefinition()) != nullptr)
	{
		const float Remaining = BoundInstance->GetRemainingDurationLive();

		// RemainingDuration < 0 (=-1) 은 영구 버프 (TechDesign_Inventory §5).
		if (Remaining <= 0.f)
		{
			BadgeText->SetText(FText::GetEmpty());
			if (UWorld* World = GetWorld())
				World->GetTimerManager().ClearTimer(CountdownTimerHandle);
		}
		else
		{
			BadgeText->SetText(FText::AsNumber(FMath::CeilToInt(Remaining)));
		}
		return;
	}

	// 무기·기타 아이템은 장착 여부만 표시한다 (기획 §6-1).
	BadgeText->SetText(BoundInstance->IsEquipped() ? EquippedBadgeText : FText::GetEmpty());
}

void ULNPMenuItemCellWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	OnCellActivated.Broadcast(BoundInstance);
}

void ULNPMenuItemCellWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(CountdownTimerHandle);

	Super::NativeDestruct();
}
