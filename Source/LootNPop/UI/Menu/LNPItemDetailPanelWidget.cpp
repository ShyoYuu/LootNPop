// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPItemDetailPanelWidget.h"
#include "Character/LNPPlayerCharacter.h"
#include "Item/LNPBuffData.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPWeaponData.h"
#include "LNPGameplayTags.h"

#include "UI/Menu/LNPMenuButtonWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void ULNPItemDetailPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (EquipButton)
		EquipButton->OnClicked().AddUObject(this, &ULNPItemDetailPanelWidget::HandleEquipClicked);
	if (DropButton)
		DropButton->OnClicked().AddUObject(this, &ULNPItemDetailPanelWidget::HandleDropClicked);
}

void ULNPItemDetailPanelWidget::SetItem(ULNPInventoryItemInstance* InInstance)
{
	BoundInstance = InInstance;

	ULNPItemDefinitionBase* Definition = BoundInstance ? BoundInstance->GetDefinition() : nullptr;

	if (IconImage)
		IconImage->SetBrushFromTexture(Definition ? Definition->Icon : nullptr);

	if (NameText)
	{
		if (Definition == nullptr)
		{
			NameText->SetText(EmptyMessage);
		}
		else
		{
			// DisplayName이 비어 있으면 에셋명으로 폴백한다 (기존 인벤토리 위젯과 동일 규칙).
			NameText->SetText(Definition->DisplayName.IsEmpty()
				? FText::FromString(Definition->GetName())
				: Definition->DisplayName);
		}
	}

	if (DetailText)
	{
		FText Detail = FText::GetEmpty();

		if (BoundInstance && Definition)
		{
			if (Cast<ULNPBuffData>(Definition) != nullptr)
			{
				const float Remaining = BoundInstance->GetRemainingDurationLive();
				if (Remaining > 0.f)
				{
					Detail = FText::Format(
						NSLOCTEXT("LNPMenu", "BuffRemaining", "Remaining {0}s"),
						FText::AsNumber(FMath::CeilToInt(Remaining)));
				}
			}
			else
			{
				const int32 Level = BoundInstance->GetStatTagStackCount(TAG_Item_Level);
				if (Level > 0)
				{
					Detail = FText::Format(
						NSLOCTEXT("LNPMenu", "ItemLevel", "Lv. {0}"), FText::AsNumber(Level));
				}
			}
		}

		DetailText->SetText(Detail);
	}

	UpdateButtons();
}

void ULNPItemDetailPanelWidget::UpdateButtons()
{
	ULNPItemDefinitionBase* Definition = BoundInstance ? BoundInstance->GetDefinition() : nullptr;
	const bool bIsWeapon  = Cast<ULNPWeaponData>(Definition) != nullptr;
	const bool bIsEquipped = BoundInstance && BoundInstance->IsEquipped();

	if (EquipButton)
	{
		// ⚠️ 라벨을 넣지 않으면 버튼이 배경만 있는 빈 상자로 보인다.
		EquipButton->SetButtonLabel(bIsEquipped ? EquippedLabel : EquipLabel);

		// 장착 대상이 아닌 아이템에는 Equip 버튼 자체를 감춘다.
		EquipButton->SetVisibility(bIsWeapon ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		EquipButton->SetIsEnabled(bIsWeapon && !bIsEquipped);
	}

	if (DropButton)
	{
		DropButton->SetButtonLabel(DropLabel);
		DropButton->SetVisibility(Definition ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		// 장착 중인 아이템은 드랍할 수 없다 (서버 DropItem도 같은 가드를 건다).
		DropButton->SetIsEnabled(Definition != nullptr && !bIsEquipped);
	}
}

UWidget* ULNPItemDetailPanelWidget::GetFirstFocusTarget() const
{
	if (EquipButton && EquipButton->GetVisibility() != ESlateVisibility::Collapsed && EquipButton->GetIsEnabled())
		return EquipButton;

	if (DropButton && DropButton->GetVisibility() != ESlateVisibility::Collapsed && DropButton->GetIsEnabled())
		return DropButton;

	return nullptr;
}

void ULNPItemDetailPanelWidget::HandleEquipClicked()
{
	if (BoundInstance == nullptr)
		return;

	if (ALNPPlayerCharacter* Character = GetOwningPlayerPawn<ALNPPlayerCharacter>())
		Character->EquipWeaponInstance(BoundInstance);
}

void ULNPItemDetailPanelWidget::HandleDropClicked()
{
	if (BoundInstance == nullptr)
		return;

	if (ALNPPlayerCharacter* Character = GetOwningPlayerPawn<ALNPPlayerCharacter>())
		Character->DropItem(BoundInstance->GetItemId());
}
