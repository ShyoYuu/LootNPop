// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/LNPStatModifier.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GAS/Effects/LNPGameplayEffect_Stat.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "AbilitySystemComponent.h"

#define LOCTEXT_NAMESPACE "LNPStats"

namespace
{
	/** Op별 항등원 — 이 값을 넣은 모디파이어는 아무 효과가 없다 (AddBase 0, MultiplyAdditive 1). */
	float GetNoOpMagnitude(ELNPStatModOp Op)
	{
		return (Op == ELNPStatModOp::Flat) ? 0.f : 1.f;
	}
}

TConstArrayView<FLNPStatMeta> LNPStat::GetStatMetaTable()
{
	// 최초 호출 시 1회 구성 — FGameplayAttribute·FGameplayTag·FText 모두 정적 초기화 시점에는 만들 수 없다.
	static const TArray<FLNPStatMeta> Table = {
		{ ULNPBaseAttributeSet::GetMaxHealthAttribute(),    TAG_GE_Data_Stat_MaxHealth,    LOCTEXT("StatMaxHealth",    "Max HP"),       ELNPStatDisplay::Integer },
		{ ULNPBaseAttributeSet::GetAttackPowerAttribute(),  TAG_GE_Data_Stat_AttackPower,  LOCTEXT("StatAttackPower",  "Attack"),       ELNPStatDisplay::Scalar  },
		{ ULNPBaseAttributeSet::GetAttackSpeedAttribute(),  TAG_GE_Data_Stat_AttackSpeed,  LOCTEXT("StatAttackSpeed",  "Attack Speed"), ELNPStatDisplay::Ratio   },
		{ ULNPBaseAttributeSet::GetDefensePowerAttribute(), TAG_GE_Data_Stat_DefensePower, LOCTEXT("StatDefensePower", "Defense"),      ELNPStatDisplay::Scalar  },
		{ ULNPBaseAttributeSet::GetMoveSpeedAttribute(),    TAG_GE_Data_Stat_MoveSpeed,    LOCTEXT("StatMoveSpeed",    "Move Speed"),   ELNPStatDisplay::Ratio   },
		{ ULNPBaseAttributeSet::GetLootSpeedAttribute(),    TAG_GE_Data_Stat_LootSpeed,    LOCTEXT("StatLootSpeed",    "Loot Speed"),   ELNPStatDisplay::Ratio   },
		{ ULNPBaseAttributeSet::GetPoiseResistanceAttribute(), TAG_GE_Data_Stat_PoiseResistance, LOCTEXT("StatPoiseResistance", "Poise Resist"), ELNPStatDisplay::Scalar  },
	};

	return Table;
}

const FLNPStatMeta* LNPStat::FindStatMeta(const FGameplayAttribute& Attribute)
{
	for (const FLNPStatMeta& Stat : GetStatMetaTable())
	{
		if (Stat.Attribute == Attribute)
			return &Stat;
	}
	return nullptr;
}

FString LNPStat::FormatStatValue(float Value, ELNPStatDisplay Display)
{
	if (Display == ELNPStatDisplay::Ratio)
	{
		FNumberFormattingOptions Format;
		Format.MaximumFractionalDigits = 0;
		return FText::AsPercent(Value, &Format).ToString();
	}

	FNumberFormattingOptions Format;
	Format.MinimumFractionalDigits = (Display == ELNPStatDisplay::Integer) ? 0 : 1;
	Format.MaximumFractionalDigits = (Display == ELNPStatDisplay::Integer) ? 0 : 1;
	return FText::AsNumber(Value, &Format).ToString();
}

FText LNPStat::MakeModifierText(const FLNPStatModifier& Modifier)
{
	const FLNPStatMeta* Stat = FindStatMeta(Modifier.Attribute);
	if (Stat == nullptr)
		return FText::GetEmpty();

	// 곱연산은 항상 퍼센트. 합연산은 대상 스탯이 배율(기초 1.0)이면 퍼센트, 아니면 절대 수치.
	const bool bUsePercent = (Modifier.Op == ELNPStatModOp::Percent) || (Stat->Display == ELNPStatDisplay::Ratio);

	FNumberFormattingOptions Format;
	Format.MaximumFractionalDigits = bUsePercent ? 0 : 2;

	const FText ValueText = bUsePercent
		? FText::AsPercent(Modifier.Magnitude, &Format)
		: FText::AsNumber(Modifier.Magnitude, &Format);

	// 음수는 숫자 자체가 부호를 갖는다 — 양수일 때만 '+'를 붙인다.
	const FText SignedValue = (Modifier.Magnitude >= 0.f)
		? FText::Format(LOCTEXT("StatModPositive", "+{0}"), ValueText)
		: ValueText;

	// 합연산은 "기초"를 붙여 곱연산 버프와 구분한다 (기획 표기 규칙).
	return (Modifier.Op == ELNPStatModOp::Flat)
		? FText::Format(LOCTEXT("StatModFlat",    "Base {0} {1}"), Stat->DisplayName, SignedValue)
		: FText::Format(LOCTEXT("StatModPercent", "{0} {1}"),      Stat->DisplayName, SignedValue);
}

void LNPStat::ApplyModifiers(UAbilitySystemComponent& ASC,
	TConstArrayView<FLNPStatModifier> Modifiers,
	TArray<FActiveGameplayEffectHandle>& OutHandles)
{
	if (Modifiers.IsEmpty())
		return;

	FGameplayEffectContextHandle EffectContext = ASC.MakeEffectContext();

	// Op 하나당 GE 하나 — 같은 스탯에 여러 항목이 걸리면 크기를 합산한다.
	auto ApplyForOp = [&](ELNPStatModOp Op, TSubclassOf<UGameplayEffect> EffectClass)
	{
		const float NoOp = GetNoOpMagnitude(Op);

		// 모든 스탯 태그를 항등원으로 초기화한 뒤 해당 항목만 누적한다 (SetByCaller 누락 방지).
		TMap<FGameplayTag, float> Magnitudes;
		for (const FLNPStatMeta& Stat : GetStatMetaTable())
			Magnitudes.Add(Stat.SetByCallerTag, NoOp);

		bool bAnyModifier = false;
		for (const FLNPStatModifier& Modifier : Modifiers)
		{
			if (Modifier.Op != Op)
				continue;

			const FLNPStatMeta* Stat = FindStatMeta(Modifier.Attribute);
			if (Stat == nullptr)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[Stats] Unknown attribute '%s' in stat modifier — skipped."),
					*Modifier.Attribute.GetName());
				continue;
			}

			Magnitudes[Stat->SetByCallerTag] += Modifier.Magnitude;
			bAnyModifier = true;
		}

		if (!bAnyModifier)
			return;

		FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
		if (!Spec.IsValid())
			return;

		for (const TPair<FGameplayTag, float>& Pair : Magnitudes)
			Spec.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);

		OutHandles.Add(ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
	};

	ApplyForOp(ELNPStatModOp::Flat,    ULNPGameplayEffect_StatFlat::StaticClass());
	ApplyForOp(ELNPStatModOp::Percent, ULNPGameplayEffect_StatPercent::StaticClass());
}

float LNPStat::ResolveStatValue(const FGameplayAttribute& Attribute,
	float BaseValue,
	TConstArrayView<FLNPStatModifier> Modifiers)
{
	float FlatSum = 0.f;
	float PercentSum = 0.f;

	for (const FLNPStatModifier& Modifier : Modifiers)
	{
		if (Modifier.Attribute != Attribute)
			continue;

		((Modifier.Op == ELNPStatModOp::Flat) ? FlatSum : PercentSum) += Modifier.Magnitude;
	}

	// ApplyModifiers가 만드는 두 GE(AddBase / MultiplyAdditive)를 어그리게이터가 평가한 결과와 같은 식.
	return (BaseValue + FlatSum) * (1.f + PercentSum);
}

#undef LOCTEXT_NAMESPACE
