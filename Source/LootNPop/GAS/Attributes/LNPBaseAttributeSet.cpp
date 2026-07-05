// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Attributes/LNPBaseAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "GAS/LNPDamageFormula.h"
#include "Net/UnrealNetwork.h"

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

void ULNPBaseAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(ULNPBaseAttributeSet, Health,          COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULNPBaseAttributeSet, MaxHealth,       COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULNPBaseAttributeSet, AttackPower,     COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULNPBaseAttributeSet, AttackSpeed,     COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULNPBaseAttributeSet, DefensePower,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULNPBaseAttributeSet, MoveSpeed,       COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULNPBaseAttributeSet, AttackMultiplier,COND_None, REPNOTIFY_Always);
}

void ULNPBaseAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)          { GAMEPLAYATTRIBUTE_REPNOTIFY(ULNPBaseAttributeSet, Health, OldValue); }
void ULNPBaseAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)       { GAMEPLAYATTRIBUTE_REPNOTIFY(ULNPBaseAttributeSet, MaxHealth, OldValue); }
void ULNPBaseAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)     { GAMEPLAYATTRIBUTE_REPNOTIFY(ULNPBaseAttributeSet, AttackPower, OldValue); }
void ULNPBaseAttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldValue)     { GAMEPLAYATTRIBUTE_REPNOTIFY(ULNPBaseAttributeSet, AttackSpeed, OldValue); }
void ULNPBaseAttributeSet::OnRep_DefensePower(const FGameplayAttributeData& OldValue)    { GAMEPLAYATTRIBUTE_REPNOTIFY(ULNPBaseAttributeSet, DefensePower, OldValue); }
void ULNPBaseAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)       { GAMEPLAYATTRIBUTE_REPNOTIFY(ULNPBaseAttributeSet, MoveSpeed, OldValue); }
void ULNPBaseAttributeSet::OnRep_AttackMultiplier(const FGameplayAttributeData& OldValue){ GAMEPLAYATTRIBUTE_REPNOTIFY(ULNPBaseAttributeSet, AttackMultiplier, OldValue); }

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
