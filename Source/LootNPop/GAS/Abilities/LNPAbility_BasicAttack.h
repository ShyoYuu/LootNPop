// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "LNPAbility_BasicAttack.generated.h"

class ULNPWeaponData;

/**
 * Abstract base for all weapon basic attack abilities.
 * Provides GetEquippedWeaponDef() so subclasses can read weapon config at runtime.
 */
UCLASS(Abstract)
class LOOTNPOP_API ULNPAbility_BasicAttack : public ULNPGameplayAbility
{
	GENERATED_BODY()
public:
	ULNPAbility_BasicAttack();

protected:
	/** Returns the currently equipped weapon's DataAsset, or null if none. */
	const ULNPWeaponData* GetEquippedWeaponDef() const;

	/** Base damage formula: (AttackPower + WeaponDamage) * AttackMultiplier. Override per ability for variations. */
	virtual float ComputeDamage() const;
};
