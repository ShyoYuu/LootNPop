// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/LNPHudWidget.h"
#include "UI/LNPHudViewModel.h"
#include "View/MVVMView.h"

void ULNPHudWidget::InitViewModel(UAbilitySystemComponent* InASC)
{
	if (!HudViewModel)
		HudViewModel = NewObject<ULNPHudViewModel>(this);

	HudViewModel->Initialize(InASC);

	// Blueprint View Model 패널에 "HUD_ViewModel"로 등록된 슬롯에 인스턴스를 주입한다.
	if (UMVVMView* View = GetExtension<UMVVMView>())
		View->SetViewModel(FName("HUD_ViewModel"), HudViewModel);
}

void ULNPHudWidget::DeinitViewModel()
{
	if (HudViewModel)
		HudViewModel->Deinitialize();
}

void ULNPHudWidget::NativeDestruct()
{
	DeinitViewModel();
	Super::NativeDestruct();
}
