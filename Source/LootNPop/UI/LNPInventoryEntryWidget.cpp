// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/LNPInventoryEntryWidget.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPBuffData.h"
#include "Item/LNPWeaponData.h"
#include "LNPGameplayTags.h"
#include "Character/LNPPlayerCharacter.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void ULNPInventoryEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 클릭 → 소유 폰의 드랍/장착. 버프 엔트리 BP에는 버튼이 없어 Optional로 null 허용.
	if (DropButton != nullptr)
		DropButton->OnClicked.AddDynamic(this, &ULNPInventoryEntryWidget::OnDropClicked);
	if (EquipButton != nullptr)
		EquipButton->OnClicked.AddDynamic(this, &ULNPInventoryEntryWidget::OnEquipClicked);
}

void ULNPInventoryEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	BoundInstance = Cast<ULNPInventoryItemInstance>(ListItemObject);
	ULNPItemDefinitionBase* Def = BoundInstance ? BoundInstance->GetDefinition() : nullptr;

	if (IconImage != nullptr)
		IconImage->SetBrushFromTexture(Def != nullptr ? Def->Icon.Get() : nullptr, false);

	if (NameText != nullptr)
	{
		// DisplayName 미설정 시 에셋명으로 폴백 — 이름이 빈칸으로 보이지 않게 한다.
		FText Name = (Def != nullptr) ? Def->DisplayName : FText::GetEmpty();
		if (Name.IsEmpty() && Def != nullptr)
			Name = FText::FromName(Def->GetFName());
		NameText->SetText(Name);
	}

	// DetailText: 버프는 잔여 시간, 그 외는 레벨(있으면). 둘 다 없으면 비운다.
	if (DetailText != nullptr)
	{
		FText Detail = FText::GetEmpty();
		if (BoundInstance != nullptr)
		{
			if (Cast<ULNPBuffData>(Def) != nullptr)
			{
				Detail = FText::FromString(FString::Printf(TEXT("%.0fs"), BoundInstance->GetRemainingDuration()));
			}
			else
			{
				const int32 Level = BoundInstance->GetStatTagStackCount(TAG_Item_Level);
				if (Level > 0)
					Detail = FText::FromString(FString::Printf(TEXT("Lv.%d"), Level));
			}
		}
		DetailText->SetText(Detail);
	}
}

void ULNPInventoryEntryWidget::OnDropClicked()
{
	if (BoundInstance == nullptr)
		return;

	if (ALNPPlayerCharacter* Character = Cast<ALNPPlayerCharacter>(GetOwningPlayerPawn()))
		Character->DropItem(BoundInstance->GetItemId());
}

void ULNPInventoryEntryWidget::OnEquipClicked()
{
	if (BoundInstance == nullptr)
		return;

	// 장착은 무기에 한해 동작 (스킬 슬롯 장착 UI는 후속). 그 외 타입이면 무시.
	if (Cast<ULNPWeaponData>(BoundInstance->GetDefinition()) != nullptr)
	{
		if (ALNPPlayerCharacter* Character = Cast<ALNPPlayerCharacter>(GetOwningPlayerPawn()))
			Character->EquipWeaponInstance(BoundInstance);
	}
}
