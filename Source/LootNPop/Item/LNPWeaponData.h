// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Animation/AnimMontage.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "LNPWeaponData.generated.h"

class ULNPVFXData;

UCLASS(BlueprintType)
class LOOTNPOP_API ULNPWeaponData : public ULNPItemDefinitionBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	float FireCooldown = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile")
	ELNPProjectileType ProjectileType = ELNPProjectileType::Linear;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "1"))
	float ProjectileSpeed = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "0.1"))
	float ProjectileHitRadius = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "0"))
	float ProjectileDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "0.1"))
	float ProjectileLifetime = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile")
	FVector MuzzleOffset = FVector(100.f, 0.f, 0.f);

	/** VFX played at spawn, during flight (trail), and on impact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|VFX")
	TObjectPtr<ULNPVFXData> ProjectileVFXData;
};
