// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPAbility_RangedAttack.h"
#include "LNPAbility_RangedSpreadAttack.generated.h"

/**
 * 산탄 원거리 공격: RangedAttack과 동일하나 Projectile을 육각형 링 방사형으로 동시 스폰한다.
 * 배치 로직은 GetFireDirections 구현(Cube 좌표계 육각 링 순회) 참조.
 *
 * 확산 형태는 무기 DataAsset이 아니라 이 어빌리티가 소유한다 — 같은 무기가 산탄 폭이 다른
 * 강공격·특수공격을 가질 수 있어야 하고, 그 축은 어빌리티별로 갈리기 때문이다.
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_RangedSpreadAttack : public ULNPAbility_RangedAttack
{
	GENERATED_BODY()
protected:
	virtual TArray<FVector> GetFireDirections(const FVector& SpawnPos) const override;

	/**
	 * 육각형 링 수. 발사 수 = 1 + 3 * N * (N + 1) → 0=1발, 1=7발, 2=19발, 3=37발.
	 * 펠릿마다 Mass Entity와 트레일 VFX가 하나씩 생기므로 올릴 때 비용을 함께 본다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat|Spread", meta = (ClampMin = "0", ClampMax = "5"))
	int32 HexRingCount = 2;

	/**
	 * 인접한 육각 셀 사이의 각도 간격 (도).
	 * HexRingCount와 곱한 값이 확산의 최대 반각이 된다 (2링 × 7.5도 = 중심에서 최대 15도).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat|Spread", meta = (ClampMin = "0"))
	float HexStepDegrees = 7.5f;
};
