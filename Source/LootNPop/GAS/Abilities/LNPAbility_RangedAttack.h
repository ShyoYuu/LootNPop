// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "LNPAbility_RangedAttack.generated.h"

/**
 * Ranged basic attack: spawns a Mass Entity with FLNPProjectileFragment
 * using the weapon DataAsset's projectile parameters, then immediately ends.
 * The Mass ProjectileMovementProcessor handles all subsequent movement.
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_RangedAttack : public ULNPAbility_BasicAttack
{
	GENERATED_BODY()
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

private:
	void SpawnProjectile() const;
};
