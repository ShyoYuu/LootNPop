// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GameplayEffectExtension.h"

ULNPBaseAttributeSet::ULNPBaseAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitAttackPower(10.0f);
	InitAttackSpeed(1.0f);
	InitDefensePower(0.0f);
	InitMoveSpeed(1.0f);
}

void ULNPBaseAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
		NewValue = FMath::Max(1.0f, NewValue);
	else if (Attribute == GetAttackSpeedAttribute() || Attribute == GetMoveSpeedAttribute())
		NewValue = FMath::Max(0.01f, NewValue);
}

void ULNPBaseAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
}
