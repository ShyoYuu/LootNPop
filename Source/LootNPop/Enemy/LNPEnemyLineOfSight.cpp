// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyLineOfSight.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

namespace LNPEnemyLineOfSight
{
	bool HasClearShot(const ULNPMassWorldCollisionSubsystem& Collision, const FVector& Muzzle, const FVector& AimPoint)
	{
		FLNPWorldHit Hit;
		return !Collision.RaycastWorld(Muzzle, AimPoint, FLNPWorldQueryParams(ELNPWorldQueryClass::EnemyLineOfSight), Hit);
	}
}
