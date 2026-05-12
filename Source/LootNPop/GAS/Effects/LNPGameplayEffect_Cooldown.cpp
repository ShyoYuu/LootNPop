// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Effects/LNPGameplayEffect_Cooldown.h"
#include "NativeGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

// Registered as a native tag — no DefaultGameplayTags.ini entry needed.
UE_DEFINE_GAMEPLAY_TAG_STATIC(LNPTAG_Ability_Cooldown_Attack, "LNP.Ability.Cooldown.Attack")

ULNPGameplayEffect_Cooldown::ULNPGameplayEffect_Cooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.f); // placeholder; actual duration injected per-spec by the ability

	// UE5.3+: grant tags via UTargetTagsGameplayEffectComponent instead of the deprecated
	// InheritableOwnedTagsContainer. GetCooldownTags() uses UGameplayEffect::GetGrantedTags()
	// which reads from this component, so CheckCooldown() will block re-activation correctly.
	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComp"));

	FInheritedTagContainer TagContainer;
	TagContainer.AddTag(LNPTAG_Ability_Cooldown_Attack);
	TargetTagsComp->SetAndApplyTargetTagChanges(TagContainer);

	GEComponents.Add(TargetTagsComp);
}
