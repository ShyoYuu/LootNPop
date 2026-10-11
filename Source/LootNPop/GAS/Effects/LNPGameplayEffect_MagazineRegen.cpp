// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Effects/LNPGameplayEffect_MagazineRegen.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "LNPGameplayTags.h"

#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GE_Data_MagazineRegen, "LNP.GE.Data.MagazineRegen", "Magazine ammo restored per regen period")

ULNPGameplayEffect_MagazineRegen::ULNPGameplayEffect_MagazineRegen()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(RegenPeriod);
	bExecutePeriodicEffectOnApplication = false;

	// 탄약은 스탯이 아니므로 AddBase 규약(스탯 UI 분해)과 무관하다. 주기 실행은 Instant처럼 기초값을 바꾼다.
	FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
	Mod.Attribute  = ULNPBaseAttributeSet::GetMagazineAmmoAttribute();
	Mod.ModifierOp = EGameplayModOp::AddBase;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag   = TAG_GE_Data_MagazineRegen;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	// 분사 중에는 억제 — 억제된 주기 GE는 실행을 건너뛰고, 풀리면 다시 돈다.
	UTargetTagRequirementsGameplayEffectComponent* TagReqComp =
		CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TagReqComp"));
	TagReqComp->OngoingTagRequirements.IgnoreTags.AddTag(TAG_State_FlameFiring);
	GEComponents.Add(TagReqComp);
}
