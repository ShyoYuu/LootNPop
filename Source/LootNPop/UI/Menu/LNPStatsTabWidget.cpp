// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPStatsTabWidget.h"
#include "UI/Menu/LNPBuffChipWidget.h"
#include "UI/Menu/LNPStatsViewModel.h"
#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPInventoryComponent.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPItemInstance.h"
#include "Item/LNPWeaponData.h"
#include "Player/LNPPlayerState.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "View/MVVMView.h"

void ULNPStatsTabWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	ALNPPlayerState* PS = GetOwningPlayerState<ALNPPlayerState>();
	if (PS == nullptr)
		return;

	if (StatsViewModel == nullptr)
		StatsViewModel = NewObject<ULNPStatsViewModel>(this);

	StatsViewModel->Initialize(PS->GetAbilitySystemComponent());

	// Blueprint View Model 패널에 등록된 슬롯에 인스턴스를 주입한다 (컨텍스트 이름 무관).
	if (UMVVMView* View = GetExtension<UMVVMView>())
		View->SetViewModelByClass(StatsViewModel);

	BoundInventory = PS->GetInventoryComponent();
	if (ULNPInventoryComponent* Inventory = BoundInventory.Get())
		Inventory->OnInventoryChanged.AddDynamic(this, &ULNPStatsTabWidget::RefreshEquipmentAndBuffs);

	RefreshEquipmentAndBuffs();
}

void ULNPStatsTabWidget::NativeOnDeactivated()
{
	if (ULNPInventoryComponent* Inventory = BoundInventory.Get())
		Inventory->OnInventoryChanged.RemoveDynamic(this, &ULNPStatsTabWidget::RefreshEquipmentAndBuffs);
	BoundInventory.Reset();

	if (StatsViewModel)
		StatsViewModel->Deinitialize();

	Super::NativeOnDeactivated();
}

void ULNPStatsTabWidget::RefreshEquipmentAndBuffs()
{
	UpdateWeaponIcon();
	RebuildBuffChips();
}

void ULNPStatsTabWidget::UpdateWeaponIcon()
{
	if (WeaponIcon == nullptr)
		return;

	UTexture2D* IconTexture = EmptySlotIcon;

	if (const ALNPPlayerState* PS = GetOwningPlayerState<ALNPPlayerState>())
	{
		if (const ULNPEquipmentComponent* Equipment = PS->GetEquipmentComponent())
		{
			const FLNPWeaponInstance& WeaponSlot = Equipment->GetWeaponSlot();
			if (WeaponSlot.IsValid() && WeaponSlot.Definition->Icon)
				IconTexture = WeaponSlot.Definition->Icon;
		}
	}

	WeaponIcon->SetBrushFromTexture(IconTexture);
}

void ULNPStatsTabWidget::RebuildBuffChips()
{
	if (BuffContainer == nullptr || BuffChipClass == nullptr)
		return;

	BuffContainer->ClearChildren();

	ULNPInventoryComponent* Inventory = BoundInventory.Get();
	if (Inventory == nullptr)
		return;

	for (ULNPInventoryItemInstance* Instance : Inventory->GetActiveBuffInstances())
	{
		if (Instance == nullptr)
			continue;

		if (ULNPBuffChipWidget* Chip = CreateWidget<ULNPBuffChipWidget>(this, BuffChipClass))
		{
			BuffContainer->AddChild(Chip);
			Chip->SetBuffInstance(Instance);
		}
	}
}
