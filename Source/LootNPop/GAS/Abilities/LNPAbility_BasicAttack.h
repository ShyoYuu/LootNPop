// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "LNPAbility_BasicAttack.generated.h"

class ULNPWeaponData;

/**
 * 모든 무기 기본 공격 Ability의 추상 기반 클래스.
 * 서브클래스가 런타임에 무기 config를 읽을 수 있도록 GetEquippedWeaponDef()를 제공한다.
 */
UCLASS(Abstract)
class LOOTNPOP_API ULNPAbility_BasicAttack : public ULNPGameplayAbility
{
	GENERATED_BODY()
public:
	ULNPAbility_BasicAttack();

protected:
	/** 현재 장착된 무기의 DataAsset을 반환한다. 없으면 null. */
	const ULNPWeaponData* GetEquippedWeaponDef() const;

	/** 기본 피해 공식: (AttackPower + WeaponDamage) * AttackMultiplier. Ability별로 Override 가능. */
	virtual float ComputeDamage() const;
};
