// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LNPHudWidget.generated.h"

class ULNPHudViewModel;
class ULNPRadialCooldownWidget;
class UAbilitySystemComponent;
class ULNPCharacterMoverComponent;

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
	/**
	 * 폰의 ASC를 전달해 ViewModel을 생성·초기화하고 MVVM View에 주입한다.
	 * Mover는 대시 쿨다운 표시용이며 null이어도 무방하다.
	 */
	void InitViewModel(UAbilitySystemComponent* InASC, ULNPCharacterMoverComponent* InMover);

	/** ViewModel 구독을 해제한다. 빙의 해제 또는 위젯 소멸 시 호출. */
	void DeinitViewModel();

protected:
	virtual void NativeDestruct() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ULNPHudViewModel> HudViewModel;

	/**
	 * 대시 쿨다운 파이. WBP에 없으면 표시만 조용히 생략된다.
	 * ⚠️ WBP에서 Is Variable을 켜야 바인딩된다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<ULNPRadialCooldownWidget> DashCooldownWidget;

	/** 대시 쿨다운은 "값"이 아니라 "시작됐다"는 이벤트라 ViewModel을 거치지 않는다 (근거는 TechDesign_HUD.md). */
	void HandleDashExecuted();

	TWeakObjectPtr<ULNPCharacterMoverComponent> BoundMover;
	FDelegateHandle DashExecutedHandle;
};
