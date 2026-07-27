// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/LNPInventoryWidget.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "Item/LNPInventoryComponent.h"
#include "Item/LNPInventoryItemInstance.h"

#include "AbilitySystemComponent.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"

TArray<FGameplayAttribute> ULNPInventoryWidget::GetDisplayedAttributes()
{
	return {
		ULNPBaseAttributeSet::GetHealthAttribute(),
		ULNPBaseAttributeSet::GetMaxHealthAttribute(),
		ULNPBaseAttributeSet::GetAttackPowerAttribute(),
		ULNPBaseAttributeSet::GetAttackSpeedAttribute(),
		ULNPBaseAttributeSet::GetDefensePowerAttribute(),
		ULNPBaseAttributeSet::GetMoveSpeedAttribute(),
		ULNPBaseAttributeSet::GetLootSpeedAttribute(),
	};
}

void ULNPInventoryWidget::InitViewModel(ULNPInventoryComponent* InInventory, UAbilitySystemComponent* InASC)
{
	// 재초기화 안전망 — 기존 구독을 먼저 해제한다.
	DeinitViewModel();

	BoundInventory = InInventory;
	if (InInventory != nullptr)
		InInventory->OnInventoryChanged.AddDynamic(this, &ULNPInventoryWidget::RefreshLists);

	BoundASC = InASC;
	if (InASC != nullptr)
	{
		// 버프 적용·만료는 어트리뷰트 변경으로 드러나므로 스탯별 델리게이트만 구독하면 충분하다.
		for (const FGameplayAttribute& Attribute : GetDisplayedAttributes())
		{
			AttributeHandles.Add(
				InASC->GetGameplayAttributeValueChangeDelegate(Attribute)
					.AddUObject(this, &ULNPInventoryWidget::OnStatAttributeChanged));
		}
	}

	RefreshLists();
	UpdateStatsText();
}

void ULNPInventoryWidget::DeinitViewModel()
{
	if (ULNPInventoryComponent* Inventory = BoundInventory.Get())
		Inventory->OnInventoryChanged.RemoveDynamic(this, &ULNPInventoryWidget::RefreshLists);
	BoundInventory.Reset();

	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		// 구독 시점과 같은 목록·순서이므로 인덱스로 짝지어 해제한다.
		const TArray<FGameplayAttribute> Attributes = GetDisplayedAttributes();
		for (int32 i = 0; i < AttributeHandles.Num() && i < Attributes.Num(); ++i)
			ASC->GetGameplayAttributeValueChangeDelegate(Attributes[i]).Remove(AttributeHandles[i]);
	}
	AttributeHandles.Reset();
	BoundASC.Reset();
}

void ULNPInventoryWidget::OnStatAttributeChanged(const FOnAttributeChangeData& Data)
{
	UpdateStatsText();
}

void ULNPInventoryWidget::RefreshLists()
{
	ULNPInventoryComponent* Inventory = BoundInventory.Get();
	if (Inventory == nullptr)
		return;

	// 보관 — 아이템 인스턴스(UObject) 중 미장착만 노출 (장착본은 장비 슬롯이 표시).
	if (StorageList != nullptr)
	{
		StorageList->ClearListItems();
		for (ULNPInventoryItemInstance* Instance : Inventory->GetBagInstances())
		{
			if (Instance != nullptr && !Instance->IsEquipped())
				StorageList->AddItem(Instance);
		}
	}

	// 버프 — 활성 버프 인스턴스를 직접 노출 (별도 래퍼 불필요).
	if (BuffList != nullptr)
	{
		BuffList->ClearListItems();
		for (ULNPInventoryItemInstance* Instance : Inventory->GetActiveBuffInstances())
		{
			if (Instance != nullptr)
				BuffList->AddItem(Instance);
		}
	}
}

void ULNPInventoryWidget::UpdateStatsText()
{
	if (StatsText == nullptr)
		return;

	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (ASC == nullptr)
	{
		StatsText->SetText(FText::GetEmpty());
		return;
	}

	auto Attr = [ASC](const FGameplayAttribute& Attribute)
	{
		return ASC->GetNumericAttribute(Attribute);
	};

	const FString Readout = FString::Printf(
		TEXT("HP            %.0f / %.0f\n")
		TEXT("AttackPower   %.1f\n")
		TEXT("AttackSpeed   %.2f\n")
		TEXT("DefensePower  %.1f\n")
		TEXT("MoveSpeed     %.2f\n")
		TEXT("LootSpeed     %.2f"),
		Attr(ULNPBaseAttributeSet::GetHealthAttribute()),
		Attr(ULNPBaseAttributeSet::GetMaxHealthAttribute()),
		Attr(ULNPBaseAttributeSet::GetAttackPowerAttribute()),
		Attr(ULNPBaseAttributeSet::GetAttackSpeedAttribute()),
		Attr(ULNPBaseAttributeSet::GetDefensePowerAttribute()),
		Attr(ULNPBaseAttributeSet::GetMoveSpeedAttribute()),
		Attr(ULNPBaseAttributeSet::GetLootSpeedAttribute()));

	StatsText->SetText(FText::FromString(Readout));
}

void ULNPInventoryWidget::NativeDestruct()
{
	DeinitViewModel();
	Super::NativeDestruct();
}
