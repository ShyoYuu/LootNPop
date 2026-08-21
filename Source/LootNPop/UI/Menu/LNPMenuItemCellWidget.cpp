// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPMenuItemCellWidget.h"
#include "Item/LNPBuffData.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPWeaponData.h"

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

	UpdateBadges();

	// 버프 셀만 잔여 시간을 매초 다시 쓴다.
	if (BoundInstance && Cast<ULNPBuffData>(BoundInstance->GetDefinition()) != nullptr)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				CountdownTimerHandle, this, &ULNPMenuItemCellWidget::UpdateDurationText, 1.0f, /*bLoop=*/true);
		}
	}
}

void ULNPMenuItemCellWidget::UpdateBadges()
{
	// 좌상단 — 장착 표시.
	if (EquipMarkText)
	{
		EquipMarkText->SetText((BoundInstance && BoundInstance->IsEquipped())
			? EquippedBadgeText
			: FText::GetEmpty());
	}

	// 우하단 — 레벨. 무기만 표시한다 (디테일 패널의 "Lv. 현재/최대" 표기와 노출 조건을 맞춘다).
	if (LevelText)
	{
		const bool bIsWeapon = BoundInstance && Cast<ULNPWeaponData>(BoundInstance->GetDefinition()) != nullptr;
		LevelText->SetText(bIsWeapon
			? FText::Format(LevelFormat, FText::AsNumber(BoundInstance->GetItemLevel()))
			: FText::GetEmpty());
	}

	// 우상단 — 잔여 시간.
	UpdateDurationText();
}

void ULNPMenuItemCellWidget::UpdateDurationText()
{
	if (DurationText == nullptr)
		return;

	// 버프가 아니면 표시할 시간이 없다.
	if (BoundInstance == nullptr || Cast<ULNPBuffData>(BoundInstance->GetDefinition()) == nullptr)
	{
		DurationText->SetText(FText::GetEmpty());
		return;
	}

	const float Remaining = BoundInstance->GetRemainingDurationLive();

	// RemainingDuration < 0 (=-1) 은 영구 버프 (TechDesign_Inventory §5).
	if (Remaining <= 0.f)
	{
		DurationText->SetText(FText::GetEmpty());
		if (UWorld* World = GetWorld())
			World->GetTimerManager().ClearTimer(CountdownTimerHandle);
		return;
	}

	DurationText->SetText(FText::AsNumber(FMath::CeilToInt(Remaining)));
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
