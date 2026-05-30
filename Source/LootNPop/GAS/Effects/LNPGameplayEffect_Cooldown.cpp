// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Effects/LNPGameplayEffect_Cooldown.h"

#include "NativeGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

// 네이티브 Tag로 등록 — DefaultGameplayTags.ini 항목 불필요.
UE_DEFINE_GAMEPLAY_TAG_STATIC(LNPTAG_Ability_Cooldown_Attack, "LNP.Ability.Cooldown.Attack")

ULNPGameplayEffect_Cooldown::ULNPGameplayEffect_Cooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.f); // 플레이스홀더; 실제 Duration은 Ability가 spec별로 주입

	// GetCooldownTags()는 UGameplayEffect::GetGrantedTags()를 사용하며 이 Component에서 읽으므로
	// CheckCooldown()이 재활성화를 올바르게 차단한다.
	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComp"));

	FInheritedTagContainer TagContainer;
	TagContainer.AddTag(LNPTAG_Ability_Cooldown_Attack);
	TargetTagsComp->SetAndApplyTargetTagChanges(TagContainer);

	GEComponents.Add(TargetTagsComp);
}
