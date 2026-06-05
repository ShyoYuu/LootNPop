// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "LNPAbility_Stagger.generated.h"

/**
 * 패링당한 공격자에게 발동되는 Stagger Ability.
 * TAG_GameplayEvent_Parry_Stagger 이벤트로 자동 트리거된다.
 * StaggerMontage를 재생하고 즉시 종료한다.
 * Player / Enemy Actor 양쪽에 Grant 가능하다.
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_Stagger : public ULNPGameplayAbility
{
	GENERATED_BODY()

public:
	ULNPAbility_Stagger();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Stagger 시 재생할 몽타주. 에디터에서 설정. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Animation")
	TObjectPtr<UAnimMontage> StaggerMontage;
};
