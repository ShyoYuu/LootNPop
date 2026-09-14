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

	// 탄창 — 예측 차감도 현재값 변경이라 이 델리게이트로 들어온다.
	CachedMagazineAmmo = FMath::RoundToInt(InASC->GetNumericAttribute(ULNPBaseAttributeSet::GetMagazineAmmoAttribute()));
	CachedMagazineSize = FMath::RoundToInt(InASC->GetNumericAttribute(ULNPBaseAttributeSet::GetMagazineSizeAttribute()));
	UpdateAmmo();

	MagazineAmmoChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(
		ULNPBaseAttributeSet::GetMagazineAmmoAttribute())
		.AddUObject(this, &ULNPHudViewModel::OnMagazineAmmoChanged);

	MagazineSizeChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(
		ULNPBaseAttributeSet::GetMagazineSizeAttribute())
		.AddUObject(this, &ULNPHudViewModel::OnMagazineSizeChanged);
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
		ASC->GetGameplayAttributeValueChangeDelegate(ULNPBaseAttributeSet::GetMagazineAmmoAttribute())
			.Remove(MagazineAmmoChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(ULNPBaseAttributeSet::GetMagazineSizeAttribute())
			.Remove(MagazineSizeChangedHandle);
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

void ULNPHudViewModel::OnMagazineAmmoChanged(const FOnAttributeChangeData& Data)
{
	const int32 NewAmmo = FMath::RoundToInt(Data.NewValue);
	if (NewAmmo == CachedMagazineAmmo)
		return;

	CachedMagazineAmmo = NewAmmo;
	UpdateAmmo();
}

void ULNPHudViewModel::OnMagazineSizeChanged(const FOnAttributeChangeData& Data)
{
	const int32 NewSize = FMath::RoundToInt(Data.NewValue);
	if (NewSize == CachedMagazineSize)
		return;

	CachedMagazineSize = NewSize;
	UpdateAmmo();
}

void ULNPHudViewModel::UpdateAmmo()
{
	UE_MVVM_SET_PROPERTY_VALUE(bHasMagazine, CachedMagazineSize > 0);

	// FText는 값 비교로 알림을 거를 수 없어 호출자가 정수 캐시로 거른 뒤 여기서 직접 통지한다.
	AmmoText = FText::Format(NSLOCTEXT("LNPHud", "AmmoCount", "{0} / {1}"),
		FText::AsNumber(CachedMagazineAmmo), FText::AsNumber(CachedMagazineSize));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AmmoText);
}

void ULNPHudViewModel::UpdateHealthPercent()
{
	const float Percent = (CachedMaxHealth > 0.f) ? (CachedHealth / CachedMaxHealth) : 0.f;
	SetHealthPercent(FMath::Clamp(Percent, 0.f, 1.f));
}
