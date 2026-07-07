// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPAbility_RangedAttack.h"
#include "LNPAbility_RangedSpreadAttack.generated.h"

/**
 * 산탄 원거리 공격: RangedAttack과 동일하나 Projectile을 육각형 링 방사형(중앙 1 + 링 2 = 19발)으로 동시 스폰한다.
 * 배치 로직은 GetFireDirections 구현(Cube 좌표계 육각 링 순회) 참조.
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_RangedSpreadAttack : public ULNPAbility_RangedAttack
{
	GENERATED_BODY()
protected:
	virtual TArray<FVector> GetFireDirections(const FVector& SpawnPos) const override;
};
