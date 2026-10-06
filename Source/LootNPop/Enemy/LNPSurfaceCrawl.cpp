// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPSurfaceCrawl.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

namespace LNPSurfaceCrawl
{
	namespace
	{
		/** 시작 겹침을 풀 때 겹침 깊이에 더하는 여유(cm). */
		constexpr float DepenetrationSkin = 0.5f;

		bool Sweep(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
			const FVector& Start, const FVector& End, FLNPWorldHit& OutHit)
		{
			return Collision.SweepSphereWorld(Start, End, Params.BodyRadius,
				FLNPWorldQueryParams(ELNPWorldQueryClass::SurfaceCrawl), OutHit);
		}

		/**
		 * 구의 접촉 법선(접점 → 구 중심). 면 법선(ImpactNormal)은 모서리에 닿았을 때 두 면 중 하나로 튀지만,
		 * 접촉 법선은 모서리를 축으로 연속으로 돈다.
		 */
		FVector ContactNormal(const FLNPWorldHit& Hit)
		{
			const FVector Normal = (Hit.Location - Hit.ImpactPoint).GetSafeNormal();
			return Normal.IsNearlyZero() ? Hit.ImpactNormal : Normal;
		}

		/** hit 자리에 틈을 남기고 붙는다. */
		void SettleOn(const FLNPWorldHit& Hit, const FParams& Params, FVector& OutLocation, FVector& OutNormal)
		{
			OutNormal = ContactNormal(Hit);
			OutLocation = Hit.Location + OutNormal * Params.SurfaceGap;
		}
	}

	float GoalDistanceSq(const FVector& Location, const FVector& Normal, const FVector& Goal, const float BodyRadius)
	{
		const FVector Delta = Goal - Location;
		const double Along = FVector::DotProduct(Delta, Normal);
		const double Excess = FMath::Max(0.0, FMath::Abs(Along) - BodyRadius);
		return static_cast<float>((Delta - Normal * Along).SizeSquared() + Excess * Excess);
	}

	FVector SteerDirection(const FVector& Location, const FVector& Normal, const FVector& Goal,
		const FVector& GravityUp, const float BodyRadius)
	{
		const FVector ToGoal = Goal - Location;
		const double Along = FVector::DotProduct(ToGoal, Normal);
		FVector Direction = FVector::VectorPlaneProject(ToGoal, Normal);
		if (Along > BodyRadius)
		{
			Direction += FVector::VectorPlaneProject(-GravityUp, Normal) * Along;
		}
		return Direction.GetSafeNormal();
	}

	FVector UpdateDetour(FDetourState& State, const FDetourParams& Params, const FVector& Location, const FVector& Normal,
		const FVector& SteerDir, const FVector& ToGoal, const FVector& GravityUp, const FVector& FallbackSide, const int8 InitialSide,
		const float DeltaTime)
	{
		if (State.Side == 0)
		{
			State.Side = InitialSide >= 0 ? 1 : -1;
		}

		// 막힘 계측은 우회 중에도 돈다 — 우회 방향이 또 막히면 반대로 시도한다.
		if (State.StallAnchor.IsZero() || FVector::DistSquared(Location, State.StallAnchor) > FMath::Square(Params.StallRadius))
		{
			State.StallAnchor = Location;
			State.StallTime = 0.f;
		}
		else
		{
			State.StallTime += DeltaTime;
		}
		if (State.StallTime >= Params.StallTimeout)
		{
			if (State.DetourTime > 0.f)
			{
				State.Side = -State.Side;
			}
			State.StallAnchor = Location;
			State.StallTime = 0.f;
			State.DetourTime = Params.Duration;
			State.DetourNormal = Normal;
			State.DetourOrigin = Location;
		}

		if (State.DetourTime <= 0.f)
			return SteerDir;

		State.DetourTime -= DeltaTime;
		// 시작 위치 근처(떨리는 모서리)에서는 기준 면을 계속 갱신한다. 기준 면은 그 자리를 떠날 때 붙어 있던 면이다.
		if (FVector::DistSquared(Location, State.DetourOrigin) <= FMath::Square(Params.StallRadius))
		{
			State.DetourNormal = Normal;
		}
		else if (FVector::DotProduct(Normal, State.DetourNormal) < Params.NewSurfaceDot)
		{
			// 다른 면에 올라섰다. 새 면에서는 일반 조향이 목표 쪽으로 데려간다.
			State.DetourTime = 0.f;
			return SteerDir;
		}

		// 목표 방향이 접평면에서 0이면(목표가 바로 위·아래) 목표 원방향과 몸의 옆으로 차례로 대신한다.
		FVector Toward = SteerDir;
		if (Toward.IsNearlyZero())
		{
			Toward = FVector::VectorPlaneProject(ToGoal, Normal).GetSafeNormal();
		}
		FVector Lateral = FVector::CrossProduct(Normal, Toward).GetSafeNormal();
		if (Lateral.IsNearlyZero())
		{
			Lateral = FVector::VectorPlaneProject(FallbackSide, Normal).GetSafeNormal();
		}
		Lateral *= State.Side;

		const bool bCeiling = FVector::DotProduct(State.DetourNormal, GravityUp) < -0.5;
		return bCeiling ? (Lateral * 0.5 - Toward).GetSafeNormal() : Lateral;
	}

	void ResetDetour(FDetourState& State)
	{
		State.StallAnchor = FVector::ZeroVector;
		State.StallTime = 0.f;
		State.DetourTime = 0.f;
	}

	bool UpdateHomeApproach(FHomeApproachState& State, const FHomeApproachParams& Params, const float DistanceToHome, const float DeltaTime)
	{
		if (DistanceToHome <= Params.FarDistance)
		{
			ResetHomeApproach(State);
			return false;
		}
		if (State.BestDistance <= 0.f || DistanceToHome < State.BestDistance - Params.ProgressMargin)
		{
			State.BestDistance = DistanceToHome;
			State.StallTime = 0.f;
			return false;
		}
		State.StallTime += DeltaTime;
		if (State.StallTime < Params.Timeout)
			return false;
		ResetHomeApproach(State);
		return true;
	}

	void ResetHomeApproach(FHomeApproachState& State)
	{
		State.BestDistance = 0.f;
		State.StallTime = 0.f;
	}

	int32 SelectNearestPod(const FVector& From, const TConstArrayView<FVector> PodPositions)
	{
		int32 Best = INDEX_NONE;
		double BestDistSq = DBL_MAX;
		for (int32 Index = 0; Index < PodPositions.Num(); ++Index)
		{
			const double DistSq = FVector::DistSquared(From, PodPositions[Index]);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Index;
			}
		}
		return Best;
	}

	bool IsCrawlable(const FLNPExactHitIdentity& Identity)
	{
		return Identity.IsKnown() && (Identity.Roles & (ELNPExactSourceRole::Support | ELNPExactSourceRole::Blocker)) != 0;
	}

	EStepResult Step(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& Normal, const FVector& Velocity, const float DeltaTime,
		FVector& OutLocation, FVector& OutNormal)
	{
		OutLocation = Location;
		OutNormal = Normal;

		const FVector Up = Normal.GetSafeNormal();
		const FVector Move = FVector::VectorPlaneProject(Velocity, Up) * DeltaTime;
		if (Up.IsNearlyZero() || Move.SizeSquared() < UE_KINDA_SMALL_NUMBER)
			return EStepResult::Attached;

		// 1. 접평면 sweep. StepUp만큼 띄워 턱을 넘는다. 띄운 자리가 천장에 겹치면(낮은 틈) 띄우지 않는다.
		FVector Lift = Up * Params.StepUp;
		FLNPWorldHit Hit;
		bool bHit = Sweep(Collision, Params, Location + Lift, Location + Lift + Move, Hit);
		if (bHit && Hit.bStartPenetrating)
		{
			Lift = FVector::ZeroVector;
			bHit = Sweep(Collision, Params, Location, Location + Move, Hit);
			if (bHit && Hit.bStartPenetrating)
			{
				// 이미 지형 안에 있다(스폰·넉백 착지 직후 등). 겹침만 풀고 이번 프레임은 전진하지 않는다.
				OutLocation = Location + Hit.ImpactNormal * (Hit.PenetrationDepth + DepenetrationSkin);
				return EStepResult::Attached;
			}
		}

		if (bHit)
		{
			// Unknown에는 붙지 않는다(D-037). 미끄러지지 않고 선다 — 다음 프레임 조향이 다른 방향을 고를 때까지다.
			if (!IsCrawlable(Hit.Identity))
				return EStepResult::Blocked;

			// 오목 모서리: 앞을 막은 면으로 갈아탄다. 띄운 높이만큼 원래 표면 쪽으로 내려앉아 두 면이 만나는 구석에 선다.
			FVector Corner;
			FVector WallNormal;
			SettleOn(Hit, Params, Corner, WallNormal);
			if (!Lift.IsZero())
			{
				FLNPWorldHit Down;
				const FVector DownEnd = Corner - Lift;
				if (!Sweep(Collision, Params, Corner, DownEnd, Down))
				{
					Corner = DownEnd;
				}
				else if (!Down.bStartPenetrating)
				{
					Corner = Down.Location + ContactNormal(Down) * Params.SurfaceGap;
				}
			}
			OutLocation = Corner;
			OutNormal = WallNormal;
			return EStepResult::Attached;
		}

		// 2. 이동한 자리에서 -법선 probe. 평면·완만한 굴곡·볼록 모서리(구가 모서리를 축으로 굴러 돈다)를 모두 이 hit가 맡는다.
		const FVector Moved = Location + Move;
		const FVector ProbeEnd = Moved - Up * (Params.StepDown + Params.SurfaceGap);
		bHit = Sweep(Collision, Params, Moved + Lift, ProbeEnd, Hit);
		if (bHit)
		{
			if (Hit.bStartPenetrating)
			{
				OutLocation = Moved + Lift + Hit.ImpactNormal * (Hit.PenetrationDepth + DepenetrationSkin);
				return EStepResult::Attached;
			}
			if (!IsCrawlable(Hit.Identity))
				return EStepResult::Blocked;
			SettleOn(Hit, Params, OutLocation, OutNormal);
			return EStepResult::Attached;
		}

		// 3. 한 프레임에 모서리를 지나쳤다. probe 끝(빈 공간으로 확인된 자리)에서 원래 표면 아래·이동 반대쪽으로 감싸 sweep한다.
		const FVector WrapEnd = ProbeEnd - Up * Params.BodyRadius - Move.GetSafeNormal() * (Move.Size() + Params.BodyRadius);
		bHit = Sweep(Collision, Params, ProbeEnd, WrapEnd, Hit);
		if (bHit && !Hit.bStartPenetrating)
		{
			if (!IsCrawlable(Hit.Identity))
				return EStepResult::Blocked;
			SettleOn(Hit, Params, OutLocation, OutNormal);
			return EStepResult::Attached;
		}

		OutLocation = Moved;
		return EStepResult::Lost;
	}

	bool Attach(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& SearchDirection, const float Reach, FVector& OutLocation, FVector& OutNormal)
	{
		const FVector Direction = SearchDirection.GetSafeNormal();
		FLNPWorldHit Hit;
		if (Direction.IsNearlyZero()
			|| !Sweep(Collision, Params, Location, Location + Direction * (Params.StepDown + Reach), Hit)
			|| Hit.bStartPenetrating || !IsCrawlable(Hit.Identity))
		{
			return false;
		}
		SettleOn(Hit, Params, OutLocation, OutNormal);
		return true;
	}
}
