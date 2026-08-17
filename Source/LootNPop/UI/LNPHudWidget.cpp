// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/LNPHudWidget.h"
#include "UI/LNPHudViewModel.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "View/MVVMView.h"
#include "Widgets/LNPRadialCooldownWidget.h"

void ULNPHudWidget::InitViewModel(UAbilitySystemComponent* InASC, ULNPCharacterMoverComponent* InMover)
{
	if (!HudViewModel)
		HudViewModel = NewObject<ULNPHudViewModel>(this);

	HudViewModel->Initialize(InASC);

	// Blueprint View Model 패널에 "HUD_ViewModel"로 등록된 슬롯에 인스턴스를 주입한다.
	if (UMVVMView* View = GetExtension<UMVVMView>())
		View->SetViewModel(FName("HUD_ViewModel"), HudViewModel);

	// 폰이 바뀔 수 있으므로 기존 구독을 먼저 끊는다 (재빙의 시 중복 구독 방지).
	if (ULNPCharacterMoverComponent* PrevMover = BoundMover.Get())
		PrevMover->OnDashExecuted.Remove(DashExecutedHandle);

	BoundMover = InMover;
	DashExecutedHandle.Reset();

	if (InMover)
		DashExecutedHandle = InMover->OnDashExecuted.AddUObject(this, &ULNPHudWidget::HandleDashExecuted);

	if (DashCooldownWidget)
		DashCooldownWidget->ClearCooldown();
}

void ULNPHudWidget::DeinitViewModel()
{
	if (HudViewModel)
		HudViewModel->Deinitialize();

	if (ULNPCharacterMoverComponent* Mover = BoundMover.Get())
		Mover->OnDashExecuted.Remove(DashExecutedHandle);

	BoundMover.Reset();
	DashExecutedHandle.Reset();

	if (DashCooldownWidget)
		DashCooldownWidget->ClearCooldown();
}

void ULNPHudWidget::HandleDashExecuted()
{
	const ULNPCharacterMoverComponent* Mover = BoundMover.Get();
	if (!Mover || !DashCooldownWidget)
		return;

	DashCooldownWidget->StartCooldown(Mover->GetDashCooldown());
}

void ULNPHudWidget::NativeDestruct()
{
	DeinitViewModel();
	Super::NativeDestruct();
}
