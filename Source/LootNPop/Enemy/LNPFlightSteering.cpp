// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPFlightSteering.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

namespace LNPFlightSteering
{
	namespace
	{
		/** 시작 겹침을 풀 때 겹침 깊이에 더하는 여유(cm). LNPEnemyExactMovement와 같은 값이다. */
		constexpr float DepenetrationSkin = 0.5f;
	}

	FVector ComputeArrivalVelocity(const FVector& Location, const FVector& Goal, const float MaxSpeed, const float DeltaTime)
	{
		const FVector ToGoal = Goal - Location;
		const float Distance = ToGoal.Size();
		if (Distance <= KINDA_SMALL_NUMBER || MaxSpeed <= 0.f || DeltaTime <= 0.f)
			return FVector::ZeroVector;

		const float Speed = FMath::Min(MaxSpeed, Distance / DeltaTime);
		return ToGoal / Distance * Speed;
	}

	EStepResult Step(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& DesiredVelocity, const float DeltaTime, FVector& OutLocation)
	{
		OutLocation = Location;

		const float Speed = DesiredVelocity.Size();
		if (Speed <= KINDA_SMALL_NUMBER || DeltaTime <= 0.f)
			return EStepResult::Hover;

		const FVector Direction = DesiredVelocity / Speed;
		const float StepDistance = Speed * DeltaTime;
		const float LookaheadDistance = Speed * FMath::Max(Params.LookaheadTime, DeltaTime);
		const float SweepRadius = Params.BodyRadius + Params.Clearance;

		FLNPWorldHit Hit;
		if (!Collision.SweepSphereWorld(Location, Location + Direction * LookaheadDistance, SweepRadius,
			FLNPWorldQueryParams(ELNPWorldQueryClass::FlightSteering), Hit))
		{
			OutLocation = Location + Direction * StepDistance;
			return EStepResult::Clear;
		}

		if (Hit.bStartPenetrating)
		{
			// 여유 구가 이미 지형에 들어와 있다. 여유만큼 되돌아가고 이번 프레임에는 전진하지 않는다 —
			// 겹친 채 전진 sweep을 믿으면 그 방향이 지형 안쪽인지 알 수 없다.
			OutLocation = Location + Hit.ImpactNormal * (Hit.PenetrationDepth + DepenetrationSkin);
			return EStepResult::Blocked;
		}

		// 여유 구가 닿는 거리까지는 몸과 지형 사이에 Clearance가 남는다. 딱 닿는 자리에 서면 다음 프레임 sweep이
		// 시작 겹침으로 잡혀 풀기와 전진을 번갈아 떨므로 조금 못 미쳐 선다.
		OutLocation = Location + Direction * FMath::Clamp(Hit.Distance - DepenetrationSkin, 0.f, StepDistance);
		return EStepResult::Blocked;
	}
}
