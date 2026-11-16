// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyNavigation.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

bool LNPEnemyNavigation::UpdateMeleeSlot(const FLNPNavSnapshot& Nav, const ELNPNavEndpointStatus Status,
	const FMassEntityHandle Player, const FLNPNavGroupRef& GroundGroup, const bool bGrounded,
	const bool bOccupied, const double Now, FLNPEnemySlotReachability& State)
{
	if (Status == ELNPNavEndpointStatus::Stale || !GroundGroup.IsValid()
		|| LNPNavQuery::TestReachability(Nav, GroundGroup, GroundGroup) == ELNPNavReachability::Stale)
	{
		return bOccupied;
	}
	if (!bGrounded)
	{
		State.UnreachableSince = -1.0;
		return bOccupied;
	}
	if (State.Player != Player || State.GroundGroup.Group != GroundGroup.Group
		|| State.GroundGroup.SnapshotGeneration != GroundGroup.SnapshotGeneration
		|| State.GroundGroup.ConnectivityGraphVersion != GroundGroup.ConnectivityGraphVersion)
	{
		State.Player = Player;
		State.GroundGroup = GroundGroup;
		State.UnreachableSince = -1.0;
	}
	if (Status == ELNPNavEndpointStatus::Reachable)
	{
		State.UnreachableSince = -1.0;
		return true;
	}
	if (!bOccupied)
	{
		return false;
	}
	if (State.UnreachableSince < 0.0)
	{
		State.UnreachableSince = Now;
	}
	return Now - State.UnreachableSince < 1.5;
}

ELNPNavEndpointStatus LNPEnemyNavigation::SelectHomePod(const FLNPNavSnapshot& Nav,
	const FVector3d& Feet, const FLNPSurfaceHandle& Surface, const FMassEntityHandle Parent,
	const TConstArrayView<FLNPEnemyHomePod> Pods, int32& OutPod)
{
	OutPod = INDEX_NONE;
	if (!Surface.IsValid() || Surface.Generation != Nav.SnapshotGeneration)
	{
		return ELNPNavEndpointStatus::Stale;
	}
	double BestDistance = DBL_MAX;
	for (int32 Index = 0; Index < Pods.Num(); ++Index)
	{
		const FLNPEnemyHomePod& Pod = Pods[Index];
		if (LNPNavQuery::TestReachability(Nav, Pod.Group, Pod.Group) == ELNPNavReachability::Stale)
		{
			return ELNPNavEndpointStatus::Stale;
		}
		FLNPNavEndpointQuery Query;
		Query.StartPosition = Feet;
		Query.StartSurface = &Surface;
		Query.GoalPosition = Pod.Position;
		Query.GoalSurface = &Pod.Surface;
		Query.ApproachRadius = 0.0;
		const FLNPNavEndpoints Ends = LNPNavGraph::ResolveEndpoints(Nav, Query);
		if (Ends.Status == ELNPNavEndpointStatus::Stale)
		{
			return Ends.Status;
		}
		if (Ends.Status != ELNPNavEndpointStatus::Reachable)
		{
			continue;
		}
		if (Pod.Entity == Parent)
		{
			OutPod = Index;
			return ELNPNavEndpointStatus::Reachable;
		}
		const double Distance = FVector3d::DistSquared(Feet, Pod.Position);
		if (Distance < BestDistance || (Distance == BestDistance && OutPod != INDEX_NONE
			&& Pod.Entity.Index < Pods[OutPod].Entity.Index))
		{
			BestDistance = Distance;
			OutPod = Index;
		}
	}
	return OutPod != INDEX_NONE ? ELNPNavEndpointStatus::Reachable : ELNPNavEndpointStatus::Unreachable;
}

bool LNPEnemyNavigation::WalkReaches(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
	const LNPEnemyExactMovement::FParams& Params, const FVector& StartCenter, const FLNPSurfaceHandle& Surface,
	const FVector& GoalFeet, const double AcceptRadius)
{
	if (!Surface.IsValid())
	{
		return false;
	}
	// 한 걸음은 캡슐 반지름이다. 접지 sweep은 연속 판정이라 걸음 사이의 벽을 건너뛰지 않는다.
	constexpr float StepSeconds = 1.f / 30.f;
	const double StepLength = FMath::Max(static_cast<double>(Params.CapsuleRadius), 10.0);
	auto FeetAt = [&Params](const FVector& Center)
	{
		return Center - (Params.GravityOrigin - Center).GetSafeNormal() * Params.CapsuleHalfHeight;
	};
	const int32 MaxSteps = FMath::CeilToInt(FVector::Distance(FeetAt(StartCenter), GoalFeet) * 1.5 / StepLength) + 2;
	FVector Location = StartCenter;
	FLNPSurfaceHandle Walking = Surface;
	for (int32 Step = 0; Step <= MaxSteps; ++Step)
	{
		const FVector Up = (Params.GravityOrigin - Location).GetSafeNormal();
		const FVector Delta = GoalFeet - FeetAt(Location);
		const FVector Planar = FVector::VectorPlaneProject(Delta, Up);
		const double PlanarDistance = Planar.Size();
		// 접평면으로 닿아도 높이가 크게 다르면(다른 층·절벽 위아래) 닿은 것이 아니다.
		if (PlanarDistance <= AcceptRadius
			&& FMath::Abs(FVector::DotProduct(Delta, Up)) <= AcceptRadius + Params.MaxStepUp)
		{
			return true;
		}
		if (Step == MaxSteps)
		{
			break;
		}
		const FVector Velocity = Planar.GetSafeNormal() * (FMath::Min(StepLength, PlanarDistance) / StepSeconds);
		FVector Next;
		FVector PhysVelocity;
		const LNPEnemyExactMovement::EGroundResult Result = LNPEnemySurfaceMovement::StepGrounded(
			&Snapshot, Collision, Params, Location, Velocity, StepSeconds, Next, PhysVelocity, Walking);
		if (Result != LNPEnemyExactMovement::EGroundResult::Grounded || !Walking.IsValid()
			|| Walking.OctantSlot != Surface.OctantSlot || Walking.LocalLayerId != Surface.LocalLayerId)
		{
			return false;
		}
		// 벽에 막혀 제자리걸음이면 실패다.
		if (FVector::DistSquared(Next, Location) < 1.0)
		{
			return false;
		}
		Location = Next;
	}
	return false;
}

int32 LNPEnemyNavigation::FindWalkAnchor(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
	const LNPEnemyExactMovement::FParams& Params, const FVector& Center, const FLNPSurfaceHandle& Surface,
	const FLNPNavOverlay* Overlay, const bool bWalkFromNode, const double AcceptRadius)
{
	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	if (!Nav.Graph.IsValid() || !Surface.IsValid() || Surface.Generation != Nav.SnapshotGeneration)
	{
		return INDEX_NONE;
	}
	const FVector Feet = Center - (Params.GravityOrigin - Center).GetSafeNormal() * Params.CapsuleHalfHeight;
	TArray<FLNPNavGraphCandidate> Candidates;
	LNPNavGraph::CollectNodesNear(Nav, Surface.OctantSlot, Surface.LocalLayerId, FVector3d(Feet), WalkAnchorSearchRadius,
		Candidates, Overlay);
	const int32 Count = FMath::Min(Candidates.Num(), WalkAnchorMaxCandidates);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Node = Candidates[Index].Node;
		const FVector Point(Nav.Graph.GetWorldPoint(Nav.Graph.GetSlot(Node), Node));
		const bool bWalkable = bWalkFromNode
			? WalkReaches(Snapshot, Collision, Params,
				Point + (Params.GravityOrigin - Point).GetSafeNormal() * Params.CapsuleHalfHeight, Surface, Feet, AcceptRadius)
			: WalkReaches(Snapshot, Collision, Params, Center, Surface, Point, AcceptRadius);
		if (bWalkable)
		{
			return Node;
		}
	}
	return INDEX_NONE;
}
