// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LNPHudWidget.generated.h"

class ULNPHudViewModel;
class UAbilitySystemComponent;

/**
 * 플레이어 HUD 위젯 C++ 기반 클래스.
 *
 * Blueprint 서브클래스(WBP_LNPHud) 설정 절차:
 *  1. 에디터 View Model 패널 → "HUD_ViewModel" 추가, 클래스 ULNPHudViewModel, 생성 모드 Manual
 *  2. HpBar  ProgressBar.Percent ← HUD_ViewModel.HealthPercent 바인딩
 *  3. AimDot Widget.Visibility   ← HUD_ViewModel.bIsFreeAiming 바인딩
 */
UCLASS()
class LOOTNPOP_API ULNPHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 폰의 ASC를 전달해 ViewModel을 생성·초기화하고 MVVM View에 주입한다. */
	void InitViewModel(UAbilitySystemComponent* InASC);

	/** ViewModel 구독을 해제한다. 빙의 해제 또는 위젯 소멸 시 호출. */
	void DeinitViewModel();

protected:
	virtual void NativeDestruct() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ULNPHudViewModel> HudViewModel;
};
