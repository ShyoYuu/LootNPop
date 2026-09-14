// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Effects/LNPGameplayEffect_AmmoCost.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"

ULNPGameplayEffect_AmmoCost::ULNPGameplayEffect_AmmoCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// AddBase 외 연산 금지 규약(ULNPBaseAttributeSet)은 스탯 UI의 C = A × B 분해를 지키기 위한 것이다.
	// 탄약은 스탯이 아니라서 해당하지 않지만, 어차피 Instant라 기초값을 직접 깎는 AddBase면 충분하다.
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = ULNPBaseAttributeSet::GetMagazineAmmoAttribute();
	Modifier.ModifierOp = EGameplayModOp::AddBase;
	Modifier.ModifierMagnitude = FScalableFloat(-1.f);
	Modifiers.Add(Modifier);
}
