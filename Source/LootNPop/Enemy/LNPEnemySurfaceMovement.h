// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "SurfaceNavigation/LNPSurfaceTypes.h"

class ULNPMassWorldCollisionSubsystem;
struct FLNPExactHitIdentity;
struct FLNPSurfaceDataSnapshot;

/** Phase 6의 Support snapshot 우선·exact 폴백 PureEntity 이동 조합. */
namespace LNPEnemySurfaceMovement
{
	/** `LNP.SurfaceNav.EnemySupportCache`. false면 Phase 3b exact-only 기준선이다. */
	LOOTNPOP_API bool IsSupportCacheEnabled();

	/** exact face identity가 현재 SurfaceData generation의 정적 Layer를 가리키면 handle로 바꾼다. */
	LOOTNPOP_API bool MakeSurfaceHandle(const FLNPExactHitIdentity& Identity, FLNPSurfaceHandle& OutHandle);

	/** scene query 없이 snapshot의 HighConfidence 지면을 캡슐 중심과 handle로 바꾼다. */
	LOOTNPOP_API bool TryResolveCachedGround(
		const FLNPSurfaceDataSnapshot& Snapshot,
		const LNPEnemyExactMovement::FParams& Params,
		const FVector& TargetLocation,
		FLNPSurfaceHandle& InOutSurface,
		FVector& OutLocation);

	/**
	 * grounded 한 프레임. 수평 blocker sweep 뒤 cache high-confidence를 우선하고,
	 * 그 밖의 상태는 같은 도달점에서 exact support probe로 폴백한다.
	 */
	LOOTNPOP_API LNPEnemyExactMovement::EGroundResult StepGrounded(
		const FLNPSurfaceDataSnapshot* SurfaceSnapshot,
		const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params,
		const FVector& Location,
		const FVector& Velocity,
		float DeltaTime,
		FVector& OutLocation,
		FVector& OutVelocity,
		FLNPSurfaceHandle& InOutSurface);

	/** 공중은 항상 exact다. 착지한 정적 Support의 handle을 기록하고 그 밖에는 무효화한다. */
	LOOTNPOP_API bool StepAirborne(
		const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params,
		const FVector& Location,
		FVector& InOutVelocity,
		float DeltaTime,
		FVector& OutLocation,
		FLNPSurfaceHandle& InOutSurface);
}
