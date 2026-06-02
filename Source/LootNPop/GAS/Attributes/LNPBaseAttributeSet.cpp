// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Attributes/LNPBaseAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "GAS/LNPDamageFormula.h"

ULNPBaseAttributeSet::ULNPBaseAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitAttackPower(10.0f);
	InitAttackSpeed(1.0f);
	InitDefensePower(0.0f);
	InitMoveSpeed(1.0f);
	InitAttackMultiplier(1.0f);
	InitIncomingDamage(0.f);
}

void ULNPBaseAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
		NewValue = FMath::Max(1.0f, NewValue);
	else if (Attribute == GetAttackSpeedAttribute() || Attribute == GetMoveSpeedAttribute() || Attribute == GetAttackMultiplierAttribute())
		NewValue = FMath::Max(0.01f, NewValue);
}

void ULNPBaseAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float RawDamage = GetIncomingDamage();
		SetIncomingDamage(0.f);
		const float FinalDamage = LNPDamage::ApplyDefense(RawDamage, GetDefensePower());
		SetHealth(FMath::Clamp(GetHealth() - FinalDamage, 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
}
