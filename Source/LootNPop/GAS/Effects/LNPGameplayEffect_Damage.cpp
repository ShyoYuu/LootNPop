// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Effects/LNPGameplayEffect_Damage.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GE_Data_Damage, "GE.Data.Damage", "Per-hit damage magnitude — pass as a negative value when applying")

ULNPGameplayEffect_Damage::ULNPGameplayEffect_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
	Mod.Attribute  = ULNPBaseAttributeSet::GetHealthAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag       = TAG_GE_Data_Damage;
	Mod.ModifierMagnitude     = FGameplayEffectModifierMagnitude(SetByCaller);
}
