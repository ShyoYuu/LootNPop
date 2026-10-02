// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyProcessors.h"
#include "Misc/ScopeExit.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LootNPop.h"
#include "Character/LNPCharacterBase.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPNavGraph.h"
#include "SurfaceNavigation/LNPNavPathSubsystem.h"
#include "SurfaceNavigation/LNPLoadBaseline.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassEntitySubsystem.h"
#include "MassCommandBuffer.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeTypes.h"
#include "LNPMassUtils.h"

namespace
{
	constexpr double WaypointAcceptRadius = 100.0;
	constexpr double RepathGoalDrift = 400.0;
	constexpr double MinRepathSeconds = 0.5;
	constexpr double NavSnapRadius = 300.0;

	struct FGoalProjectionKey
	{
		FVector3d Position;
		FLNPSurfaceHandle Surface;

		bool operator==(const FGoalProjectionKey&) const = default;

		friend uint32 GetTypeHash(const FGoalProjectionKey& Key)
		{
			// FVector의 바이트 해시도 좌표 비교처럼 +0과 -0을 같은 키로 취급한다.
			const FVector3d Position(Key.Position.X == 0.0 ? 0.0 : Key.Position.X,
				Key.Position.Y == 0.0 ? 0.0 : Key.Position.Y, Key.Position.Z == 0.0 ? 0.0 : Key.Position.Z);
			return HashCombineFast(GetTypeHash(Position), HashCombineFast(GetTypeHash(Key.Surface.Generation),
				HashCombineFast(GetTypeHash(Key.Surface.OctantSlot), GetTypeHash(Key.Surface.LocalLayerId))));
		}
	};

	FVector3d FeetOf(const FVector3d& Center, const double HalfHeight)
	{
		return Center + Center.GetSafeNormal() * HalfHeight;
	}

	int32 NearestNode(const FLNPNavSnapshot& Nav, const FLNPSurfaceHandle& Surface,
		const FVector3d& Position, const FLNPNavOverlay* Overlay)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_NearestNode);
		if (!Surface.IsValid() || Surface.Generation != Nav.SnapshotGeneration)
		{
			return INDEX_NONE;
		}
		TArray<FLNPNavGraphCandidate> Candidates;
		LNPNavGraph::CollectNodesNear(Nav, Surface.OctantSlot, Surface.LocalLayerId,
			Position, NavSnapRadius, Candidates, Overlay, true);
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
	bRequiresGameThreadExecution = true;
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
				&& !Character->GetMoverComponent()->IsAirborne()
				&& Character->GetMoverComponent()->TryGetFloorCheckHitResult(FloorHit)
				&& FloorHit.IsValidBlockingHit();
			if (Value.bGrounded)
			{
				Value.Surface = FLNPSurfaceHandle();
				Value.Node = INDEX_NONE;
				Value.Group = MAX_uint32;
				Value.GroundGroup = FLNPNavGroupRef();
				FLNPSurfaceHandle Surface;
				const FLNPExactHitIdentity Identity = ULNPHitIdentitySubsystem::ResolveHit(*IdentitySnapshot, FloorHit);
				if (LNPEnemySurfaceMovement::MakeSurfaceHandle(Identity, Surface))
				{
					const FVector3d Feet = FloorHit.ImpactPoint;
					const int32 Node = NearestNode(Snapshot->Nav, Surface, Feet, nullptr);
					if (Node != INDEX_NONE)
					{
						Value.Surface = Surface;
						Value.GroundPoint = Feet;
						Value.Node = Node;
						Value.Group = LNPNavGraph::GetGroup(Snapshot->Nav, Node);
						Value.GroundGroup = {Value.Group, Snapshot->Nav.ConnectivityGraphVersion, Snapshot->Generation};
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
	: PathQuery(*this), PodQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ExecutionOrder.ExecuteAfter.Add(ULNPEnemyTargetFollowProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(ULNPPlayerNavProcessor::StaticClass()->GetFName());
}

void ULNPEnemyPathProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	PathQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PathQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	PathQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	PathQuery.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadOnly);
	PathQuery.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
	PathQuery.AddRequirement<FLNPEnemyIdleFragment>(EMassFragmentAccess::ReadWrite);
	PathQuery.AddRequirement<FLNPEnemyPathFragment>(EMassFragmentAccess::ReadWrite);
	PathQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	PathQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	PathQuery.AddTagRequirement<FLNPEnemyFlyingTag>(EMassFragmentPresence::None);
	PathQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	PathQuery.RegisterWithProcessor(*this);
	PodQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PodQuery.AddRequirement<FLNPLootPodFragment>(EMassFragmentAccess::ReadWrite);
	PodQuery.AddTagRequirement<FLNPLootPodTag>(EMassFragmentPresence::All);
	PodQuery.RegisterWithProcessor(*this);
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
	const double ConsumerBegin = FPlatformTime::Seconds();
	ON_SCOPE_EXIT { Paths->SetLastConsumerSeconds(FPlatformTime::Seconds() - ConsumerBegin); };
	const TSharedPtr<const FLNPNavOverlay, ESPMode::ThreadSafe> Overlay = Paths->TakeOverlay();
	// 한 실행의 고정 snapshot·overlay 안에서만 동일한 목표 투영을 공유한다. NoNode 결과도 다음 실행에는 다시 조회한다.
	TMap<FGoalProjectionKey, int32> GoalProjections;
	const double Now = World->GetTimeSeconds();
	TArray<FMassEntityHandle> HomesChanged;
	TArray<FLNPEnemyHomePod> Pods;
	PodQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_Pods);
		const auto Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const auto PodData = Ctx.GetMutableFragmentView<FLNPLootPodFragment>();
		for (int32 Index = 0; Index < Ctx.GetNumEntities(); ++Index)
		{
			FLNPLootPodFragment& Pod = PodData[Index];
			if (Pod.State == ELNPLootPodState::Popped)
			{
				continue;
			}
			const FVector3d Position = Transforms[Index].GetTransform().GetLocation();
			if (!Pod.NavGroup.IsValid() || LNPNavQuery::TestReachability(Nav, Pod.NavGroup, Pod.NavGroup) == ELNPNavReachability::Stale)
			{
				Pod.NavNode = NearestNode(Nav, Pod.SurfaceHandle, Position, nullptr);
				Pod.NavGroup = {LNPNavGraph::GetGroup(Nav, Pod.NavNode), Nav.ConnectivityGraphVersion, Nav.SnapshotGeneration};
			}
			if (Pod.NavGroup.IsValid())
			{
				Pods.Add({Ctx.GetEntity(Index), Position, Pod.SurfaceHandle, Pod.NavGroup});
			}
		}
	});
	PathQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const auto Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const auto MoveTargets = Ctx.GetFragmentView<FMassMoveTargetFragment>();
		const auto Enemies = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		const auto Velocities = Ctx.GetFragmentView<FLNPEnemyVelocityFragment>();
		const auto Targeting = Ctx.GetFragmentView<FLNPEnemyTargetingFragment>();
		const auto Idle = Ctx.GetMutableFragmentView<FLNPEnemyIdleFragment>();
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
			State.bApproachingUnreachable = false;
			const FVector3d Center = Transforms[Index].GetTransform().GetLocation();
			const FVector3d Start = FeetOf(Center, Shared.Config->CapsuleHalfHeight);
			FLNPEnemyFragment& Enemy = Enemies[Index];
			if (!Velocities[Index].Velocity.IsNearlyZero() || !Enemy.SurfaceHandle.IsValid()
				|| Enemy.DynamicSupportContact.IsValid())
			{
				Enemy.bNeedsHomeCheck |= Enemy.ParentLootPod.IsValid() || Enemy.bOrphaned;
				Paths->Cancel(Owner);
				State.Path.Reset();
				State.RequestSerial = 0;
				continue;
			}
			if (Enemy.bNeedsHomeCheck || (Enemy.bOrphaned && Now - Enemy.LastHomeCheckTime >= 1.0))
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_HomeCheck);
				int32 HomeIndex;
				const ELNPNavEndpointStatus HomeStatus = LNPEnemyNavigation::SelectHomePod(
					Nav, Start, Enemy.SurfaceHandle, Enemy.ParentLootPod, Pods, HomeIndex);
				if (HomeStatus != ELNPNavEndpointStatus::Stale)
				{
					const FMassEntityHandle OldParent = Enemy.ParentLootPod;
					const bool bLanded = Enemy.bNeedsHomeCheck;
					Enemy.bNeedsHomeCheck = false;
					Enemy.LastHomeCheckTime = Now;
					Enemy.bOrphaned = HomeIndex == INDEX_NONE;
					Enemy.ParentLootPod = HomeIndex != INDEX_NONE ? Pods[HomeIndex].Entity : FMassEntityHandle();
					if (HomeIndex != INDEX_NONE)
					{
						Enemy.ParentPodLocation = Pods[HomeIndex].Position;
					}
					else if (bLanded)
					{
						Enemy.ParentPodLocation = Start;
					}
					if (bLanded || OldParent != Enemy.ParentLootPod)
					{
						Paths->Cancel(Owner);
						State.Path.Reset();
						State.RequestSerial = 0;
						Idle[Index].bNeedNewWanderTarget = true;
						Idle[Index].bWanderTargetTimedOut = true;
						HomesChanged.Add(Owner);
					}
					if (OldParent != Enemy.ParentLootPod)
					{
						UE_LOG(LogLootNPop, Log, TEXT("[SurfaceNavHome] enemy=%d oldPod=%d newPod=%d orphaned=%d"),
							Owner.Index, OldParent.Index, Enemy.ParentLootPod.Index, Enemy.bOrphaned);
					}
				}
			}

			const FMassMoveTargetFragment& MoveTarget = MoveTargets[Index];
			const FLNPEnemyTargetingFragment& Target = Targeting[Index];
			FVector3d SyntheticGoal = FVector3d::ZeroVector;
			FLNPSurfaceHandle SyntheticSurface;
			const ULNPLoadBaselineSubsystem* Baseline = LNPLoadBaseline::IsSyntheticChase()
				? World->GetSubsystem<ULNPLoadBaselineSubsystem>() : nullptr;
			const bool bSynthetic = Baseline && Baseline->GetSyntheticChaseGoal(Owner.Index, SyntheticGoal, SyntheticSurface);
			const bool bTargetActive = Target.TargetPlayer.IsValid() && EntityManager.IsEntityActive(Target.TargetPlayer);
			const bool bChasing = bSynthetic || (Target.State == ELNPTargetingState::Confirmed && bTargetActive);
			bool bAlertApproach = false;
			bool bUnreachableTarget = false;
			if (!bSynthetic && Target.State != ELNPTargetingState::None && bTargetActive)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_TargetReachability);
				const FLNPPlayerNavFragment* PlayerNav = EntityManager.GetFragmentDataPtr<FLNPPlayerNavFragment>(Target.TargetPlayer);
				if (PlayerNav && PlayerNav->Surface.IsValid())
				{
					FLNPNavEndpointQuery Query;
					Query.StartPosition = Start;
					Query.StartSurface = &Enemy.SurfaceHandle;
					Query.GoalPosition = Target.TargetLocation;
					Query.GoalSurface = &PlayerNav->Surface;
					Query.ApproachRadius = 0.0;
					const ELNPNavEndpointStatus Reachability = LNPNavGraph::ResolveEndpoints(Nav, Query).Status;
					bUnreachableTarget = Reachability == ELNPNavEndpointStatus::Unreachable || Reachability == ELNPNavEndpointStatus::NoNode;
					bAlertApproach = Target.State == ELNPTargetingState::Alert && bUnreachableTarget;
				}
			}
			const bool bTargetPath = bChasing || bAlertApproach;
			if (!bTargetPath && (Target.State != ELNPTargetingState::None || Idle[Index].bNeedNewWanderTarget))
			{
				Paths->Cancel(Owner);
				State.Path.Reset();
				State.RequestSerial = 0;
				if (Target.State == ELNPTargetingState::None)
				{
					State.SteeringPoint = Center;
					State.bHasSteeringPoint = true;
				}
				continue;
			}
			if (!bSynthetic && !bAlertApproach && MoveTarget.DistanceToGoal <= FLNPEnemyMovementConfig::ArrivalTolerance)
			{
				Paths->Cancel(Owner);
				State.Path.Reset();
				State.RequestSerial = 0;
				continue;
			}
			FLNPSurfaceHandle GoalSurface = Enemy.SurfaceHandle;
			if (bSynthetic)
			{
				GoalSurface = SyntheticSurface;
			}
			else if (bTargetPath)
			{
				const FLNPPlayerNavFragment* PlayerNav = EntityManager.GetFragmentDataPtr<FLNPPlayerNavFragment>(Target.TargetPlayer);
				if (PlayerNav == nullptr || !PlayerNav->Surface.IsValid())
				{
					continue;
				}
				GoalSurface = PlayerNav->Surface;
			}
			const FVector3d Goal = bSynthetic ? SyntheticGoal : bUnreachableTarget ? FVector3d(Target.TargetLocation)
				: FeetOf(MoveTarget.Center, Shared.Config->CapsuleHalfHeight);
			int32 StartNode;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_StartProjection);
				StartNode = NearestNode(Nav, Enemy.SurfaceHandle, Start, Overlay.Get());
			}
			int32 GoalNode;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_GoalProjection);
				const FGoalProjectionKey Key{Goal, GoalSurface};
				if (const int32* Cached = GoalProjections.Find(Key))
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_GoalProjectionHit);
					GoalNode = *Cached;
				}
				else
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_GoalProjectionMiss);
					GoalNode = NearestNode(Nav, GoalSurface, Goal, Overlay.Get());
					GoalProjections.Add(Key, GoalNode);
				}
			}
			if (StartNode == INDEX_NONE)
			{
				State.SteeringPoint = Center;
				State.bHasSteeringPoint = true;
				continue;
			}
			const uint32 GoalTile = GoalNode == INDEX_NONE ? MAX_uint32 : LNPNavGraph::GetTileKey(Nav, GoalNode);
			const bool bGoalChanged = State.RequestedTarget != Target.TargetPlayer || State.GoalTile != GoalTile
				|| (State.Status != ELNPNavPathStatus::Unreachable && FVector3d::Dist(State.RequestedGoal, Goal) > RepathGoalDrift);
			const bool bPathInvalid = State.Path.IsValid() && !State.Path->IsCurrent(Nav, Overlay.Get());
			const bool bNoPathOverlayChanged = !State.Path.IsValid() && State.RequestSerial != 0
				&& State.RequestedOverlayRevision != (Overlay.IsValid() ? Overlay->Revision : 0);
			if (bPathInvalid || bGoalChanged || bNoPathOverlayChanged)
			{
				State.Path.Reset();
			}
			bool bDirectGoal;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_DirectGoal);
				bDirectGoal = GoalNode != INDEX_NONE && LNPNavGraph::IsDirectWalkable(Nav, StartNode, GoalNode, Overlay.Get());
			}
			if (bDirectGoal)
			{
				Paths->RecordFollowerFrame(false, 0);
				if (State.RequestSerial != 0)
				{
					Paths->Cancel(Owner);
				}
				State.Path.Reset();
				State.RequestSerial = 0;
				State.Status = ELNPNavPathStatus::None;
				if (bSynthetic)
				{
					State.SteeringPoint = Goal;
					State.bHasSteeringPoint = true;
				}
				continue;
			}
			if (State.RequestSerial != 0 && !bGoalChanged && !bPathInvalid && !bNoPathOverlayChanged)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_Result);
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
				TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_Submit);
				FLNPNavPathRequest Request;
				Request.Owner = Owner;
				Request.Priority = bChasing ? ELNPNavPathPriority::Chase : ELNPNavPathPriority::Background;
				Request.StartPosition = Start;
				Request.StartSurface = Enemy.SurfaceHandle;
				Request.GoalPosition = Goal;
				Request.GoalSurface = GoalSurface;
				Request.ApproachRadius = bTargetPath ? 3000.0 : 0.0;
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
				TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_FollowWaypoints);
				State.bApproachingUnreachable = State.Status == ELNPNavPathStatus::Unreachable
					|| State.Status == ELNPNavPathStatus::NoNode;
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
				else if (State.bApproachingUnreachable && !Waypoints.IsEmpty())
				{
					// 접근점에 도착한 뒤 의미상 목표 방향으로 다시 걷지 않는다.
					State.SteeringPoint = Waypoints.Last().Location;
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
	if (!HomesChanged.IsEmpty())
	{
		Context.GetMutableSubsystemChecked<UMassSignalSubsystem>().SignalEntities(UE::Mass::Signals::StateTreeActivate, HomesChanged);
	}
}
