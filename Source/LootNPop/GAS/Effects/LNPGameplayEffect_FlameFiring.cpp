// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Effects/LNPGameplayEffect_FlameFiring.h"
#include "LNPGameplayTags.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

ULNPGameplayEffect_FlameFiring::ULNPGameplayEffect_FlameFiring()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(0.3f); // 플레이스홀더; 실제 Duration은 어빌리티가 무기별로 주입

	// 틱마다 새 인스턴스가 쌓이지 않도록 하나로 묶고, 다시 적용될 때 지속시간만 갱신한다.
	// SetStackingType()은 에디터 전용·비노출이라 생성자에서는 (5.7 deprecated) 필드에 직접 쓴다.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComp"));

	FInheritedTagContainer TagContainer;
	TagContainer.AddTag(TAG_State_FlameFiring);
	TargetTagsComp->SetAndApplyTargetTagChanges(TagContainer);

	GEComponents.Add(TargetTagsComp);
}
