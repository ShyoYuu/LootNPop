// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/LNPHudViewModel.h"
#include "AbilitySystemComponent.h"
#include "LNPGameplayTags.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"

void ULNPHudViewModel::Initialize(UAbilitySystemComponent* InASC)
{
	if (!InASC)
		return;

	Deinitialize();
	BoundASC = InASC;

	// 초기값 설정
	CachedHealth    = InASC->GetNumericAttribute(ULNPBaseAttributeSet::GetHealthAttribute());
	CachedMaxHealth = InASC->GetNumericAttribute(ULNPBaseAttributeSet::GetMaxHealthAttribute());
	UpdateHealthPercent();
	SetIsFreeAiming(InASC->HasMatchingGameplayTag(TAG_AimMode_FreeAim));

	// 어트리뷰트 변경 구독
	HealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(
		ULNPBaseAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ULNPHudViewModel::OnHealthChanged);

	MaxHealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(
		ULNPBaseAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &ULNPHudViewModel::OnMaxHealthChanged);

	// FreeAim 태그 변경 구독
	AimTagHandle = InASC->RegisterGameplayTagEvent(TAG_AimMode_FreeAim, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ULNPHudViewModel::OnAimModeTagChanged);
}

void ULNPHudViewModel::Deinitialize()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(ULNPBaseAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(ULNPBaseAttributeSet::GetMaxHealthAttribute())
			.Remove(MaxHealthChangedHandle);
		ASC->RegisterGameplayTagEvent(TAG_AimMode_FreeAim, EGameplayTagEventType::NewOrRemoved)
			.Remove(AimTagHandle);
	}
	BoundASC.Reset();
}

void ULNPHudViewModel::SetHealthPercent(float InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, InValue);
}

void ULNPHudViewModel::SetIsFreeAiming(bool InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsFreeAiming, InValue);
}

void ULNPHudViewModel::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedHealth = Data.NewValue;
	UpdateHealthPercent();
}

void ULNPHudViewModel::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxHealth = Data.NewValue;
	UpdateHealthPercent();
}

void ULNPHudViewModel::OnAimModeTagChanged(const FGameplayTag Tag, int32 Count)
{
	SetIsFreeAiming(Count > 0);
}

void ULNPHudViewModel::UpdateHealthPercent()
{
	const float Percent = (CachedMaxHealth > 0.f) ? (CachedHealth / CachedMaxHealth) : 0.f;
	SetHealthPercent(FMath::Clamp(Percent, 0.f, 1.f));
}
