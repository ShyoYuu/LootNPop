// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyProcessors.h"
#include "Misc/ScopeExit.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyNavigation.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LootNPop.h"
#include "Character/LNPCharacterBase.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
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
	/** 플레이어 보행 anchor: node에서 걸어온 적의 발이 플레이어 발에서 이 거리 안이면 근접 공격 위치로 본다(D-085). */
	constexpr double PlayerWalkAnchorReach = 150.0;
	/** 플레이어가 이만큼 움직이면 캐시와 무관하게 보행 anchor를 다시 검증한다. */
	constexpr double PlayerWalkAnchorMoveTolerance = 50.0;
	/** Nav 밖 적의 마지막 접근 검증에서 MoveTarget 발에 닿았다고 보는 거리. */
	constexpr double FinalApproachAccept = 50.0;
	/** Nav 주머니에서 마지막으로 트인 자리(D-090)로 되돌아갈 때 도착으로 보는 거리. */
	constexpr double LastOpenArrival = 30.0;

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

	/** HintNode는 탐색 창만 줄이고 결과는 바꾸지 않는다(LNPNavGraph::FindNearestNode). 지난 프레임 결과를 넘긴다. */
	int32 NearestNode(const FLNPNavSnapshot& Nav, const FLNPSurfaceHandle& Surface,
		const FVector3d& Position, const FLNPNavOverlay* Overlay, const int32 HintNode = INDEX_NONE)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_NearestNode);
		if (!Surface.IsValid() || Surface.Generation != Nav.SnapshotGeneration)
		{
			return INDEX_NONE;
		}
		return LNPNavGraph::FindNearestNode(Nav, Surface.OctantSlot, Surface.LocalLayerId,
			Position, NavSnapRadius, Overlay, HintNode).Node;
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
	const auto* Collision = World ? World->GetSubsystem<ULNPMassWorldCollisionSubsystem>() : nullptr;
	const TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot =
		SurfaceData ? SurfaceData->TakeSnapshot() : nullptr;
	if (!Snapshot.IsValid() || !Snapshot->Nav.Graph.IsValid() || HitIdentity == nullptr || Collision == nullptr)
	{
		return;
	}
	const TSharedRef<const FLNPHitIdentitySnapshot, ESPMode::ThreadSafe> IdentitySnapshot = HitIdentity->GetSnapshot();
	const double Now = World->GetTimeSeconds();
	// 플레이어 쪽 보행 anchor는 적 설정과 무관한 기본 지상 캡슐로 검증한다(node에 선 적이 플레이어 근처까지 걸어간다).
	LNPEnemyExactMovement::FParams AnchorParams;
	AnchorParams.bLateralSweep = LNPEnemyExactMovement::IsLateralSweepEnabled();
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
				const FLNPSurfaceHandle PreviousSurface = Value.Surface;
				Value.Surface = FLNPSurfaceHandle();
				Value.bBorrowedSurface = false;
				Value.bInNavPocket = false;
				Value.Node = INDEX_NONE;
				Value.Group = MAX_uint32;
				Value.GroundGroup = FLNPNavGroupRef();
				Value.bWalkAnchored = false;
				FLNPSurfaceHandle Surface;
				const FLNPExactHitIdentity Identity = Collision->ResolveFloorHit(*IdentitySnapshot, FloorHit);
				bool bHasSurface = LNPEnemySurfaceMovement::MakeSurfaceHandle(Identity, Surface);
				if (!bHasSurface)
				{
					// 바위(Support 없는 정적 Blocker) 위는 직전 지지면 층으로 계속 투영한다(D-091).
					Value.bBorrowedSurface = LNPEnemyNavigation::BorrowBlockerFloorSurface(
						Identity, PreviousSurface, Snapshot->Nav.SnapshotGeneration, Surface);
					bHasSurface = Value.bBorrowedSurface;
				}
				if (bHasSurface)
				{
					// 물리 바닥은 Nav 투영과 무관하게 갱신한다(D-085).
					const FVector3d Feet = FloorHit.ImpactPoint;
					Value.Surface = Surface;
					Value.GroundPoint = Feet;
					int32 Node = NearestNode(Snapshot->Nav, Surface, Feet, nullptr);
					// Nav 주머니(D-090)는 투영 반경 안이어도 최근접 node가 바위·벽 너머일 수 있다. 적과 같이 보행 anchor로 검증하고,
					// 실패하면 Nav 밖(D-091 도달 불가)으로 둔다. 주머니에서 투영 node를 그대로 쓰면 적이 node에서 영구히 기다린다.
					Value.bInNavPocket = Node != INDEX_NONE && LNPNavGraph::IsInNavPocket(Snapshot->Nav, Node, Feet, nullptr);
					if (Node == INDEX_NONE || Value.bInNavPocket)
					{
						// 투영 반경 밖: node에 선 적이 실제로 걸어서 플레이어 근처에 닿는 node만 anchor로 쓴다. 벽 너머는 실패한다.
						const bool bCached = Value.WalkAnchorGeneration == Snapshot->Nav.SnapshotGeneration
							&& Now - Value.WalkAnchorCheckTime < LNPEnemyNavigation::WalkAnchorRetrySeconds
							&& FVector::DistSquared(Value.WalkAnchorCheckFeet, Feet) <= FMath::Square(PlayerWalkAnchorMoveTolerance);
						if (!bCached)
						{
							const FVector Up = (AnchorParams.GravityOrigin - FVector(Feet)).GetSafeNormal();
							Value.WalkAnchorNode = LNPEnemyNavigation::FindWalkAnchor(*Snapshot, *Collision, AnchorParams,
								FVector(Feet) + Up * AnchorParams.CapsuleHalfHeight, Surface, nullptr, true, PlayerWalkAnchorReach);
							Value.WalkAnchorGeneration = Snapshot->Nav.SnapshotGeneration;
							Value.WalkAnchorCheckTime = Now;
							Value.WalkAnchorCheckFeet = Feet;
						}
						Node = Value.WalkAnchorNode;
						Value.bWalkAnchored = Node != INDEX_NONE;
						if (Value.bWalkAnchored)
						{
							Value.WalkAnchorPoint = Snapshot->Nav.Graph.GetWorldPoint(Snapshot->Nav.Graph.GetSlot(Node), Node);
						}
					}
					if (Node != INDEX_NONE)
					{
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
	PathQuery.AddTagRequirement<FLNPEnemyDormantTag>(EMassFragmentPresence::None);
	PathQuery.AddTagRequirement<FLNPEnemyFlyingTag>(EMassFragmentPresence::None);
	PathQuery.AddTagRequirement<FLNPEnemyCrawlTag>(EMassFragmentPresence::None);
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
	const ULNPMassWorldCollisionSubsystem* Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
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
		// 보행 anchor·마지막 접근 검증은 이동 프로세서와 같은 접지 이동 설정으로 걷는다.
		LNPEnemyExactMovement::FParams WalkParams;
		WalkParams.GravityOrigin = Shared.Config->MovementConfig.GravityOrigin;
		WalkParams.CapsuleRadius = Shared.Config->CapsuleRadius;
		WalkParams.CapsuleHalfHeight = Shared.Config->CapsuleHalfHeight;
		WalkParams.bLateralSweep = LNPEnemyExactMovement::IsLateralSweepEnabled();
		// 투영 반경 밖 시작점의 보행 anchor(D-085). 걸어서 돌아갈 수 있다고 검증된 node만 돌려주며 결과는 잠시 재사용한다.
		auto ResolveWalkAnchor = [&](FLNPEnemyPathFragment& State, const FVector3d& Center, const FLNPSurfaceHandle& Surface)
		{
			if (State.WalkAnchorGeneration != Nav.SnapshotGeneration || Now - State.WalkAnchorCheckTime >= LNPEnemyNavigation::WalkAnchorRetrySeconds)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_WalkAnchor);
				State.WalkAnchorNode = Collision ? LNPEnemyNavigation::FindWalkAnchor(*Snapshot, *Collision, WalkParams,
					FVector(Center), Surface, Overlay.Get(), false, WaypointAcceptRadius) : INDEX_NONE;
				State.WalkAnchorGeneration = Nav.SnapshotGeneration;
				State.WalkAnchorCheckTime = Now;
			}
			return State.WalkAnchorNode;
		};
		auto NodePoint = [&Nav](const int32 Node) { return Nav.Graph.GetWorldPoint(Nav.Graph.GetSlot(Node), Node); };
		for (int32 Index = 0; Index < Ctx.GetNumEntities(); ++Index)
		{
			const FMassEntityHandle Owner = Ctx.GetEntity(Index);
			FLNPEnemyPathFragment& State = PathData[Index];
			State.bHasSteeringPoint = false;
			State.bApproachingUnreachable = false;
			const FVector3d Center = Transforms[Index].GetTransform().GetLocation();
			const FVector3d Start = FeetOf(Center, Shared.Config->CapsuleHalfHeight);
			FLNPEnemyFragment& Enemy = Enemies[Index];
			if (Enemy.ActivityBand == ELNPEnemyActivityBand::Wander && !Enemy.bNeedsHomeCheck)
			{
				Paths->Cancel(Owner);
				State.Path.Reset();
				State.RequestSerial = 0;
				continue;
			}
			if (!Velocities[Index].Velocity.IsNearlyZero() || !Enemy.SurfaceHandle.IsValid()
				|| Enemy.DynamicSupportContact.IsValid())
			{
				Enemy.bNeedsHomeCheck |= Enemy.ParentLootPod.IsValid() || Enemy.bOrphaned;
				Paths->Cancel(Owner);
				State.Path.Reset();
				State.RequestSerial = 0;
				continue;
			}
			if (Enemy.bNeedsHomeCheck || State.HomeConnectivityGraphVersion != Nav.ConnectivityGraphVersion
				|| (Enemy.bOrphaned && Now - Enemy.LastHomeCheckTime >= 1.0))
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
					State.HomeConnectivityGraphVersion = Nav.ConnectivityGraphVersion;
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
				// 마지막 접지 바닥이 Nav 소속 없이 식별된 플레이어(바위 주머니·절벽 아래)도 도달 불가 판정에 넣는다(D-091).
				// 슬롯 판정과 같이 Node 없는 바닥을 판정 불가로 건너뛰지 않고, 도달 불가 접근(D-063)으로 보낸다.
				const bool bOffNavPlayer = PlayerNav && PlayerNav->Node == INDEX_NONE && PlayerNav->Surface.IsValid();
				if (PlayerNav && (PlayerNav->Node != INDEX_NONE || bOffNavPlayer))
				{
					FLNPNavEndpointQuery Query;
					Query.StartPosition = Start;
					Query.StartSurface = &Enemy.SurfaceHandle;
					Query.GoalPosition = PlayerNav->bWalkAnchored ? PlayerNav->WalkAnchorPoint : FVector(Target.TargetLocation);
					Query.GoalSurface = &PlayerNav->Surface;
					Query.ApproachRadius = 0.0;
					FLNPNavEndpoints Ends = LNPNavGraph::ResolveEndpoints(Nav, Query);
					if (Ends.Status == ELNPNavEndpointStatus::NoNode && Ends.StartNode == INDEX_NONE)
					{
						// 시작점이 투영 반경 밖이면 걸어서 돌아갈 수 있는 anchor에서 다시 본다(D-085).
						const int32 Anchor = ResolveWalkAnchor(State, Center, Enemy.SurfaceHandle);
						if (Anchor != INDEX_NONE)
						{
							Query.StartPosition = NodePoint(Anchor);
							Ends = LNPNavGraph::ResolveEndpoints(Nav, Query);
						}
					}
					const ELNPNavEndpointStatus Reachability = Ends.Status;
					bUnreachableTarget = Reachability == ELNPNavEndpointStatus::Unreachable || Reachability == ELNPNavEndpointStatus::NoNode
						|| (bOffNavPlayer && Reachability != ELNPNavEndpointStatus::Stale);
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
			const FLNPPlayerNavFragment* GoalPlayerNav = nullptr;
			if (bSynthetic)
			{
				GoalSurface = SyntheticSurface;
			}
			else if (bTargetPath)
			{
				GoalPlayerNav = EntityManager.GetFragmentDataPtr<FLNPPlayerNavFragment>(Target.TargetPlayer);
				// Nav 소속(투영 node·보행 anchor)이 없는 물리 바닥은 도달 불가 접근(D-063, 목표=플레이어 위치)으로만 쓴다(D-091).
				if (GoalPlayerNav == nullptr || (GoalPlayerNav->Node == INDEX_NONE && !bUnreachableTarget))
				{
					continue;
				}
				GoalSurface = GoalPlayerNav->Surface;
			}
			else if (Idle[Index].bReturningHome && Idle[Index].ReturnGoalSurface.IsValid())
			{
				// 귀환 목표는 Pod 표면에 투영한 점이다. 적의 현재 표면(다른 Layer·옥탄트일 수 있다)으로 찾으면 NoNode가 된다.
				GoalSurface = Idle[Index].ReturnGoalSurface;
			}
			FVector3d Goal = bSynthetic ? SyntheticGoal : bUnreachableTarget ? FVector3d(Target.TargetLocation)
				: FeetOf(MoveTarget.Center, Shared.Config->CapsuleHalfHeight);
			// 마지막 접근 검증의 목표. 보행 anchor로 목표를 바꿔도 실제로 다가갈 곳은 MoveTarget 발이다.
			const FVector3d ApproachGoal = Goal;
			int32 StartNode;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_StartProjection);
				StartNode = NearestNode(Nav, Enemy.SurfaceHandle, Start, Overlay.Get(), State.StartHintNode);
				State.StartHintNode = StartNode;
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
					GoalNode = NearestNode(Nav, GoalSurface, Goal, Overlay.Get(), State.GoalHintNode);
					GoalProjections.Add(Key, GoalNode);
				}
				State.GoalHintNode = GoalNode;
			}
			if (GoalNode == INDEX_NONE && GoalPlayerNav != nullptr && GoalPlayerNav->bWalkAnchored && !bUnreachableTarget)
			{
				// Nav 밖 플레이어: 실제로 걸어서 플레이어 근처에 닿는다고 검증된 anchor node까지 경로를 낸다(D-085).
				GoalNode = GoalPlayerNav->Node;
				Goal = FVector3d(GoalPlayerNav->WalkAnchorPoint);
			}
			// 투영 반경 밖 시작점은 걸어서 돌아갈 수 있다고 검증된 anchor를 시작 node로 쓴다. 없으면 기존처럼 제자리다.
			bool bOffNavStart = false;
			bool bRetreatToOpen = false;
			if (StartNode == INDEX_NONE)
			{
				StartNode = ResolveWalkAnchor(State, Center, Enemy.SurfaceHandle);
				bOffNavStart = StartNode != INDEX_NONE;
			}
			else if (LNPNavGraph::IsInNavPocket(Nav, StartNode, Start, Overlay.Get()))
			{
				// Nav 주머니(D-090): 발이 불완전 삼각형에 있고 최근접 node가 그 삼각형 외접원보다 멀면, 투영 반경 안이어도 그 node로
				// 곧게 걸을 수 있다는 보장이 없다(벽 모서리·바위 사이). Nav 밖과 같이 실제 보행으로 검증한 anchor로 먼저 돌아간다.
				// 경로 선분 위에서는 최근접 node가 외접원 안이라 이 분기에 들어오지 않는다(100cm 기준이 만든 진동을 막는다).
				const int32 Anchor = ResolveWalkAnchor(State, Center, Enemy.SurfaceHandle);
				if (Anchor != INDEX_NONE)
				{
					StartNode = Anchor;
					bOffNavStart = true;
				}
				else
				{
					// 곧게 걸어 닿는 node가 없으면(꺾인 주머니) 마지막으로 트인 삼각형에 있던 자리로 되돌아간다. 걸어 들어온 쪽이다.
					// 기록이 없거나 다른 snapshot·slot·Layer이거나 너무 멀면 이전처럼 최근접 node에서 시작한다.
					const int32 OpenSlot = Nav.Graph.GetSlot(State.LastOpenNode);
					const int32 StartSlot = Nav.Graph.GetSlot(StartNode);
					const double OpenDistanceSquared = FVector3d::DistSquared(Start, FVector3d(State.LastOpenPoint));
					bRetreatToOpen = State.LastOpenGeneration == Nav.SnapshotGeneration && OpenSlot != INDEX_NONE && OpenSlot == StartSlot
						&& Nav.Graph.GetNode(OpenSlot, State.LastOpenNode).LayerOrdinal == Nav.Graph.GetNode(StartSlot, StartNode).LayerOrdinal
						&& OpenDistanceSquared <= FMath::Square(LNPEnemyNavigation::WalkAnchorSearchRadius)
						&& OpenDistanceSquared > FMath::Square(LastOpenArrival);
				}
			}
			else
			{
				State.WalkAnchorNode = INDEX_NONE;
				State.WalkAnchorCheckTime = -DBL_MAX;
				State.FinalApproachCheckTime = -DBL_MAX;
				if (LNPNavGraph::IsSegmentWalkable(Nav, StartNode, Start, StartNode, Start, Overlay.Get()))
				{
					State.LastOpenPoint = FVector(Start);
					State.LastOpenGeneration = Nav.SnapshotGeneration;
					State.LastOpenNode = StartNode;
				}
			}
			if (StartNode == INDEX_NONE)
			{
				State.SteeringPoint = Center;
				State.bHasSteeringPoint = true;
				continue;
			}
			const FVector3d PathStart = bOffNavStart ? NodePoint(StartNode) : Start;
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
				// 실제로 곧게 걸을 선분(발 또는 Nav 밖이면 anchor node → 실제로 다가갈 MoveTarget 발)이 트인 삼각형만 지나야 한다(D-090).
				// node 사이 격자 직선은 실제 선분에서 수 m 어긋나 바위를 지날 수 있다. 끝점이 불완전 삼각형이면 경로로 목표 node까지 간다.
				bDirectGoal = GoalNode != INDEX_NONE
					&& LNPNavGraph::IsSegmentWalkable(Nav, StartNode, PathStart, GoalNode, ApproachGoal, Overlay.Get());
			}
			if (bDirectGoal && bOffNavStart && !bSynthetic)
			{
				// Nav 밖에서 목표로 곧게 가려면 현재 위치에서 MoveTarget 발까지의 실제 이동을 검증한다. 막히면 anchor로 먼저 돌아간다.
				if (Now - State.FinalApproachCheckTime >= LNPEnemyNavigation::WalkAnchorRetrySeconds)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_FinalApproach);
					State.bFinalApproachWalkable = Collision && LNPEnemyNavigation::WalkReaches(*Snapshot, *Collision, WalkParams,
						FVector(Center), Enemy.SurfaceHandle, FVector(ApproachGoal), FinalApproachAccept);
					State.FinalApproachCheckTime = Now;
				}
				bDirectGoal = State.bFinalApproachWalkable;
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
					// cache 경로는 앞쪽 waypoint에서 합류한다. 그 앞 waypoint로는 곧게 걸을 수 없을 수 있다.
					State.WaypointIndex = Result.FirstWaypoint;
				}
			}
			if (!State.Path.IsValid() && (State.RequestSerial == 0 || bGoalChanged || bPathInvalid || bNoPathOverlayChanged
				|| State.Status == ELNPNavPathStatus::Stale)
				&& Now - State.LastRequestTime >= MinRepathSeconds)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_Submit);
				// 진단 전용 사유 비트. 위 재요청 조건을 그대로 옮기며 동작에는 쓰지 않는다(Phase 10 단위 1).
				// 직선 보행 분기만 Status를 None으로 되돌리므로, 버린 요청의 원인을 그것으로 가른다.
				namespace ER = ELNPNavRequestReason;
				const uint32 Reasons = (State.RequestSerial == 0
						? (State.LastRequestTime == -DBL_MAX ? ER::NoPrior
							: State.Status == ELNPNavPathStatus::None ? ER::AfterDirect : ER::AfterCancel) : 0u)
					| (State.RequestedTarget != Target.TargetPlayer ? ER::TargetChanged : 0u)
					| (State.GoalTile != GoalTile ? ER::GoalTileChanged : 0u)
					| (State.Status != ELNPNavPathStatus::Unreachable && FVector3d::Dist(State.RequestedGoal, Goal) > RepathGoalDrift
						? ER::GoalDrift : 0u)
					| (bPathInvalid ? ER::PathInvalid : 0u)
					| (bNoPathOverlayChanged ? ER::NoPathOverlayChanged : 0u)
					| (State.Status == ELNPNavPathStatus::Stale ? ER::StaleResult : 0u);
				FLNPNavPathRequest Request;
				Request.Owner = Owner;
				Request.Priority = bChasing ? ELNPNavPathPriority::Chase : ELNPNavPathPriority::Background;
				Request.StartPosition = PathStart;
				Request.StartSurface = Enemy.SurfaceHandle;
				Request.GoalPosition = Goal;
				Request.GoalSurface = GoalSurface;
				Request.ApproachRadius = bTargetPath ? 3000.0 : 0.0;
				State.RequestSerial = Paths->Submit(Request);
				Paths->RecordRequestReason(Owner, State.RequestSerial, Reasons, LNPNavGraph::GetTileKey(Nav, StartNode),
					GoalNode, GoalTile);
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
				// 건너뛰기도 실제 선분(현재 발 → 다음 waypoint 지면점)이 트인 삼각형만 지날 때다(D-090).
				while (Waypoints.IsValidIndex(State.WaypointIndex + 1)
					&& LNPNavGraph::IsSegmentWalkable(Nav, StartNode, PathStart, Waypoints[State.WaypointIndex + 1].Node,
						Waypoints[State.WaypointIndex + 1].Location, Overlay.Get()))
				{
					++State.WaypointIndex;
				}
				// 격자 직선 검사는 Nav 구멍을 가로지르지 못한다. 멈춘 자리의 다음 waypoint를 실제 보행으로 검증해 건너뛴다(D-088).
				if (Collision && Waypoints.IsValidIndex(State.WaypointIndex + 1)
					&& Now - State.ShortcutCheckTime >= LNPEnemyNavigation::WalkAnchorRetrySeconds)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_Shortcut);
					for (int32 Skip = 0; Skip < LNPEnemyNavigation::PathShortcutMaxSkips && Waypoints.IsValidIndex(State.WaypointIndex + 1); ++Skip)
					{
						const int32 Candidate = State.WaypointIndex + 1;
						if (State.ShortcutFailedSerial == State.RequestSerial && State.ShortcutFailedWaypoint == Candidate)
						{
							break;
						}
						const LNPEnemyNavigation::EPathShortcut Shortcut = LNPEnemyNavigation::CheckPathShortcut(*Snapshot, *Collision,
							WalkParams, FVector(Center), Enemy.SurfaceHandle, FVector(Waypoints[State.WaypointIndex].Location),
							FVector(Waypoints[Candidate].Location));
						if (Shortcut != LNPEnemyNavigation::EPathShortcut::Walkable && Shortcut != LNPEnemyNavigation::EPathShortcut::Blocked)
						{
							break;
						}
						State.ShortcutCheckTime = Now;
						Paths->RecordPathShortcutWalk(Shortcut == LNPEnemyNavigation::EPathShortcut::Walkable);
						if (Shortcut == LNPEnemyNavigation::EPathShortcut::Blocked)
						{
							State.ShortcutFailedSerial = State.RequestSerial;
							State.ShortcutFailedWaypoint = Candidate;
							break;
						}
						State.WaypointIndex = Candidate;
						State.ShortcutSerial = State.RequestSerial;
						State.ShortcutWaypoint = Candidate;
					}
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
				else if (!bSynthetic && GoalPlayerNav && GoalPlayerNav->bWalkAnchored && !Waypoints.IsEmpty())
				{
					// 보행 anchor(D-085)가 검증한 것은 anchor node → 플레이어 발 구간이다. 근접 슬롯 위치(MoveTarget)는 그 길 밖
					// (바위 너머)일 수 있으므로 경로를 다 쓰면 검증된 목표인 플레이어 발로 조향한다. 정지는 근접 정지 거리다(D-091).
					State.SteeringPoint = GoalPlayerNav->GroundPoint;
					State.bHasSteeringPoint = true;
				}
				else if (!bSynthetic && Collision && !Waypoints.IsEmpty() && !(GoalPlayerNav && GoalPlayerNav->bWalkAnchored))
				{
					// 경로를 다 쓰면 이동은 MoveTarget으로 곧장 간다. 여기 왔다면 직선 판정은 거부됐다(끝 삼각형이 불완전).
					// 그 마지막 구간을 실제로 걸어서 닿는지 검증하고, 막히면(바위 뒤 주머니) 목표 node에서 기다린다(D-090).
					// Nav 밖 플레이어는 anchor 검증(D-085)이 이미 이 구간을 걸어 봤다.
					if (Now - State.EndApproachCheckTime >= LNPEnemyNavigation::WalkAnchorRetrySeconds)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_EndApproach);
						State.bEndApproachWalkable = LNPEnemyNavigation::WalkReaches(*Snapshot, *Collision, WalkParams,
							FVector(Center), Enemy.SurfaceHandle, FVector(ApproachGoal), FinalApproachAccept);
						State.EndApproachCheckTime = Now;
					}
					if (!State.bEndApproachWalkable)
					{
						State.SteeringPoint = Center;
						State.bHasSteeringPoint = true;
					}
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
			// 걸음으로 검증한 지름길 waypoint를 따라가는 중이면 Nav 구멍 한가운데서 투영을 잃어도 anchor로 되돌아가지 않는다(D-088).
			const bool bFollowingShortcut = State.Path.IsValid() && State.ShortcutSerial == State.RequestSerial
				&& State.ShortcutWaypoint == State.WaypointIndex && State.Path->Waypoints.IsValidIndex(State.WaypointIndex);
			if (bOffNavStart && !bFollowingShortcut)
			{
				// 경로는 anchor에서 시작한다. 검증된 보행 구간인 anchor까지 먼저 걸어가 Nav 시작점을 되찾는다.
				State.SteeringPoint = FVector(PathStart);
				State.bHasSteeringPoint = true;
			}
			if (bRetreatToOpen)
			{
				// 주머니에서 곧게 닿는 node가 없다. 마지막으로 트인 자리로 걸어 나간 뒤 그곳에서 경로를 다시 따른다(D-090).
				State.SteeringPoint = State.LastOpenPoint;
				State.bHasSteeringPoint = true;
			}
		}
	});
	if (!HomesChanged.IsEmpty())
	{
		Context.GetMutableSubsystemChecked<UMassSignalSubsystem>().SignalEntities(UE::Mass::Signals::StateTreeActivate, HomesChanged);
	}
}
