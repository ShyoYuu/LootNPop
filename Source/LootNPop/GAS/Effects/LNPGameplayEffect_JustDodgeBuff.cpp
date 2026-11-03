// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Effects/LNPGameplayEffect_JustDodgeBuff.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "LNPGameplayTags.h"

ULNPGameplayEffect_JustDodgeBuff::ULNPGameplayEffect_JustDodgeBuff()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(2.5f); // 플레이스홀더; 적용 커맨드가 ULNPSettings::JustDodgeBuffDuration으로 덮는다

	// 곱연산 채널 — 스탯 GE(ULNPGameplayEffect_StatPercent)와 같은 연산·같은 SetByCaller 태그라 다른 곱연산 버프와 합산된다.
	auto AddPercentModifier = [this](const FGameplayAttribute& Attribute, const FGameplayTag& DataTag)
	{
		FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
		Mod.Attribute  = Attribute;
		Mod.ModifierOp = EGameplayModOp::MultiplyAdditive;
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag   = DataTag;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	};
	AddPercentModifier(ULNPBaseAttributeSet::GetAttackSpeedAttribute(), TAG_GE_Data_Stat_AttackSpeed);
	AddPercentModifier(ULNPBaseAttributeSet::GetMoveSpeedAttribute(),   TAG_GE_Data_Stat_MoveSpeed);

	// SetStackingType()은 에디터 전용·비노출이라 생성자에서는 (5.7 deprecated) 필드에 직접 쓴다 (Burn과 같은 이유).
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;

	// 갱신마다 큐가 다시 나가지 않게 한다 — Burn에서 Looping GCN이 갱신 횟수만큼 덧쌓였던 문제(ULNPGameplayEffect_Burn 생성자 주석).
	bSuppressStackingCues = true;
	GameplayCues.Add(FGameplayEffectCue(TAG_GameplayCue_JustDodge_Buff, 0.f, 0.f));
}
