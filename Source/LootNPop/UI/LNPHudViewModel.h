// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "LNPHudViewModel.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;
struct FGameplayTag;

/**
 * 플레이어 HUD용 MVVM ViewModel.
 *
 * Blueprint 위젯에서 바인딩 방법:
 *  - HpBar  : ProgressBar.Percent ← HUD_ViewModel.HealthPercent
 *  - AimDot : Widget.Visibility   ← HUD_ViewModel.bIsFreeAiming 에 Function Binding(bool→ESlateVisibility) 적용
 *
 * Blueprint View Model 패널에 이름 "HUD_ViewModel", 생성 모드 Manual로 등록 후
 * C++ 코드(ULNPHudWidget::InitViewModel)가 인스턴스를 주입한다.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPHudViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(UAbilitySystemComponent* InASC);
	void Deinitialize();

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "LNP|HUD", meta = (AllowPrivateAccess = "true"))
	float HealthPercent = 1.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "LNP|HUD", meta = (AllowPrivateAccess = "true"))
	bool bIsFreeAiming = false;

	void SetHealthPercent(float InValue);
	void SetIsFreeAiming(bool InValue);

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle AimTagHandle;

	float CachedHealth    = 1.f;
	float CachedMaxHealth = 1.f;

	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnMaxHealthChanged(const FOnAttributeChangeData& Data);
	void OnAimModeTagChanged(const FGameplayTag Tag, int32 Count);
	void UpdateHealthPercent();
};
