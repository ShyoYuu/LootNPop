// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPItemDetailPanelWidget.h"
#include "Character/LNPPlayerCharacter.h"
#include "Item/LNPBuffData.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPWeaponData.h"
#include "Item/LNPInventoryComponent.h"
#include "Player/LNPPlayerState.h"
#include "GAS/LNPStatModifier.h"

#include "UI/Menu/LNPMenuButtonWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void ULNPItemDetailPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (EquipButton)
		EquipButton->OnClicked().AddUObject(this, &ULNPItemDetailPanelWidget::HandleEquipClicked);
	if (MergeButton)
		MergeButton->OnClicked().AddUObject(this, &ULNPItemDetailPanelWidget::HandleMergeClicked);
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
		TArray<FString> DetailLines;

		if (BoundInstance && Definition)
		{
			// 스탯 변경은 선언형 목록에서 그대로 문구를 만든다. 무기는 목록의 원본이 레벨 테이블이므로
			// 현재 레벨의 행을 읽어야 강화된 실제 값이 보인다 (정의의 StatModifiers는 무시된다).
			const ULNPWeaponData* WeaponDef = Cast<ULNPWeaponData>(Definition);
			const TConstArrayView<FLNPStatModifier> Modifiers = WeaponDef
				? WeaponDef->GetStatModifiersForLevel(BoundInstance->GetItemLevel())
				: TConstArrayView<FLNPStatModifier>(Definition->StatModifiers);

			for (const FLNPStatModifier& Modifier : Modifiers)
			{
				const FText ModifierText = LNPStat::MakeModifierText(Modifier);
				if (!ModifierText.IsEmpty())
					DetailLines.Add(ModifierText.ToString());
			}

			if (Cast<ULNPBuffData>(Definition) != nullptr)
			{
				// 영구 버프(-1)는 남은 시간을 표시하지 않는다.
				const float Remaining = BoundInstance->GetRemainingDurationLive();
				if (Remaining > 0.f)
				{
					DetailLines.Add(FText::Format(
						NSLOCTEXT("LNPMenu", "BuffRemaining", "Remaining {0}s"),
						FText::AsNumber(FMath::CeilToInt(Remaining))).ToString());
				}
			}
			else if (WeaponDef != nullptr)
			{
				// 무기는 상한이 곧 레벨 테이블의 마지막 행이라 "현재/최대"로 함께 보여준다.
				DetailLines.Add(FText::Format(
					NSLOCTEXT("LNPMenu", "ItemLevelOfMax", "Lv. {0}/{1}"),
					FText::AsNumber(BoundInstance->GetItemLevel()),
					FText::AsNumber(WeaponDef->GetMaxLevel())).ToString());
			}
		}

		DetailText->SetText(FText::FromString(FString::Join(DetailLines, TEXT("\n"))));
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

	if (MergeButton)
	{
		// 재료 현황은 소유 클라이언트가 로컬로 센다 — 가방이 소유자에게 복제되므로 가능하다.
		// 서버는 TryMergeItem에서 같은 판정을 처음부터 다시 한다.
		int32 Have = 0;
		int32 Need = 0;
		bool bMergeable = false;
		bool bInventoryFound = false;
		if (bIsWeapon)
		{
			if (const ALNPPlayerState* PS = GetOwningPlayerState<ALNPPlayerState>())
			{
				if (const ULNPInventoryComponent* Inventory = PS->GetInventoryComponent())
				{
					bInventoryFound = true;
					bMergeable = Inventory->CanMergeItem(BoundInstance, Have, Need);
				}
			}
		}

		// 인벤토리를 찾은 무기인데 합성 불가 = 최대 레벨 도달. (인벤토리를 못 찾았으면 아래에서 감춘다 —
		// 그 경우까지 "Max Lv."로 쓰면 없는 사실을 말하게 된다.)
		MergeButton->SetButtonLabel(bMergeable
			? FText::Format(MergeLabel, FText::AsNumber(Have), FText::AsNumber(Need))
			: MaxLevelLabel);

		MergeButton->SetVisibility((bIsWeapon && bInventoryFound)
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
		MergeButton->SetIsEnabled(bMergeable && Have >= Need);
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

	if (MergeButton && MergeButton->GetVisibility() != ESlateVisibility::Collapsed && MergeButton->GetIsEnabled())
		return MergeButton;

	if (DropButton && DropButton->GetVisibility() != ESlateVisibility::Collapsed && DropButton->GetIsEnabled())
		return DropButton;

	return nullptr;
}

void ULNPItemDetailPanelWidget::HandleEquipClicked()
{
	if (BoundInstance == nullptr)
		return;

	if (ALNPPlayerCharacter* Character = GetOwningPlayerPawn<ALNPPlayerCharacter>())
		Character->RequestEquipWeaponInstance(BoundInstance);
}

void ULNPItemDetailPanelWidget::HandleMergeClicked()
{
	if (BoundInstance == nullptr)
		return;

	if (ALNPPlayerCharacter* Character = GetOwningPlayerPawn<ALNPPlayerCharacter>())
		Character->RequestMergeItem(BoundInstance->GetItemId());
}

void ULNPItemDetailPanelWidget::HandleDropClicked()
{
	if (BoundInstance == nullptr)
		return;

	if (ALNPPlayerCharacter* Character = GetOwningPlayerPawn<ALNPPlayerCharacter>())
		Character->DropItem(BoundInstance->GetItemId());
}
