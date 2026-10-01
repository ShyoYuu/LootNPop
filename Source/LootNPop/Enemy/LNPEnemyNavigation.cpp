// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyNavigation.h"

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
