// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPAbility_BasicAttack.h"
#include "LNPAbility_RangedAttack.generated.h"

struct FLNPFireAimTargetData;

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

	/** AimInput: 발동 이벤트에 실린 발사 순간 조준 스냅샷. 없으면(적 NPC) 캐릭터의 GetBaseAimRotation으로 쏜다. */
	virtual void SpawnProjectile(const FGameplayAbilityActivationInfo& ActivationInfo, const FLNPFireAimTargetData* AimInput) const;
	virtual TArray<FVector> GetFireDirections(const FVector& SpawnPos, const FLNPFireAimTargetData* AimInput) const;

	/** 폭발 스플래시 넉백 강도. 0이면 넉백 없음. 직격 넉백은 KnockbackStrength(기반 클래스)를 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat")
	float SplashKnockbackStrength = 200.f;
};
