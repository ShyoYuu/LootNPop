// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Effects/LNPGameplayEffect_Burn.h"
#include "GAS/Effects/LNPGameplayEffect_Damage.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "LNPGameplayTags.h"

ULNPGameplayEffect_Burn::ULNPGameplayEffect_Burn()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(3.f); // 플레이스홀더; 적용 커맨드가 어빌리티의 BurnDuration으로 덮는다
	Period = FScalableFloat(0.5f);           // 플레이스홀더; 적용 커맨드가 어빌리티의 BurnTickInterval로 덮는다
	bExecutePeriodicEffectOnApplication = false; // 첫 틱은 한 주기 뒤 — 적 화상(TickAccum 0에서 시작)과 같다

	// 중첩당 피해. 주기 실행이 중첩 수를 곱한다.
	FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
	Mod.Attribute  = ULNPBaseAttributeSet::GetIncomingDamageAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag   = TAG_GE_Data_Damage;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	// SetStackingType()은 에디터 전용·비노출이라 생성자에서는 (5.7 deprecated) 필드에 직접 쓴다 (FlameFiring과 같은 이유).
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = MaxStacks;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::NeverReset;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;

	// 큐는 "걸림/풀림" 두 번뿐이다. 기본값(false)이면 중첩이 갱신될 때마다 비신뢰 멀티캐스트로 OnActive·WhileActive가
	// 다시 나가, Looping GCN이 Niagara 컴포넌트를 갱신 횟수만큼 덧쌓고 게스트는 그 RPC를 일부 놓쳐 화면마다 수명이 달라졌다.
	bSuppressStackingCues = true;
	GameplayCues.Add(FGameplayEffectCue(TAG_GameplayCue_Status_Burn, 0.f, 0.f));
}
