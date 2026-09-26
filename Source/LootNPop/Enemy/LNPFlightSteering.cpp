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

	namespace
	{
		/** 전방 F를 Right 쪽으로 Yaw, Up 쪽으로 Pitch만큼 기울인 방향. */
		FVector MakeCandidate(const FVector& F, const FVector& R, const FVector& U, const float YawDeg, const float PitchDeg)
		{
			const float Yaw = FMath::DegreesToRadians(YawDeg);
			const float Pitch = FMath::DegreesToRadians(PitchDeg);
			return (F * FMath::Cos(Yaw) * FMath::Cos(Pitch) + R * FMath::Sin(Yaw) * FMath::Cos(Pitch) + U * FMath::Sin(Pitch)).GetSafeNormal();
		}

		/** 진행 기준을 갱신하고 교착 시간을 잰다. */
		void TrackProgress(const FParams& Params, const FVector& Location, const FVector& Goal, const bool bWantsToMove,
			const float DeltaTime, FSteeringState& State)
		{
			const float Distance = FVector::Dist(Goal, Location);
			if (!bWantsToMove || FVector::DistSquared(Goal, State.ProgressGoal) > FMath::Square(Params.GoalMovedReset))
			{
				State.ProgressGoal = Goal;
				State.BestGoalDistance = Distance;
				State.NoProgressTime = 0.f;
				State.bVerticalReversed = false;
			}
			else if (Distance < State.BestGoalDistance - Params.ProgressEpsilon)
			{
				State.BestGoalDistance = Distance;
				State.NoProgressTime = 0.f;
			}
			else
			{
				State.NoProgressTime += DeltaTime;
			}
		}
	}

	EStepResult Steer(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& Up, const FVector& Goal, const float MaxSpeed, const FVector& ExtraVelocity,
		const float DeltaTime, FSteeringState& State, FVector& OutLocation)
	{
		const FVector Desired = ComputeArrivalVelocity(Location, Goal, MaxSpeed, DeltaTime);
		TrackProgress(Params, Location, Goal, !Desired.IsNearlyZero(), DeltaTime, State);

		if (Desired.IsNearlyZero())
		{
			State.AvoidDirection = FVector::ZeroVector;
			State.AvoidTimeRemaining = 0.f;
			return Step(Collision, Params, Location, ExtraVelocity, DeltaTime, OutLocation);
		}

		if (State.NoProgressTime >= Params.StuckGiveUpTime)
		{
			// 복구를 다 했는데도 가까워지지 않았다. 기억을 비우고 호출자에게 목표를 바꾸라고 알린다.
			State.Reset();
			Step(Collision, Params, Location, ExtraVelocity, DeltaTime, OutLocation);
			return EStepResult::Stuck;
		}

		if (State.NoProgressTime >= Params.StuckReverseTime && !State.bVerticalReversed)
		{
			// 한쪽(예: 섬 위로 넘기)으로 계속 막혔다. 반대쪽(섬 아래로 빠지기)을 선호하게 하고 유지 중인 회피를 버린다.
			State.VerticalPreference = -State.VerticalPreference;
			State.bVerticalReversed = true;
			State.AvoidTimeRemaining = 0.f;
		}

		const float Speed = Desired.Size();
		const FVector Forward = Desired / Speed;

		FVector TryDirection = Forward;
		if (State.AvoidTimeRemaining > 0.f && !State.AvoidDirection.IsNearlyZero())
		{
			TryDirection = State.AvoidDirection;
			State.AvoidTimeRemaining -= DeltaTime;
		}

		const EStepResult Result = Step(Collision, Params, Location, TryDirection * Speed + ExtraVelocity, DeltaTime, OutLocation);
		if (Result != EStepResult::Blocked)
			return Result;

		// 막혔다. 이번 프레임은 여유 앞까지만 왔고, 원래 원하는 방향 주위의 후보를 평가해 다음 프레임부터 쓸 회피 방향을 고른다.
		FVector UpOnPlane = FVector::VectorPlaneProject(Up, Forward).GetSafeNormal();
		if (UpOnPlane.IsNearlyZero())
		{
			// 목표가 바로 위·아래다. 아무 수직축이나 기준으로 삼는다.
			FVector Unused;
			Forward.FindBestAxisVectors(UpOnPlane, Unused);
		}
		const FVector RightOnPlane = FVector::CrossProduct(UpOnPlane, Forward);

		const bool bWide = State.NoProgressTime >= Params.StuckWidenTime;
		const float Cone = bWide ? Params.WideConeDeg : Params.ConeDeg;

		TArray<FVector, TInlineAllocator<11>> Candidates;
		for (const float Yaw : { -Cone, 0.f, Cone })
		{
			for (const float Pitch : { -Cone, 0.f, Cone })
			{
				if (Yaw != 0.f || Pitch != 0.f)
					Candidates.Add(MakeCandidate(Forward, RightOnPlane, UpOnPlane, Yaw, Pitch));
			}
		}
		if (!State.AvoidDirection.IsNearlyZero())
		{
			Candidates.Add(State.AvoidDirection);
		}
		if (bWide)
		{
			// 짧은 후퇴. 진행 점수가 음수라 다른 후보가 모두 막혔을 때만 뽑힌다.
			Candidates.Add(-Forward);
		}

		const float LookaheadDistance = Speed * FMath::Max(Params.LookaheadTime, DeltaTime);
		const float SweepRadius = Params.BodyRadius + Params.Clearance;
		const FLNPWorldQueryParams QueryParams(ELNPWorldQueryClass::FlightSteering);

		FVector Best = FVector::ZeroVector;
		float BestScore = -UE_BIG_NUMBER;
		for (const FVector& Candidate : Candidates)
		{
			FLNPWorldHit Hit;
			const bool bHit = Collision.SweepSphereWorld(OutLocation, OutLocation + Candidate * LookaheadDistance, SweepRadius, QueryParams, Hit);
			// 트인 후보는 막힌 후보보다 언제나 앞선다(2 > 1). 막힌 후보끼리는 더 멀리 트인 쪽이 낫다.
			const float Openness = !bHit ? 2.f : (Hit.bStartPenetrating ? 0.f : Hit.Time);
			const float Score = Openness
				+ FVector::DotProduct(Candidate, Forward)
				+ 0.3f * FVector::DotProduct(Candidate, State.AvoidDirection)
				+ 0.25f * State.VerticalPreference * FVector::DotProduct(Candidate, UpOnPlane);
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = Candidate;
			}
		}

		State.AvoidDirection = Best;
		State.AvoidTimeRemaining = Params.AvoidHoldTime;
		return EStepResult::Blocked;
	}
}
