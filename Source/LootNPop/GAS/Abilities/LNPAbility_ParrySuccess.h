// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "LNPAbility_ParrySuccess.generated.h"

/**
 * 패링 성공 시 방어자에게 발동되는 Ability.
 * TAG_GameplayEvent_Parry_Success 이벤트로 자동 트리거된다.
 * ReactionMontage를 재생하고 즉시 종료한다.
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_ParrySuccess : public ULNPGameplayAbility
{
	GENERATED_BODY()

public:
	ULNPAbility_ParrySuccess();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** 패링 성공 시 재생할 몽타주. 에디터에서 설정. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Animation")
	TObjectPtr<UAnimMontage> ReactionMontage;
};
