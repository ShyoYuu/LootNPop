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
	/** 머리 node에서도 정체해 점열을 놓은 뒤 걸어서 닿는 node부터 경로를 다시 내는 시간(초, D-092). */
	constexpr double TailRecoverSeconds = 3.0;

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
	: PlayerNavQuery(*this), EnemyConfigQuery(*this)
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
	EnemyConfigQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	EnemyConfigQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyConfigQuery.RegisterWithProcessor(*this);
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
	// 발자취 구간은 지상 적 중 가장 큰 캡슐로 걷는다(D-092). 그 캡슐이 지나가면 작은 적도 지나간다.
	LNPEnemyExactMovement::FParams TrailParams = AnchorParams;
	EnemyConfigQuery.ForEachEntityChunk(Context, [&TrailParams](FMassExecutionContext& Ctx)
	{
		const ULNPEnemyConfig* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config;
		if (Config && !Config->IsFlying() && !Config->IsCrawling())
		{
			TrailParams.GravityOrigin = Config->MovementConfig.GravityOrigin;
			TrailParams.CapsuleRadius = FMath::Max(TrailParams.CapsuleRadius, Config->CapsuleRadius);
			TrailParams.CapsuleHalfHeight = FMath::Max(TrailParams.CapsuleHalfHeight, Config->CapsuleHalfHeight);
		}
	});
	PlayerNavQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FMassActorFragment> Actors = Ctx.GetFragmentView<FMassActorFragment>();
		const TArrayView<FLNPPlayerNavFragment> NavFragments = Ctx.GetMutableFragmentView<FLNPPlayerNavFragment>();
		for (int32 Index = 0; Index < Ctx.GetNumEntities(); ++Index)
		{
			FLNPPlayerNavFragment Value = NavFragments.IsValidIndex(Index) ? NavFragments[Index] : FLNPPlayerNavFragment();
			const ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(Actors[Index].Get());
			FHitResult FloorHit;
			const bool bWasGrounded = Value.bGrounded;
			const bool bWasOnBlocker = Value.bBorrowedSurface;
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
				Value.bTrailAnchored = false;
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
					// 발자취(D-092): Nav 위·트인 삼각형의 자리를 기억했다가, 벗어나면 걸어 들어간 길을 적 캡슐 걸음으로 검증해 남긴다.
					const bool bOnNav = Node != INDEX_NONE && !Value.bInNavPocket;
					const int32 OpenNode = bOnNav && LNPNavGraph::IsSegmentWalkable(Snapshot->Nav, Node, Feet, Node, Feet, nullptr)
						? Node : INDEX_NONE;
					// 바위(빌린 층) 위는 적이 오르지 못하므로 공중처럼 점을 남기지도 끊지도 않는다. 내려와 지지면에 서면
					// 착지와 같이 직전 점 → 그 자리 구간을 검증한다. 적은 마지막 점 뒤 바위 위 플레이어 발로 조향한다.
					if (bOnNav || !Value.bBorrowedSurface)
					{
						LNPEnemyNavigation::UpdatePlayerTrail(*Snapshot, *Collision, TrailParams, FVector(Feet), Surface, bOnNav, OpenNode,
							!bWasGrounded || bWasOnBlocker, Value.Trail);
					}
					if (!bOnNav && Value.Trail.IsValid())
					{
						// Nav 소속 우선순위: 투영 → 발자취 → 보행 anchor. 발자취는 꺾인 길도 담으므로 anchor의 직선 하나보다 앞선다.
						Node = Value.Trail.EntryNode;
						Value.bWalkAnchored = true;
						Value.bTrailAnchored = true;
						Value.WalkAnchorPoint = Value.Trail.Points[0];
					}
					else if (!bOnNav)
					{
						// 투영 반경 밖: node에 선 적이 실제로 걸어서 플레이어 근처에 닿는 node만 anchor로 쓴다. 벽 너머는 실패한다.
						const bool bCached = Value.WalkAnchorGeneration == Snapshot->Nav.SnapshotGeneration
							&& Now - Value.WalkAnchorCheckTime < LNPEnemyNavigation::WalkAnchorRetrySeconds
							&& FVector::DistSquared(Value.WalkAnchorCheckFeet, Feet) <= FMath::Square(PlayerWalkAnchorMoveTolerance);
						if (!bCached)
						{
							// 발자취와 같은 가장 큰 지상 캡슐로 걷고, 걸은 궤적을 적이 따라갈 점열로 남긴다(D-092).
							// anchor node → 발 한 직선만 주면 적이 node 근처 다른 자리에서 출발해 미끄러지며 선을 벗어났다(PIE).
							const FVector Up = (TrailParams.GravityOrigin - FVector(Feet)).GetSafeNormal();
							TArray<FVector> WalkedPath;
							Value.WalkAnchorNode = LNPEnemyNavigation::FindWalkAnchor(*Snapshot, *Collision, TrailParams,
								FVector(Feet) + Up * TrailParams.CapsuleHalfHeight, Surface, nullptr, true, PlayerWalkAnchorReach, &WalkedPath);
							Value.WalkAnchorGeneration = Snapshot->Nav.SnapshotGeneration;
							Value.WalkAnchorCheckTime = Now;
							Value.WalkAnchorCheckFeet = Feet;
							FVector Path[FLNPPlayerNavFragment::MaxAnchorPath];
							const int32 PathCount = Value.WalkAnchorNode == INDEX_NONE ? 0 : LNPEnemyNavigation::BuildAnchorPath(
								FVector(Snapshot->Nav.Graph.GetWorldPoint(Snapshot->Nav.Graph.GetSlot(Value.WalkAnchorNode), Value.WalkAnchorNode)),
								WalkedPath, Path);
							bool bChanged = PathCount != Value.AnchorPathCount;
							for (int32 Point = 0; Point < PathCount && !bChanged; ++Point)
							{
								bChanged = !Path[Point].Equals(Value.AnchorPath[Point], 1.0);
							}
							if (bChanged)
							{
								FMemory::Memcpy(Value.AnchorPath, Path, sizeof(FVector) * PathCount);
								Value.AnchorPathCount = PathCount;
								++Value.AnchorRevision;
							}
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
				// 공격 거리 안에 서 있는 동안은 점열 정체 시계를 멈춘다. 다시 움직일 때 곧바로 물러나지 않게 한다.
				State.TailBestDistance = DBL_MAX;
				State.TailProgressTime = Now;
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
			// 걸음 검증된 점열(발자취·보행 anchor)로 이어진 플레이어(D-092): 머리 node까지 경로로 간 뒤 점열을 따라간다.
			// 따라가는 동안은 Nav 밖이어도 자기 anchor·트인 자리로 되돌리지 않는다(D-088 지름길과 같은 규칙).
			const bool bTailTarget = !bSynthetic && bChasing && GoalPlayerNav && GoalPlayerNav->bWalkAnchored && !bUnreachableTarget;
			if (State.TailIndex != INDEX_NONE)
			{
				if (!bTailTarget || State.TailTarget != Target.TargetPlayer)
				{
					State.TailIndex = INDEX_NONE;
					State.TailBestDistance = DBL_MAX;
					State.TailProgressTime = Now;
				}
				else if (State.TailHeadNode != GoalPlayerNav->Node || State.TailRevision != GoalPlayerNav->GetTailRevision()
					|| State.bTailTrail != GoalPlayerNav->bTrailAnchored)
				{
					// 머리·개정이 바뀌었다(고리 제거·anchor↔발자취). 가까운 점이 있으면 거기서 잇고, 없으면 경로로 머리부터 다시 간다.
					State.TailIndex = LNPEnemyNavigation::SyncTail(GoalPlayerNav->GetTailPoints(), FVector(Start),
						(WalkParams.GravityOrigin - FVector(Center)).GetSafeNormal());
					State.TailHeadNode = GoalPlayerNav->Node;
					State.TailRevision = GoalPlayerNav->GetTailRevision();
					State.bTailTrail = GoalPlayerNav->bTrailAnchored;
					State.TailBestDistance = DBL_MAX;
					State.TailProgressTime = Now;
				}
			}
			if (State.TailIndex != INDEX_NONE)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LNPEnemyPath_FollowTail);
				FVector Steer;
				if (LNPEnemyNavigation::FollowTail(GoalPlayerNav->GetTailPoints(), GoalPlayerNav->GroundPoint, FVector(Start),
					(WalkParams.GravityOrigin - FVector(Center)).GetSafeNormal(), Now, State.TailIndex,
					State.TailBestDistance, State.TailProgressTime, Steer))
				{
					// 들어가는 길을 적 자신도 남긴다. 플레이어가 먼저 나가 점열을 잃으면 이 점을 거꾸로 되짚어 나온다.
					if (State.CrumbCount < FLNPEnemyPathFragment::MaxCrumbs && (State.CrumbCount == 0
						|| FVector3d::DistSquared(FVector3d(State.Crumbs[State.CrumbCount - 1]), Start)
							>= FMath::Square(LNPEnemyNavigation::CrumbSpacing)))
					{
						State.Crumbs[State.CrumbCount++] = FVector3f(Start);
					}
					State.SteeringPoint = Steer;
					State.bHasSteeringPoint = true;
					Paths->RecordFollowerFrame(false, 0);
					continue;
				}
				// 머리 node로도 곧게 돌아가지 못한다. 점열을 놓고 걸어서 닿는 node(D-085)부터 경로로 머리까지 다시 간다.
				State.TailIndex = INDEX_NONE;
				State.TailBestDistance = DBL_MAX;
				State.TailProgressTime = Now;
				State.TailRecoverUntil = Now + TailRecoverSeconds;
				Paths->Cancel(Owner);
				State.Path.Reset();
				State.RequestSerial = 0;
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
			bool bRetreatCrumb = false;
			// 적 자신의 발자취(D-092)를 최근 점부터 되짚는다. 점 TailArrival 안이면 그 점을 지우고 다음(더 바깥) 점으로 간다.
			// 들어올 때 벽을 따라 미끄러진 길은 거꾸로 곧게 걷지 못할 수 있다(PIE). 1초 동안 가까워지지 못하면 그 점을 건너뛴다.
			// 점열 추종이 끝난 뒤라 정체 시계는 TailBestDistance·TailProgressTime을 함께 쓴다.
			auto RetreatAlongCrumbs = [&State, &Start, &WalkParams, &Center, Now]()
			{
				const FVector Up = (WalkParams.GravityOrigin - FVector(Center)).GetSafeNormal();
				auto Planar = [&](const int32 At) { return FVector::VectorPlaneProject(FVector(State.Crumbs[At]) - FVector(Start), Up).Size(); };
				while (State.CrumbCount > 0 && Planar(State.CrumbCount - 1) <= LNPEnemyNavigation::TailArrival)
				{
					--State.CrumbCount;
					State.TailBestDistance = DBL_MAX;
					State.TailProgressTime = Now;
				}
				if (State.CrumbCount == 0)
				{
					return false;
				}
				const double Distance = Planar(State.CrumbCount - 1);
				if (Distance < State.TailBestDistance - LNPEnemyNavigation::TailStallProgress)
				{
					State.TailBestDistance = Distance;
					State.TailProgressTime = Now;
				}
				else if (Now - State.TailProgressTime >= LNPEnemyNavigation::TailStallSeconds)
				{
					--State.CrumbCount;
					State.TailBestDistance = DBL_MAX;
					State.TailProgressTime = Now;
					if (State.CrumbCount == 0)
					{
						return false;
					}
				}
				State.SteeringPoint = FVector(State.Crumbs[State.CrumbCount - 1]);
				State.bHasSteeringPoint = true;
				return true;
			};
			// 적 자기 발자취(D-092)는 점열 추종 중에만 남겼다. 분리력에 밀려 주머니로 들어간 적은 발자취가 없어 꺾인 길 너머의 트인 자리로
			// 곧게 가다 영구히 섰다(단위6 PIE). 발이 트인 삼각형 밖이면 점열과 무관하게 남긴다. 되짚어 나가는 중에는 남기지 않는다
			// (방금 지운 점 근처에 다시 남겨 제자리에서 돌지 않게, 호출하는 쪽이 되짚기 전 분기에서만 부른다).
			auto RecordCrumb = [&State, &Start]()
			{
				if (State.CrumbCount < FLNPEnemyPathFragment::MaxCrumbs && (State.CrumbCount == 0
					|| FVector3d::DistSquared(FVector3d(State.Crumbs[State.CrumbCount - 1]), Start)
						>= FMath::Square(LNPEnemyNavigation::CrumbSpacing)))
				{
					State.Crumbs[State.CrumbCount++] = FVector3f(Start);
				}
			};
			// 곧게 걸어 닿는 node가 없으면(꺾인 주머니) 마지막으로 트인 삼각형에 있던 자리로 되돌아간다. 걸어 들어온 쪽이다.
			// 기록이 없거나 다른 snapshot·slot·Layer이거나 너무 멀면 이전처럼 최근접 node에서 시작한다.
			auto CanRetreatToOpen = [&]()
			{
				const int32 OpenSlot = Nav.Graph.GetSlot(State.LastOpenNode);
				const int32 StartSlot = Nav.Graph.GetSlot(StartNode);
				const double OpenDistanceSquared = FVector3d::DistSquared(Start, FVector3d(State.LastOpenPoint));
				return State.LastOpenGeneration == Nav.SnapshotGeneration && OpenSlot != INDEX_NONE && OpenSlot == StartSlot
					&& Nav.Graph.GetNode(OpenSlot, State.LastOpenNode).LayerOrdinal == Nav.Graph.GetNode(StartSlot, StartNode).LayerOrdinal
					&& OpenDistanceSquared <= FMath::Square(LNPEnemyNavigation::WalkAnchorSearchRadius)
					&& OpenDistanceSquared > FMath::Square(LastOpenArrival);
			};
			const bool bTailRecover = Now < State.TailRecoverUntil;
			if (StartNode == INDEX_NONE)
			{
				StartNode = ResolveWalkAnchor(State, Center, Enemy.SurfaceHandle);
				bOffNavStart = StartNode != INDEX_NONE;
				if (!bOffNavStart && RetreatAlongCrumbs())
				{
					// 걸어서 닿는 node가 없는 Nav 밖이다. 들어온 길로 되돌아 나간다.
					continue;
				}
			}
			else if (bTailRecover && !LNPNavGraph::IsInNavPocket(Nav, StartNode, Start, Overlay.Get()))
			{
				// 점열을 놓쳤거나 waypoint 추종이 정체한 직후(Nav 위): 최근접 node가 아니라 실제로 걸어서 닿는 node부터 경로를 낸다.
				// 그 node 지면점 TailArrival 안에 닿으면 회복을 끝낸다. waypoint 도착 반경(100cm)이면 node 99cm 옆 턱에 낀 적이
				// 움직이지 않고 회복을 끝내 같은 자리로 다시 조향했다(단위6 PIE).
				const int32 Anchor = ResolveWalkAnchor(State, Center, Enemy.SurfaceHandle);
				if (Anchor != INDEX_NONE)
				{
					StartNode = Anchor;
					bOffNavStart = FVector3d::DistSquared(Start, NodePoint(Anchor)) > FMath::Square(LNPEnemyNavigation::TailArrival);
					if (!bOffNavStart)
					{
						State.TailRecoverUntil = -DBL_MAX;
					}
				}
				else
				{
					// 걸어서 닿는 node가 없다. 주머니와 같이 자기 발자취, 없으면 마지막으로 트인 자리로 되돌아간다.
					bRetreatCrumb = State.CrumbCount > 0;
					bRetreatToOpen = CanRetreatToOpen();
				}
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
					RecordCrumb();
				}
				else
				{
					// 걸어 들어오거나 밀려 들어온 주머니면 자기 발자취를 거꾸로 되짚어 나간다. 트인 자리 한 점은 꺾인 길에서 곧게 닿지 않는다(PIE).
					// 발자취가 없거나 다 건너뛰었으면 아래 트인 자리 규칙으로 간다.
					bRetreatCrumb = State.CrumbCount > 0;
					bRetreatToOpen = CanRetreatToOpen();
				}
			}
			else
			{
				State.WalkAnchorNode = INDEX_NONE;
				State.WalkAnchorCheckTime = -DBL_MAX;
				State.FinalApproachCheckTime = -DBL_MAX;
				if (LNPNavGraph::IsSegmentWalkable(Nav, StartNode, Start, StartNode, Start, Overlay.Get()))
				{
					State.CrumbCount = 0;
					State.LastOpenPoint = FVector(Start);
					State.LastOpenGeneration = Nav.SnapshotGeneration;
					State.LastOpenNode = StartNode;
				}
				else
				{
					RecordCrumb();
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
			bool bStartedTail = false;
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
				// 정체 감지(단위6): 걸으려는데 1초 동안 조향 waypoint로 10cm도 가까워지지 못하면, 경로 선을 벗어나 턱·바위에 끼었다
				// (Ridge 재현: node 옆 불완전 삼각형에서 첫 걸음부터 Rejected). 경로를 놓고 걸어서 닿는 node부터 다시 간다.
				// 이동 프로세서가 서 있기로 한 프레임(공격·경직·정지 거리, LastMoveIntentSpeed 0)과 조향점이 waypoint가 아닌 프레임
				// (anchor·발자취·트인 자리로 되돌아가는 중)은 재지 않는다. MoveTarget.DesiredSpeed는 Confirmed 추격 중에도 0이라 쓰지 않는다.
				// Key는 waypoint 번호, 마지막 접근 대기 중 목표 node로 돌아가는 동안은 Waypoints.Num()이다.
				bool bWaypointStalled = false;
				auto TrackStall = [&](const int32 Key, const FVector3d& Target)
				{
					const double Distance = FVector3d::Dist(Start, Target);
					if (Enemy.LastMoveIntentSpeed <= 0.f || bOffNavStart || bRetreatCrumb || bRetreatToOpen
						|| State.StallSerial != State.RequestSerial || State.StallWaypoint != Key
						|| Distance < State.StallBestDistance - LNPEnemyNavigation::TailStallProgress)
					{
						State.StallSerial = State.RequestSerial;
						State.StallWaypoint = Key;
						State.StallBestDistance = Distance;
						State.StallProgressTime = Now;
					}
					else if (Now - State.StallProgressTime >= LNPEnemyNavigation::TailStallSeconds)
					{
						bWaypointStalled = true;
					}
				};
				if (Waypoints.IsValidIndex(State.WaypointIndex))
				{
					State.SteeringPoint = Waypoints[State.WaypointIndex].Location;
					State.bHasSteeringPoint = true;
					TrackStall(State.WaypointIndex, Waypoints[State.WaypointIndex].Location);
				}
				else if (State.bApproachingUnreachable && !Waypoints.IsEmpty())
				{
					// 접근점에 도착한 뒤 의미상 목표 방향으로 다시 걷지 않는다.
					State.SteeringPoint = Waypoints.Last().Location;
					State.bHasSteeringPoint = true;
				}
				else if (bTailTarget && !Waypoints.IsEmpty())
				{
					// 보행 anchor(D-085)·발자취(D-092)가 검증한 것은 머리 node → 점열 → 플레이어 발이다. 근접 슬롯 위치(MoveTarget)는
					// 그 길 밖(바위 너머)일 수 있고, 지금 자리에서 발로 곧장 가는 직선도 검증된 길이 아니다(PIE: 옆 node에서 첫 걸음부터 막힘).
					// 경로를 다 쓰면 머리 node부터 점열을 따라간다. 정지는 근접 정지 거리다.
					State.TailTarget = Target.TargetPlayer;
					State.TailHeadNode = GoalPlayerNav->Node;
					State.TailRevision = GoalPlayerNav->GetTailRevision();
					State.bTailTrail = GoalPlayerNav->bTrailAnchored;
					State.TailIndex = FMath::Max(0, LNPEnemyNavigation::SyncTail(GoalPlayerNav->GetTailPoints(), FVector(Start),
						(WalkParams.GravityOrigin - FVector(Center)).GetSafeNormal()));
					State.TailBestDistance = DBL_MAX;
					State.TailProgressTime = Now;
					State.TailRecoverUntil = -DBL_MAX;
					LNPEnemyNavigation::FollowTail(GoalPlayerNav->GetTailPoints(), GoalPlayerNav->GroundPoint,
						FVector(Start), (WalkParams.GravityOrigin - FVector(Center)).GetSafeNormal(), Now, State.TailIndex,
						State.TailBestDistance, State.TailProgressTime, State.SteeringPoint);
					State.bHasSteeringPoint = true;
					bStartedTail = true;
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
						// 목표 node(마지막 waypoint)에서 기다린다. 도착 판정(100cm) 뒤 분리력·경사로 밀려나 node 밖 경사에 선 채 기다리면
						// 슬롯 쪽으로 다시 다가갈 길이 없다(단위6 PIE: node에서 130cm 떨어진 턱 아래에서 30초 정지). 50cm 밖이면 node로
						// 돌아가고, 그 복귀도 정체 감지에 넣어 못 돌아가면 걸어서 닿는 node부터 회복한다.
						const FVector3d GoalNodePoint = Waypoints.Last().Location;
						const FVector Up = (WalkParams.GravityOrigin - FVector(Center)).GetSafeNormal();
						if (FVector::VectorPlaneProject(FVector(GoalNodePoint - Start), Up).SizeSquared()
							> FMath::Square(LNPEnemyNavigation::TailArrival))
						{
							State.SteeringPoint = FVector(GoalNodePoint);
							TrackStall(Waypoints.Num(), GoalNodePoint);
						}
						else
						{
							State.SteeringPoint = Center;
						}
						State.bHasSteeringPoint = true;
					}
				}
				Paths->RecordFollowerFrame(true, State.WaypointIndex - PreviousWaypoint);
				if (bWaypointStalled)
				{
					Paths->RecordWaypointStall();
					State.StallWaypoint = INDEX_NONE;
					State.TailRecoverUntil = Now + TailRecoverSeconds;
					State.WalkAnchorCheckTime = -DBL_MAX;
					Paths->Cancel(Owner);
					State.Path.Reset();
					State.RequestSerial = 0;
					State.SteeringPoint = Center;
					State.bHasSteeringPoint = true;
					continue;
				}
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
			if (bOffNavStart && !bFollowingShortcut && !bStartedTail)
			{
				// 경로는 anchor에서 시작한다. 검증된 보행 구간인 anchor까지 먼저 걸어가 Nav 시작점을 되찾는다.
				State.SteeringPoint = FVector(PathStart);
				State.bHasSteeringPoint = true;
			}
			if (bRetreatCrumb && !bStartedTail && RetreatAlongCrumbs())
			{
				// 자기 발자취를 되짚는 중이다(조향점은 RetreatAlongCrumbs가 정했다).
			}
			else if (bRetreatToOpen && !bStartedTail)
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
