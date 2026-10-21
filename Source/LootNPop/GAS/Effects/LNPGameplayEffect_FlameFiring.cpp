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

	// 분사 VFX. 홀드 내내 WhileActive 한 번, 마지막 틱 뒤 만료 때 Removed 한 번이다.
	// 서버 전용 GE라 게스트 소유자에게는 RTT만큼 늦게 도착한다 — 그쪽은 어빌리티의 로컬 펄스가 대신한다
	// (ALNPGameplayCueNotify_FlameStream).
	// 스택 갱신 큐는 끈다 — 기본값(false)이면 틱(0.1초)마다 갱신될 때 OnActive·WhileActive가 비신뢰 멀티캐스트로
	// 전원에게 다시 나간다. 이미 켜진 큐를 "켜라"고 반복할 뿐이라 분사 중인 플레이어 1명당 초당 10회의 낭비였다.
	bSuppressStackingCues = true;
	GameplayCues.Add(FGameplayEffectCue(TAG_GameplayCue_Weapon_Flame, 0.f, 0.f));
}
