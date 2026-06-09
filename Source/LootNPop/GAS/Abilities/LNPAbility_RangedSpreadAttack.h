// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPAbility_RangedAttack.h"
#include "LNPAbility_RangedSpreadAttack.generated.h"

/**
 * 산탄 원거리 공격: RangedAttack과 동일하나 Projectile을 5x5 방사형으로 동시 스폰한다.
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_RangedSpreadAttack : public ULNPAbility_RangedAttack
{
	GENERATED_BODY()
protected:
	virtual TArray<FVector> GetFireDirections(const FVector& SpawnPos) const override;
};
