// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "LNPEnemyActivity.generated.h"

/** Pod 귀속 PureEntity의 서버 시뮬레이션 대역. 표현 LOD와 별개다. */
UENUM()
enum class ELNPEnemyActivityBand : uint8
{
	Dormant,
	Wander,
	Active,
};

namespace LNPEnemyActivity
{
	inline constexpr double DistanceInterval = 0.25;
	inline constexpr double WakeHoldSeconds = 10.0;

	/** 휴면 진입은 컬 거리 +10%, 복귀는 +5%다. 활성 진입/이탈도 10% 간격을 둔다. */
	inline ELNPEnemyActivityBand SelectBand(const ELNPEnemyActivityBand Previous, const double DistanceSq,
		const float ActiveDistance, const float CullDistance, const bool bForceActive, const bool bCanSleep)
	{
		if (bForceActive || ActiveDistance <= 0.f || CullDistance < ActiveDistance)
			return ELNPEnemyActivityBand::Active;
		const double ActiveLimit = ActiveDistance * (Previous == ELNPEnemyActivityBand::Active ? 1.1 : 1.0);
		if (DistanceSq <= FMath::Square(ActiveLimit))
			return ELNPEnemyActivityBand::Active;
		const double DormantLimit = CullDistance * (Previous == ELNPEnemyActivityBand::Dormant ? 1.05 : 1.1);
		return bCanSleep && DistanceSq > FMath::Square(DormantLimit)
			? ELNPEnemyActivityBand::Dormant : ELNPEnemyActivityBand::Wander;
	}
}
