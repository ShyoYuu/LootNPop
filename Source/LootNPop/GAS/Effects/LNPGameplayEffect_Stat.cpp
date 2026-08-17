// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Effects/LNPGameplayEffect_Stat.h"
#include "GAS/LNPStatModifier.h"

namespace
{
	/** 스탯 메타 테이블의 모든 스탯에 대해 같은 연산의 SetByCaller 모디파이어를 만든다. */
	void BuildStatModifiers(TArray<FGameplayModifierInfo>& OutModifiers, EGameplayModOp::Type ModifierOp)
	{
		const TConstArrayView<FLNPStatMeta> Stats = LNPStat::GetStatMetaTable();
		OutModifiers.Reserve(Stats.Num());

		for (const FLNPStatMeta& Stat : Stats)
		{
			FGameplayModifierInfo& Mod = OutModifiers.AddDefaulted_GetRef();
			Mod.Attribute  = Stat.Attribute;
			Mod.ModifierOp = ModifierOp;

			FSetByCallerFloat SetByCaller;
			SetByCaller.DataTag   = Stat.SetByCallerTag;
			Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		}
	}
}

ULNPGameplayEffect_StatFlat::ULNPGameplayEffect_StatFlat()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	BuildStatModifiers(Modifiers, EGameplayModOp::AddBase);
}

ULNPGameplayEffect_StatPercent::ULNPGameplayEffect_StatPercent()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	BuildStatModifiers(Modifiers, EGameplayModOp::MultiplyAdditive);
}
