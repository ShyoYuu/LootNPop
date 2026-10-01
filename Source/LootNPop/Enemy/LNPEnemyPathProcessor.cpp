// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyProcessors.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "Character/LNPCharacterBase.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPNavGraph.h"
#include "SurfaceNavigation/LNPNavPathSubsystem.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassEntitySubsystem.h"
#include "MassCommandBuffer.h"
#include "LNPMassUtils.h"

namespace
{
	constexpr double WaypointAcceptRadius = 100.0;
	constexpr double RepathGoalDrift = 400.0;
	constexpr double MinRepathSeconds = 0.5;
	constexpr double NavSnapRadius = 300.0;

	FVector3d FeetOf(const FVector3d& Center, const double HalfHeight)
	{
		return Center + Center.GetSafeNormal() * HalfHeight;
	}

	int32 NearestNode(const FLNPNavSnapshot& Nav, const FLNPSurfaceHandle& Surface,
		const FVector3d& Position, const FLNPNavOverlay* Overlay)
	{
		if (!Surface.IsValid() || Surface.Generation != Nav.SnapshotGeneration)
		{
			return INDEX_NONE;
		}
		TArray<FLNPNavGraphCandidate> Candidates;
		LNPNavGraph::CollectNodesNear(Nav, Surface.OctantSlot, Surface.LocalLayerId,
			Position, NavSnapRadius, Candidates, Overlay);
		return Candidates.IsEmpty() ? INDEX_NONE : Candidates[0].Node;
	}

	bool IsFinished(const ELNPNavPathStatus Status)
	{
		return Status == ELNPNavPathStatus::Succeeded || Status == ELNPNavPathStatus::NoPath
			|| Status == ELNPNavPathStatus::Unreachable || Status == ELNPNavPathStatus::NoNode
			|| Status == ELNPNavPathStatus::Stale || Status == ELNPNavPathStatus::Cancelled;
	}
}

ULNPPlayerNavProcessor::ULNPPlayerNavProcessor()
	: PlayerNavQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ExecutionOrder.ExecuteBefore.Add(ULNPEnemyPathProcessor::StaticClass()->GetFName());
}

void ULNPPlayerNavProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	PlayerNavQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	PlayerNavQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerNavQuery.AddRequirement<FLNPPlayerNavFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	PlayerNavQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerNavQuery.RegisterWithProcessor(*this);
}

void ULNPPlayerNavProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (LNPMass::IsClientWorld(EntityManager))
	{
		return;
	}
	UWorld* World = GetWorld();
	const ULNPSurfaceDataSubsystem* SurfaceData = World ? World->GetSubsystem<ULNPSurfaceDataSubsystem>() : nullptr;
	const ULNPHitIdentitySubsystem* HitIdentity = World ? World->GetSubsystem<ULNPHitIdentitySubsystem>() : nullptr;
	const TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot =
		SurfaceData ? SurfaceData->TakeSnapshot() : nullptr;
	if (!Snapshot.IsValid() || !Snapshot->Nav.Graph.IsValid() || HitIdentity == nullptr)
	{
		return;
	}
	const TSharedRef<const FLNPHitIdentitySnapshot, ESPMode::ThreadSafe> IdentitySnapshot = HitIdentity->GetSnapshot();
	PlayerNavQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FMassActorFragment> Actors = Ctx.GetFragmentView<FMassActorFragment>();
		const TArrayView<FLNPPlayerNavFragment> NavFragments = Ctx.GetMutableFragmentView<FLNPPlayerNavFragment>();
		for (int32 Index = 0; Index < Ctx.GetNumEntities(); ++Index)
		{
			FLNPPlayerNavFragment Value = NavFragments.IsValidIndex(Index) ? NavFragments[Index] : FLNPPlayerNavFragment();
			const ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(Actors[Index].Get());
			FHitResult FloorHit;
			Value.bGrounded = Character && Character->GetMoverComponent()
				&& Character->GetMoverComponent()->TryGetFloorCheckHitResult(FloorHit)
				&& FloorHit.IsValidBlockingHit();
			if (Value.bGrounded)
			{
				FLNPSurfaceHandle Surface;
				const FLNPExactHitIdentity Identity = ULNPHitIdentitySubsystem::ResolveHit(*IdentitySnapshot, FloorHit);
				if (LNPEnemySurfaceMovement::MakeSurfaceHandle(Identity, Surface))
				{
					const FVector3d Feet = FloorHit.ImpactPoint;
					const int32 Node = NearestNode(Snapshot->Nav, Surface, Feet, nullptr);
					if (Node != INDEX_NONE)
					{
						Value.Surface = Surface;
						Value.Node = Node;
						Value.Group = LNPNavGraph::GetGroup(Snapshot->Nav, Node);
					}
				}
			}
			if (NavFragments.IsValidIndex(Index))
			{
				NavFragments[Index] = Value;
			}
			else
			{
				Ctx.Defer().PushCommand<FMassCommandAddFragmentInstances<FLNPPlayerNavFragment>>(Ctx.GetEntity(Index), Value);
			}
		}
	});
}

ULNPEnemyPathProcessor::ULNPEnemyPathProcessor()
	: PathQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ExecutionOrder.ExecuteAfter.Add(ULNPEnemyTargetFollowProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(ULNPPlayerNavProcessor::StaticClass()->GetFName());
}

void ULNPEnemyPathProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	PathQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PathQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	PathQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly);
	PathQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadOnly);
	PathQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	PathQuery.AddRequirement<FLNPEnemyIdleFragment>(EMassFragmentAccess::ReadOnly);
	PathQuery.AddRequirement<FLNPEnemyPathFragment>(EMassFragmentAccess::ReadWrite);
	PathQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	PathQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	PathQuery.AddTagRequirement<FLNPEnemyFlyingTag>(EMassFragmentPresence::None);
	PathQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	PathQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyPathProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (LNPMass::IsClientWorld(EntityManager))
	{
		return;
	}
	UWorld* World = GetWorld();
	ULNPNavPathSubsystem* Paths = World ? World->GetSubsystem<ULNPNavPathSubsystem>() : nullptr;
	const ULNPSurfaceDataSubsystem* SurfaceData = World ? World->GetSubsystem<ULNPSurfaceDataSubsystem>() : nullptr;
	const TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot =
		SurfaceData ? SurfaceData->TakeSnapshot() : nullptr;
	if (Paths == nullptr || !Snapshot.IsValid() || !Snapshot->Nav.Graph.IsValid())
	{
		return;
	}
	const FLNPNavSnapshot& Nav = Snapshot->Nav;
	const TSharedPtr<const FLNPNavOverlay, ESPMode::ThreadSafe> Overlay = Paths->TakeOverlay();
	const double Now = World->GetTimeSeconds();
	PathQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const auto Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const auto MoveTargets = Ctx.GetFragmentView<FMassMoveTargetFragment>();
		const auto Enemies = Ctx.GetFragmentView<FLNPEnemyFragment>();
		const auto Velocities = Ctx.GetFragmentView<FLNPEnemyVelocityFragment>();
		const auto Targeting = Ctx.GetFragmentView<FLNPEnemyTargetingFragment>();
		const auto Idle = Ctx.GetFragmentView<FLNPEnemyIdleFragment>();
		const auto PathData = Ctx.GetMutableFragmentView<FLNPEnemyPathFragment>();
		const FLNPEnemySharedFragment& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
		if (Shared.Config == nullptr)
		{
			return;
		}
		for (int32 Index = 0; Index < Ctx.GetNumEntities(); ++Index)
		{
			const FMassEntityHandle Owner = Ctx.GetEntity(Index);
			FLNPEnemyPathFragment& State = PathData[Index];
			State.bHasSteeringPoint = false;
			const FVector3d Center = Transforms[Index].GetTransform().GetLocation();
			const FVector3d Start = FeetOf(Center, Shared.Config->CapsuleHalfHeight);
			const FLNPEnemyFragment& Enemy = Enemies[Index];
			if (!Velocities[Index].Velocity.IsNearlyZero() || !Enemy.SurfaceHandle.IsValid()
				|| Enemy.DynamicSupportContact.IsValid())
			{
				Paths->Cancel(Owner);
				State.Path.Reset();
				State.RequestSerial = 0;
				continue;
			}

			const FMassMoveTargetFragment& MoveTarget = MoveTargets[Index];
			const FLNPEnemyTargetingFragment& Target = Targeting[Index];
			const bool bChasing = Target.State == ELNPTargetingState::Confirmed && Target.TargetPlayer.IsValid();
			if (!bChasing && (Target.State != ELNPTargetingState::None || Idle[Index].bNeedNewWanderTarget))
			{
				Paths->Cancel(Owner);
				State.Path.Reset();
				State.RequestSerial = 0;
				continue;
			}
			if (MoveTarget.DistanceToGoal <= FLNPEnemyMovementConfig::ArrivalTolerance)
			{
				Paths->Cancel(Owner);
				State.Path.Reset();
				State.RequestSerial = 0;
				continue;
			}
			FLNPSurfaceHandle GoalSurface = Enemy.SurfaceHandle;
			if (bChasing)
			{
				const FLNPPlayerNavFragment* PlayerNav = EntityManager.GetFragmentDataPtr<FLNPPlayerNavFragment>(Target.TargetPlayer);
				if (PlayerNav == nullptr || !PlayerNav->Surface.IsValid())
				{
					continue;
				}
				GoalSurface = PlayerNav->Surface;
			}
			const FVector3d Goal = FeetOf(MoveTarget.Center, Shared.Config->CapsuleHalfHeight);
			const int32 StartNode = NearestNode(Nav, Enemy.SurfaceHandle, Start, Overlay.Get());
			const int32 GoalNode = NearestNode(Nav, GoalSurface, Goal, Overlay.Get());
			if (StartNode == INDEX_NONE)
			{
				State.SteeringPoint = Center;
				State.bHasSteeringPoint = true;
				continue;
			}
			const uint32 GoalTile = GoalNode == INDEX_NONE ? MAX_uint32 : LNPNavGraph::GetTileKey(Nav, GoalNode);
			const bool bGoalChanged = State.RequestedTarget != Target.TargetPlayer
				|| State.GoalTile != GoalTile || FVector3d::Dist(State.RequestedGoal, Goal) > RepathGoalDrift;
			const bool bPathInvalid = State.Path.IsValid() && !State.Path->IsCurrent(Nav, Overlay.Get());
			const bool bNoPathOverlayChanged = !State.Path.IsValid() && State.RequestSerial != 0
				&& State.RequestedOverlayRevision != (Overlay.IsValid() ? Overlay->Revision : 0);
			if (bPathInvalid || bGoalChanged || bNoPathOverlayChanged)
			{
				State.Path.Reset();
			}
			if (GoalNode != INDEX_NONE && LNPNavGraph::IsDirectWalkable(Nav, StartNode, GoalNode, Overlay.Get()))
			{
				Paths->RecordFollowerFrame(false, 0);
				if (State.RequestSerial != 0)
				{
					Paths->Cancel(Owner);
				}
				State.Path.Reset();
				State.RequestSerial = 0;
				State.Status = ELNPNavPathStatus::None;
				continue;
			}
			if (State.RequestSerial != 0 && !bGoalChanged && !bPathInvalid && !bNoPathOverlayChanged)
			{
				FLNPNavPathResult Result;
				State.Status = Paths->GetResult(Owner, State.RequestSerial, Result);
				if (IsFinished(State.Status) && Result.Path.IsValid() && State.Path != Result.Path)
				{
					State.Path = Result.Path;
					State.WaypointIndex = 0;
				}
			}
			if (!State.Path.IsValid() && (State.RequestSerial == 0 || bGoalChanged || bPathInvalid || bNoPathOverlayChanged
				|| State.Status == ELNPNavPathStatus::Stale)
				&& Now - State.LastRequestTime >= MinRepathSeconds)
			{
				FLNPNavPathRequest Request;
				Request.Owner = Owner;
				Request.Priority = bChasing ? ELNPNavPathPriority::Chase : ELNPNavPathPriority::Background;
				Request.StartPosition = Start;
				Request.StartSurface = Enemy.SurfaceHandle;
				Request.GoalPosition = Goal;
				Request.GoalSurface = GoalSurface;
				Request.ApproachRadius = bChasing ? 3000.0 : 0.0;
				State.RequestSerial = Paths->Submit(Request);
				State.RequestedGoal = Goal;
				State.RequestedTarget = Target.TargetPlayer;
				State.GoalTile = GoalTile;
				State.RequestedOverlayRevision = Overlay.IsValid() ? Overlay->Revision : 0;
				State.LastRequestTime = Now;
				State.Status = ELNPNavPathStatus::Queued;
				++State.Replans;
			}
			if (State.Path.IsValid())
			{
				const TArray<FLNPNavPathWaypoint>& Waypoints = State.Path->Waypoints;
				const int32 PreviousWaypoint = State.WaypointIndex;
				while (Waypoints.IsValidIndex(State.WaypointIndex)
					&& FVector3d::DistSquared(Start, Waypoints[State.WaypointIndex].Location)
						<= FMath::Square(WaypointAcceptRadius))
				{
					++State.WaypointIndex;
				}
				while (Waypoints.IsValidIndex(State.WaypointIndex + 1)
					&& LNPNavGraph::IsDirectWalkable(Nav, StartNode, Waypoints[State.WaypointIndex + 1].Node, Overlay.Get()))
				{
					++State.WaypointIndex;
				}
				if (Waypoints.IsValidIndex(State.WaypointIndex))
				{
					State.SteeringPoint = Waypoints[State.WaypointIndex].Location;
					State.bHasSteeringPoint = true;
				}
				Paths->RecordFollowerFrame(true, State.WaypointIndex - PreviousWaypoint);
			}
			else
			{
				Paths->RecordFollowerFrame(false, 0);
				// 차단된 직선으로 관통하지 않도록 경로 대기 중에는 현재 위치를 조향점으로 쓴다.
				State.SteeringPoint = Center;
				State.bHasSteeringPoint = true;
			}
		}
	});
}
