// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "LNPAbility_RangedAttack.generated.h"

/**
 * 원거리 기본 공격: 무기 DataAsset의 Projectile 파라미터로 FLNPProjectileFragment를 가진 Mass Entity를 스폰하고
 * 즉시 종료한다. 이후 모든 이동은 Mass ProjectileMovementProcessor가 처리한다.
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
