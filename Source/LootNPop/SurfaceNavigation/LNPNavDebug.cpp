// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavQuery.h"
#include "SurfaceNavigation/LNPNavPathSubsystem.h"
#include "SurfaceNavigation/LNPNavGraph.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "Character/LNPCharacterBase.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyNavigation.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "MassCommonFragments.h"
#include "MassNavigationFragments.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassEntityQuery.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "LootNPop.h"

#if !UE_BUILD_SHIPPING
namespace
{
	/** 경로 추종 조향 진단(D-090): 조향점·현재 waypoint·MoveTarget까지의 거리와 시작 node 기준 실제 선분 판정. */
	FString DescribeSteering(const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay, const FVector& Center, const double HalfHeight,
		const FLNPEnemyPathFragment& Path, const FMassMoveTargetFragment& MoveTarget, const double Now)
	{
		const FVector3d Feet = FVector3d(Center) + FVector3d(Center).GetSafeNormal() * HalfHeight;
		const FVector3d GoalFeet = FVector3d(MoveTarget.Center) + FVector3d(MoveTarget.Center).GetSafeNormal() * HalfHeight;
		auto NodeText = [&Nav](const int32 Node)
		{
			const int32 Slot = Nav.Graph.GetSlot(Node);
			if (Slot == INDEX_NONE) { return FString(TEXT("none")); }
			const FLNPNavGraphNode& GraphNode = Nav.Graph.GetNode(Slot, Node);
			return FString::Printf(TEXT("%d(%u,%u)"), Node, GraphNode.I, GraphNode.J);
		};
		const int32 Start = Path.StartHintNode;
		const bool bStartValid = Nav.Graph.GetSlot(Start) != INDEX_NONE;
		FString Text = FString::Printf(TEXT(" | steerDist=%.1f moveTargetDist=%.1f distToGoal=%.1f startNode=%s startDist=%.1f startTri=%d pocket=%d directSeg=%d endWalk=%d lastOpenDist=%.1f approachUnreachable=%d tail=%d tailBest=%.1f tailStallAge=%.2f crumbs=%u recover=%.2f"),
			Path.bHasSteeringPoint ? FVector::Dist(Center, Path.SteeringPoint) : -1.0,
			FVector3d::Dist(Feet, GoalFeet), MoveTarget.DistanceToGoal, *NodeText(Start),
			bStartValid ? FVector3d::Dist(Feet, Nav.Graph.GetWorldPoint(Nav.Graph.GetSlot(Start), Start)) : -1.0,
			bStartValid && LNPNavGraph::IsSegmentWalkable(Nav, Start, Feet, Start, Feet, Overlay),
			bStartValid && LNPNavGraph::IsInNavPocket(Nav, Start, Feet, Overlay),
			bStartValid && Path.GoalHintNode != INDEX_NONE && LNPNavGraph::IsSegmentWalkable(Nav, Start, Feet, Path.GoalHintNode, GoalFeet, Overlay),
			Path.bEndApproachWalkable, Path.LastOpenNode != INDEX_NONE ? FVector3d::Dist(Feet, FVector3d(Path.LastOpenPoint)) : -1.0,
			Path.bApproachingUnreachable, Path.TailIndex, Path.TailBestDistance == DBL_MAX ? -1.0 : Path.TailBestDistance,
			Path.TailIndex != INDEX_NONE ? Now - Path.TailProgressTime : -1.0, Path.CrumbCount,
			FMath::Max(0.0, Path.TailRecoverUntil - Now));
		if (Path.Path.IsValid() && Path.Path->Waypoints.IsValidIndex(Path.WaypointIndex))
		{
			const FLNPNavPathWaypoint& Waypoint = Path.Path->Waypoints[Path.WaypointIndex];
			Text += FString::Printf(TEXT(" waypointNode=%s waypointDist=%.1f waypointSeg=%d steerIsWaypoint=%d"),
				*NodeText(Waypoint.Node), FVector3d::Dist(Feet, Waypoint.Location),
				bStartValid && LNPNavGraph::IsSegmentWalkable(Nav, Start, Feet, Waypoint.Node, Waypoint.Location, Overlay),
				Path.bHasSteeringPoint && FVector3d::Dist(FVector3d(Path.SteeringPoint), Waypoint.Location) < 1.0);
		}
		return Text;
	}

	const TCHAR* LexWalkEnd(const LNPEnemyNavigation::FWalkProbe::EEnd End)
	{
		using EEnd = LNPEnemyNavigation::FWalkProbe::EEnd;
		switch (End)
		{
		case EEnd::Reached: return TEXT("reached");
		case EEnd::NoSurface: return TEXT("noSurface");
		case EEnd::NotGrounded: return TEXT("notGrounded");
		case EEnd::LayerChanged: return TEXT("layerChanged");
		case EEnd::Stuck: return TEXT("stuck");
		case EEnd::MaxSteps: return TEXT("maxSteps");
		}
		return TEXT("?");
	}

	/** 근접 적이 보행 anchor 통과 뒤 공격 거리 밖에서 서는 원인 프로브(Phase16 단위5b 0단계). 실제 이동·상태는 바꾸지 않는다. */
	void LogWalkProbe(const FLNPSurfaceDataSnapshot& Snapshot, const ULNPMassWorldCollisionSubsystem& Collision,
		const TCHAR* Variant, const LNPEnemyExactMovement::FParams& Params, const FVector& StartFeet, const FLNPSurfaceHandle& Surface,
		const FVector& GoalFeet, const double Accept, const double Stride = 0.0)
	{
		const FVector Up = (Params.GravityOrigin - StartFeet).GetSafeNormal();
		LNPEnemyNavigation::FWalkProbe Probe;
		if (Stride > 0.0)
		{
			Probe.StepLength = Stride;
			Probe.StepSeconds = 1.f / 60.f;
		}
		const bool bReached = LNPEnemyNavigation::WalkReaches(Snapshot, Collision, Params, StartFeet + Up * Params.CapsuleHalfHeight,
			Surface, GoalFeet, Accept, &Probe);
		UE_LOG(LogLootNPop, Display,
			TEXT("[NavProbe] variant=%s radius=%.0f halfHeight=%.0f accept=%.0f stride=%.1f reached=%d end=%s ground=%d steps=%d best3D=%.1f bestPlanar=%.1f bestHeight=%.1f startFeet=%s endFeet=%s"),
			Variant, Params.CapsuleRadius, Params.CapsuleHalfHeight, Accept, Stride > 0.0 ? Stride : FMath::Max(Params.CapsuleRadius, 10.f),
			bReached, LexWalkEnd(Probe.End), static_cast<int32>(Probe.LastGround), Probe.Steps, Probe.Best3D, Probe.BestPlanar,
			Probe.BestHeight, *StartFeet.ToString(), *Probe.EndFeet.ToString());
	}

	/** 누적 경로 통계와 별도로 플레이어 가까운 NPC의 인지·접지·경로 상태를 비교한다. */
	void LogNearbyEnemies(UWorld& World, const FLNPSurfaceDataSnapshot& Snapshot, const FVector3d& Feet)
	{
		UMassEntitySubsystem* Mass = World.GetSubsystem<UMassEntitySubsystem>();
		if (!Mass) { return; }
		FMassEntityManager& Manager = Mass->GetMutableEntityManager();
		FMassExecutionContext Context(Manager, 0.0f);
		FLNPPlayerNavFragment PlayerNav;
		FVector PlayerCenter = FVector(Feet);
		double PlayerDistance = DBL_MAX;
		FMassEntityQuery PlayerQuery(Manager.AsShared());
		PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		PlayerQuery.AddRequirement<FLNPPlayerNavFragment>(EMassFragmentAccess::ReadOnly);
		PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
		{
			const auto Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const auto Nav = Ctx.GetFragmentView<FLNPPlayerNavFragment>();
			for (int32 I = 0; I < Ctx.GetNumEntities(); ++I)
			{
				const FVector Center = Transforms[I].GetTransform().GetLocation();
				const double Distance = FVector3d::DistSquared(Center, Feet);
				if (Distance < PlayerDistance) { PlayerDistance = Distance; PlayerNav = Nav[I]; PlayerCenter = Center; }
			}
		});
		UE_LOG(LogLootNPop, Display, TEXT("[NavEnemy] player grounded=%d slot=%u layer=%u node=%d group=%u walkAnchored=%d trailAnchored=%d trail=%d trailBroken=%d trailRevision=%u trailBreaks=%u lastBreak=%d lastBreakFeet=%s anchorPath=%d anchorRevision=%u borrowedSurface=%d pocket=%d feet=%s"),
			PlayerNav.bGrounded, PlayerNav.Surface.OctantSlot, PlayerNav.Surface.LocalLayerId, PlayerNav.Node,
			PlayerNav.Group, PlayerNav.bWalkAnchored, PlayerNav.bTrailAnchored, PlayerNav.Trail.Count, PlayerNav.Trail.bBroken,
			PlayerNav.Trail.Revision, PlayerNav.Trail.Breaks, static_cast<int32>(PlayerNav.Trail.LastBreak),
			*PlayerNav.Trail.LastBreakFeet.ToString(), PlayerNav.AnchorPathCount, PlayerNav.AnchorRevision, PlayerNav.bBorrowedSurface, PlayerNav.bInNavPocket, *PlayerNav.GroundPoint.ToString());
		const APlayerController* Controller = World.GetFirstPlayerController();
		const ALNPCharacterBase* Character = Controller ? Cast<ALNPCharacterBase>(Controller->GetPawn()) : nullptr;
		FHitResult FloorHit;
		if (Character && Character->GetMoverComponent()->TryGetFloorCheckHitResult(FloorHit))
		{
			const auto* Registry = World.GetSubsystem<ULNPHitIdentitySubsystem>();
			const auto Identity = Registry ? Registry->ResolveHit(FloorHit) : FLNPExactHitIdentity();
			UE_LOG(LogLootNPop, Display, TEXT("[NavEnemy] playerFloor component=%s face=%d blocking=%d slot=%u layer=%u"),
				*GetNameSafe(FloorHit.GetComponent()), FloorHit.FaceIndex, FloorHit.IsValidBlockingHit(), Identity.Slot, Identity.LocalLayerId);
		}
		const ULNPNavPathSubsystem* NavPaths = World.GetSubsystem<ULNPNavPathSubsystem>();
		const TSharedPtr<const FLNPNavOverlay, ESPMode::ThreadSafe> Overlay = NavPaths ? NavPaths->TakeOverlay() : nullptr;
		struct FNearby
		{
			double Distance;
			FString Line;
			const ULNPEnemyConfig* Config = nullptr;
			FVector Center = FVector::ZeroVector;
			FLNPSurfaceHandle Surface;
			FVector Push = FVector::ZeroVector;
			bool bHasSteeringPoint = false;
			FVector SteeringPoint = FVector::ZeroVector;
		};
		TArray<FNearby> Nearby;
		int32 Flyers = 0, NearbyFlyers = 0;
		FMassEntityQuery Query(Manager.AsShared());
		Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FLNPEnemyVelocityFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FLNPEnemyTargetingFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FLNPEnemyTargetingCandidateFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FLNPEnemyPathFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FLNPEntityAttackFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FLNPEnemySeparationFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddConstSharedRequirement<FLNPEnemySharedFragment>();
		Query.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
		Query.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
		Query.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
		{
			const auto Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const auto Enemies = Ctx.GetFragmentView<FLNPEnemyFragment>();
			const auto Velocities = Ctx.GetFragmentView<FLNPEnemyVelocityFragment>();
			const auto Targets = Ctx.GetFragmentView<FLNPEnemyTargetingFragment>();
			const auto Candidates = Ctx.GetFragmentView<FLNPEnemyTargetingCandidateFragment>();
			const auto Paths = Ctx.GetFragmentView<FLNPEnemyPathFragment>();
			const auto Attacks = Ctx.GetFragmentView<FLNPEntityAttackFragment>();
			const auto MoveTargets = Ctx.GetFragmentView<FMassMoveTargetFragment>();
			const auto Separations = Ctx.GetFragmentView<FLNPEnemySeparationFragment>();
			const ULNPEnemyConfig* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config;
			if (!Config) { return; }
			for (int32 I = 0; I < Ctx.GetNumEntities(); ++I)
			{
				const FTransform& Transform = Transforms[I].GetTransform();
				const FVector Center = Transform.GetLocation();
				const double Distance = FVector::Dist(Center, PlayerCenter);
				Flyers += Config->IsFlying() ? 1 : 0;
				NearbyFlyers += Config->IsFlying() && Distance < 5000.0 ? 1 : 0;
				if (Distance > 1800.0) { continue; }
				FLNPNavEndpointQuery Ends;
				Ends.StartPosition = Center + Center.GetSafeNormal() * Config->CapsuleHalfHeight;
				Ends.StartSurface = &Enemies[I].SurfaceHandle;
				Ends.GoalPosition = PlayerNav.GetNavQueryPoint();
				Ends.GoalSurface = &PlayerNav.Surface;
				Ends.ApproachRadius = 0.0;
				const auto Status = LNPNavGraph::ResolveEndpoints(Snapshot.Nav, Ends).Status;
				const double Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
					FVector::DotProduct(Transform.GetUnitAxis(EAxis::X), (PlayerCenter - Center).GetSafeNormal()), -1.0, 1.0)));
				const auto& Path = Paths[I];
				Nearby.Add({Distance, FString::Printf(TEXT("[NavEnemy] entity=%d config=%s pos=%s distance=%.1f angle=%.1f awareness=%.0f vision=%.0f flying=%d candidates=%u state=%d target=%d slot=%u layer=%u surfaceValid=%d airborneSpeed=%.1f endpoint=%d pathStatus=%d waypoints=%d waypoint=%d steering=%d walkAnchor=%d losBlocked=%d holdFire=%d attackPhase=%d"),
					Ctx.GetEntity(I).Index, *Config->GetName(), *Center.ToString(), Distance, Angle,
					Config->TargetingConfig.AwarenessDistance, Config->TargetingConfig.VisionDistance, Config->IsFlying(),
					Candidates[I].NumPotentialTargets, static_cast<int32>(Targets[I].State), Targets[I].TargetPlayer.Index,
					Enemies[I].SurfaceHandle.OctantSlot, Enemies[I].SurfaceHandle.LocalLayerId, Enemies[I].SurfaceHandle.IsValid(),
					Velocities[I].Velocity.Size(), static_cast<int32>(Status), static_cast<int32>(Path.Status),
					Path.Path.IsValid() ? Path.Path->Waypoints.Num() : 0, Path.WaypointIndex, Path.bHasSteeringPoint, Path.WalkAnchorNode,
					Attacks[I].bLineOfSightBlocked, Attacks[I].bHoldingGroundFirePosition, static_cast<int32>(Attacks[I].Phase))
					+ DescribeSteering(Snapshot.Nav, Overlay.Get(), Center, Config->CapsuleHalfHeight, Path, MoveTargets[I], World.GetTimeSeconds())
					+ FString::Printf(TEXT(" | playerToPod=%.0f chaseRadius=%.0f orphaned=%d"),
						FVector::Dist(PlayerCenter, Enemies[I].ParentPodLocation), Config->TargetingConfig.ChaseRadius, Enemies[I].bOrphaned),
					Config, Center, Enemies[I].SurfaceHandle, Separations[I].Push, Path.bHasSteeringPoint, Path.SteeringPoint});
			}
		});
		Nearby.Sort([](const FNearby& A, const FNearby& B) { return A.Distance < B.Distance; });
		for (int32 I = 0; I < FMath::Min(10, Nearby.Num()); ++I) { UE_LOG(LogLootNPop, Display, TEXT("%s"), *Nearby[I].Line); }

		// 보행 anchor 원인 프로브(단위5b 0단계): 플레이어 anchor 검증을 그대로 다시 걷고, 캡슐·출발점·보폭을 하나씩 바꿔 걷는다.
		const ULNPMassWorldCollisionSubsystem* Collision = World.GetSubsystem<ULNPMassWorldCollisionSubsystem>();
		constexpr int32 MaxGroundProbes = 3;
		const FNearby* Ground[MaxGroundProbes] = {};
		int32 GroundCount = 0;
		for (const FNearby& Entry : Nearby)
		{
			if (GroundCount < MaxGroundProbes && Entry.Config && !Entry.Config->IsFlying() && !Entry.Config->IsCrawling())
			{
				Ground[GroundCount++] = &Entry;
			}
		}
		if (Collision && PlayerNav.bGrounded && PlayerNav.Surface.IsValid() && GroundCount > 0)
		{
			const FVector GoalFeet = PlayerNav.GroundPoint;
			LNPEnemyExactMovement::FParams Default;
			Default.bLateralSweep = LNPEnemyExactMovement::IsLateralSweepEnabled();
			auto EnemyParams = [&Default](const ULNPEnemyConfig& Config)
			{
				LNPEnemyExactMovement::FParams Params = Default;
				Params.GravityOrigin = Config.MovementConfig.GravityOrigin;
				Params.CapsuleRadius = Config.CapsuleRadius;
				Params.CapsuleHalfHeight = Config.CapsuleHalfHeight;
				return Params;
			};
			const LNPEnemyExactMovement::FParams Melee = EnemyParams(*Ground[0]->Config);
			UE_LOG(LogLootNPop, Display, TEXT("[NavProbe] player walkAnchored=%d node=%d feet=%s nearestGroundConfig=%s stop=%.0f"),
				PlayerNav.bWalkAnchored, PlayerNav.Node, *GoalFeet.ToString(), *Ground[0]->Config->GetName(),
				FLNPEnemyMovementConfig::ComputeStopDistance(Ground[0]->Config->MovementConfig.AttackRange));
			if (PlayerNav.bWalkAnchored && Snapshot.Nav.Graph.GetSlot(PlayerNav.Node) != INDEX_NONE)
			{
				const FVector NodeFeet(Snapshot.Nav.Graph.GetWorldPoint(Snapshot.Nav.Graph.GetSlot(PlayerNav.Node), PlayerNav.Node));
				// 실제 anchor 검증과 같은 입력(기본 캡슐·플레이어 바닥 Surface·150cm), 실제 근접 캡슐, 그리고 얼마나 가까이 가는지(30cm).
				LogWalkProbe(Snapshot, *Collision, TEXT("anchorCheck"), Default, NodeFeet, PlayerNav.Surface, GoalFeet, 150.0);
				LogWalkProbe(Snapshot, *Collision, TEXT("anchorMeleeCapsule"), Melee, NodeFeet, PlayerNav.Surface, GoalFeet, 150.0);
				LogWalkProbe(Snapshot, *Collision, TEXT("anchorClosest"), Default, NodeFeet, PlayerNav.Surface, GoalFeet, 30.0);
				LogWalkProbe(Snapshot, *Collision, TEXT("anchorMeleeClosest"), Melee, NodeFeet, PlayerNav.Surface, GoalFeet, 30.0);
			}
			for (int32 I = 0; I < GroundCount; ++I)
			{
				const FNearby& Entry = *Ground[I];
				const LNPEnemyExactMovement::FParams Params = EnemyParams(*Entry.Config);
				const FVector Up = (Params.GravityOrigin - Entry.Center).GetSafeNormal();
				const FVector EnemyFeet = Entry.Center - Up * Params.CapsuleHalfHeight;
				const FVector ToPlayer = FVector::VectorPlaneProject(GoalFeet - EnemyFeet, Up).GetSafeNormal();
				const FVector PlanarPush = FVector::VectorPlaneProject(Entry.Push, Up);
				UE_LOG(LogLootNPop, Display, TEXT("[NavProbe] enemyIndex=%d distance=%.1f moveSpeed=%.0f push=%.1f pushForward=%.1f pushSide=%.1f"),
					I, Entry.Distance, Entry.Config->MovementConfig.MoveSpeed, PlanarPush.Size(), FVector::DotProduct(PlanarPush, ToPlayer),
					FVector::CrossProduct(ToPlayer, PlanarPush).Size());
				// 실제 이동 프로세서처럼 현재 자리에서 플레이어 발로 곧게: 실제 캡슐(반지름 보폭), 프레임 보폭, 기본 캡슐.
				LogWalkProbe(Snapshot, *Collision, TEXT("enemyMeleeCapsule"), Params, EnemyFeet, Entry.Surface, GoalFeet, 30.0);
				LogWalkProbe(Snapshot, *Collision, TEXT("enemyFrameStride"), Params, EnemyFeet, Entry.Surface, GoalFeet, 30.0,
					Entry.Config->MovementConfig.MoveSpeed / 60.0);
				LogWalkProbe(Snapshot, *Collision, TEXT("enemyDefaultCapsule"), Default, EnemyFeet, Entry.Surface, GoalFeet, 30.0);
				// 지금 조향점(점열 점·waypoint·anchor)까지 실제로 걸어 닿는지. 조향점이 이미 30cm 안이면 걷지 않고 닿는다.
				if (Entry.bHasSteeringPoint)
				{
					LogWalkProbe(Snapshot, *Collision, TEXT("enemyToSteer"), Params, EnemyFeet, Entry.Surface, Entry.SteeringPoint, 30.0);
				}
			}
		}
		UE_LOG(LogLootNPop, Display, TEXT("[NavEnemy] aliveFlyers=%d within5000cm=%d"), Flyers, NearbyFlyers);
	}

	enum class ENavDrawMode : uint8
	{
		Component,
		Tile,
		Group,
	};

	const TCHAR* LexToString(const ENavDrawMode Mode)
	{
		switch (Mode)
		{
		case ENavDrawMode::Tile: return TEXT("tile");
		case ENavDrawMode::Group: return TEXT("group");
		default: return TEXT("component");
		}
	}

	/** 보고서와 그림이 같은 ID에 같은 색을 쓰도록 ID 해시로 색상만 바꾼다. */
	FColor MakeIdColor(const uint32 Id)
	{
		const uint32 Hash = HashCombineFast(Id, 0x9E3779B9u);
		return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hash & 0xFF), 190, 255).ToFColor(true);
	}

	FString DescribeColor(const FColor Color)
	{
		return FString::Printf(TEXT("#%02X%02X%02X"), Color.R, Color.G, Color.B);
	}

	TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> TakeReadySnapshot(UWorld* World, const TCHAR* Tag)
	{
		const ULNPSurfaceDataSubsystem* SurfaceData = World ? World->GetSubsystem<ULNPSurfaceDataSubsystem>() : nullptr;
		TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot =
			SurfaceData ? SurfaceData->TakeSnapshot() : nullptr;
		if (!Snapshot.IsValid() || !Snapshot->Nav.IsValid())
		{
			UE_LOG(LogLootNPop, Warning, TEXT("[%s] %s -> NOT_READY"), Tag, *GetNameSafe(World));
			return nullptr;
		}
		return Snapshot;
	}

	/** 로컬 플레이어 폰의 발 위치. 구 내부 중력은 바깥쪽이므로 발은 중심 반대 방향으로 반높이만큼 떨어져 있다. */
	bool GetLocalFeet(UWorld* World, FVector3d& OutFeet)
	{
		const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			return false;
		}
		const FVector3d Location = Pawn->GetActorLocation();
		OutFeet = Location + Location.GetSafeNormal() * Pawn->GetSimpleCollisionHalfHeight();
		return true;
	}

	/** 발 위치에서 Support handle을 찾아 같은 Layer의 node로 투영한다. */
	bool ProjectFeet(const FLNPSurfaceDataSnapshot& Snapshot, const FVector3d& Feet, FLNPNavProjection& OutProjection)
	{
		FLNPSurfaceQuery Query;
		Query.WorldPosition = Feet;
		Query.MaxStepUp = 100.0;
		Query.MaxDrop = 300.0;
		FLNPSurfaceQueryResult Result;
		return LNPSurfaceDataLoading::QuerySupport(Snapshot, Query, Result) == ELNPSurfaceQueryStatus::HighConfidence
			&& LNPNavQuery::ProjectToNode(
				Snapshot, Feet, Result.Surface, LNPNavQuery::DefaultProjectionRadius, OutProjection);
	}

	/** runtime StaticNavComponent별 node 수. asset-local descriptor를 runtime ID로 합산한다. */
	TArray<uint64> ComputeComponentSizes(const FLNPSurfaceDataSnapshot& Snapshot)
	{
		TArray<uint64> Sizes;
		Sizes.SetNumZeroed(Snapshot.Nav.RuntimeStaticComponentCount);
		for (int32 Slot = 0; Slot < Snapshot.Slots.Num(); ++Slot)
		{
			for (const FLNPNavStaticComponent& Component : Snapshot.Slots[Slot].Traversal->StaticComponents)
			{
				const uint32 Runtime = LNPNavRuntime::GetRuntimeStaticComponent(
					Snapshot.Nav, Slot, Component.LocalStaticComponentId);
				if (Sizes.IsValidIndex(Runtime))
				{
					Sizes[Runtime] += Component.NodeCount;
				}
			}
		}
		return Sizes;
	}

	void LogNodeLine(const TCHAR* Tag, const FLNPSurfaceDataSnapshot& Snapshot, const FLNPNavProjection& Projection,
		const TArray<uint64>& ComponentSizes)
	{
		int32 Slot = INDEX_NONE;
		FLNPLocalNavNodeRef Local;
		LNPNavRuntime::ResolveRuntimeNodeRef(Snapshot.Nav, Projection.Node, Slot, Local);
		uint32 Component = MAX_uint32;
		FLNPNavGroupRef Group;
		LNPNavQuery::GetStaticComponent(Snapshot, Projection.Node, Component);
		LNPNavQuery::GetReachabilityGroup(Snapshot, Projection.Node, Group);
		const uint64 Size = ComponentSizes.IsValidIndex(Component) ? ComponentSizes[Component] : 0;
		UE_LOG(LogLootNPop, Display,
			TEXT("[%s] localNode slot=%d layer=%u(runtime %u) tile=%u cell=%u distance=%.1fcm component=%u(%s nodes=%llu) group=%u(%s) version=%u"),
			Tag, Slot, Local.LocalNavLayerId, Projection.Node.RuntimeNavLayerId, Local.TileId, Local.LocalCellIndex,
			Projection.Distance, Component, *DescribeColor(MakeIdColor(Component)), Size,
			Group.Group, *DescribeColor(MakeIdColor(Group.Group)), Group.ConnectivityGraphVersion);
	}

	/**
	 * LNP.SurfaceNav.NavReport [TopComponents]
	 * 게시된 Nav snapshot의 slot별 cell·Tile·component·portal·seam 수, 막힘, ReachabilityGroup, component 크기 분포와
	 * 로컬 폰 위치의 node를 기록한다. DrawNav와 같은 색 규칙을 쓴다.
	 */
	FAutoConsoleCommandWithWorldAndArgs GLNPNavReport(
		TEXT("LNP.SurfaceNav.NavReport"),
		TEXT("Log published Nav counts per slot, seam/blocked/portal totals, ReachabilityGroups, StaticNavComponent size ")
		TEXT("distribution, and the local pawn's projected node. Args: [TopComponents] (default 10)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			constexpr TCHAR Tag[] = TEXT("NavReport");
			const TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot = TakeReadySnapshot(World, Tag);
			if (!Snapshot.IsValid())
			{
				return;
			}
			const int32 TopCount = Args.Num() > 0 ? FMath::Max(0, FCString::Atoi(*Args[0])) : 10;
			const FLNPNavSnapshot& Nav = Snapshot->Nav;
			// 7b 탐색 분기 수 입력. seam link·portal은 제외한 같은 Layer grid edge 기준 차수다.
			int64 GridDegrees[7] = {};
			UE_LOG(LogLootNPop, Display,
				TEXT("[%s] %s NetMode=%d generation=%llu connectivityVersion=%u runtimeLayers=%u runtimeComponents=%u groups=%u seamLinks=%d blockedSeamNodes=%d blockedSeamEdges=%d"),
				Tag, *GetNameSafe(World), static_cast<int32>(World->GetNetMode()), Nav.SnapshotGeneration,
				Nav.ConnectivityGraphVersion, Nav.SlotLayerBase.Last(), Nav.RuntimeStaticComponentCount,
				Nav.ReachabilityGroupCount, Nav.SeamLinks.Num(), Nav.BlockedSeamNodes.Num(), Nav.BlockedSeamEdges.Num());

			for (int32 Slot = 0; Slot < Snapshot->Slots.Num(); ++Slot)
			{
				const FLNPSurfaceDataSlotSnapshot& SlotSnapshot = Snapshot->Slots[Slot];
				int32 Tiles = 0;
				int32 Cells = 0;
				int32 EdgeEnds = 0;
				int32 IsolatedCells = 0;
				for (const FLNPNavLayer& Layer : SlotSnapshot.Navigation->Layers)
				{
					Tiles += Layer.Tiles.Num();
					for (const FLNPNavTile& Tile : Layer.Tiles)
					{
						Cells += Tile.Cells.Num();
						for (const FLNPNavCell& Cell : Tile.Cells)
						{
							const int32 Degree = FMath::CountBits(Cell.EdgeMask);
							EdgeEnds += Degree;
							IsolatedCells += Degree == 0 ? 1 : 0;
							++GridDegrees[FMath::Min(Degree, 6)];
						}
					}
				}
				TSet<uint32> RuntimeComponents;
				for (uint32 Local = 0; Local < SlotSnapshot.Navigation->LocalStaticComponentCount; ++Local)
				{
					RuntimeComponents.Add(LNPNavRuntime::GetRuntimeStaticComponent(Nav, Slot, static_cast<uint16>(Local)));
				}
				UE_LOG(LogLootNPop, Display,
					TEXT("[%s] slot=%d asset=%s layers=%d(runtime %u..%u) tiles=%d cells=%d edges=%d isolatedCells=%d localComponents=%u runtimeComponents=%d portals=%d seamEndpoints=%d"),
					Tag, Slot, *SlotSnapshot.SurfaceDataAsset.GetAssetName(), SlotSnapshot.Navigation->Layers.Num(),
					Nav.SlotLayerBase[Slot], Nav.SlotLayerBase[Slot + 1] - 1, Tiles, Cells, EdgeEnds / 2, IsolatedCells,
					SlotSnapshot.Navigation->LocalStaticComponentCount, RuntimeComponents.Num(),
					SlotSnapshot.Traversal->Portals.Num(), SlotSnapshot.Traversal->SeamEndpoints.Num());
			}

			UE_LOG(LogLootNPop, Display,
				TEXT("[%s] gridDegree 0:%lld 1:%lld 2:%lld 3:%lld 4:%lld 5:%lld 6:%lld (all slots, excludes seam links and portals)"),
				Tag, GridDegrees[0], GridDegrees[1], GridDegrees[2], GridDegrees[3], GridDegrees[4], GridDegrees[5],
				GridDegrees[6]);

			const TArray<uint64> Sizes = ComputeComponentSizes(*Snapshot);
			int32 Buckets[5] = {};
			for (const uint64 Size : Sizes)
			{
				++Buckets[Size <= 1 ? 0 : Size <= 10 ? 1 : Size <= 100 ? 2 : Size <= 1000 ? 3 : 4];
			}
			UE_LOG(LogLootNPop, Display,
				TEXT("[%s] componentSizes nodes<=1:%d 2-10:%d 11-100:%d 101-1000:%d >1000:%d"),
				Tag, Buckets[0], Buckets[1], Buckets[2], Buckets[3], Buckets[4]);
			TArray<uint32> Order;
			Order.Reserve(Sizes.Num());
			for (uint32 Component = 0; Component < static_cast<uint32>(Sizes.Num()); ++Component)
			{
				Order.Add(Component);
			}
			Order.StableSort([&Sizes](const uint32 A, const uint32 B) { return Sizes[A] > Sizes[B]; });
			for (int32 Rank = 0; Rank < FMath::Min(TopCount, Order.Num()); ++Rank)
			{
				const uint32 Component = Order[Rank];
				const uint32 Group = Nav.ReachabilityGroupByStaticComponent[Component];
				UE_LOG(LogLootNPop, Display, TEXT("[%s] top%d component=%u(%s) nodes=%llu group=%u(%s)"),
					Tag, Rank + 1, Component, *DescribeColor(MakeIdColor(Component)), Sizes[Component],
					Group, *DescribeColor(MakeIdColor(Group)));
			}

			FVector3d Feet;
			FLNPNavProjection Projection;
			if (!GetLocalFeet(World, Feet))
			{
				UE_LOG(LogLootNPop, Display, TEXT("[%s] localNode none (no local pawn)"), Tag);
			}
			else if (!ProjectFeet(*Snapshot, Feet, Projection))
			{
				FLNPSurfaceQuery Query;
				Query.WorldPosition = Feet;
				Query.MaxStepUp = 100.0;
				Query.MaxDrop = 300.0;
				FLNPSurfaceQueryResult Result;
				const ELNPSurfaceQueryStatus Status = LNPSurfaceDataLoading::QuerySupport(*Snapshot, Query, Result);
				UE_LOG(LogLootNPop, Display,
					TEXT("[%s] localNode none within %.0fcm of feet %s (radius %.1f) supportStatus=%d slot=%u layer=%u"),
					Tag, LNPNavQuery::DefaultProjectionRadius, *Feet.ToString(), Feet.Length(), static_cast<int32>(Status),
					Result.Surface.OctantSlot, Result.Surface.LocalLayerId);
			}
			else
			{
				LogNodeLine(Tag, *Snapshot, Projection, Sizes);
			}
			if (GetLocalFeet(World, Feet)) { LogNearbyEnemies(*World, *Snapshot, Feet); }
			if (const ULNPNavPathSubsystem* Paths = World->GetSubsystem<ULNPNavPathSubsystem>())
			{
				int32 StatusCounts[static_cast<int32>(ELNPNavPathStatus::Cancelled) + 1] = {};
				Paths->GetScheduler().VisitResults([&](FMassEntityHandle, const FLNPNavPathResult& Result)
				{
					++StatusCounts[static_cast<int32>(Result.Status)];
				});
				const FLNPNavPathScheduler& Scheduler = Paths->GetScheduler();
				const FLNPNavPathSchedulerStats& Stats = Scheduler.GetStats();
				UE_LOG(LogLootNPop, Display,
					TEXT("[%s] paths queued=%d running=%d success=%d noPath=%d unreachable=%d noNode=%d stale=%d cancelled=%d submitted=%llu expansions=%llu lastTickExpansions=%d lastTickMs=%.3f cacheHits=%llu cacheMisses=%llu followerFrames=%llu followingPathFrames=%llu waypointsAdvanced=%llu shortcutWalks=%llu shortcutPasses=%llu"),
					Tag, Scheduler.GetQueuedCount(), Scheduler.GetRunningCount(),
					StatusCounts[static_cast<int32>(ELNPNavPathStatus::Succeeded)],
					StatusCounts[static_cast<int32>(ELNPNavPathStatus::NoPath)],
					StatusCounts[static_cast<int32>(ELNPNavPathStatus::Unreachable)],
					StatusCounts[static_cast<int32>(ELNPNavPathStatus::NoNode)],
					StatusCounts[static_cast<int32>(ELNPNavPathStatus::Stale)],
					StatusCounts[static_cast<int32>(ELNPNavPathStatus::Cancelled)],
					Stats.Submitted, Stats.Expansions, Stats.LastTickExpansions,
					Paths->GetLastTickSeconds() * 1000.0, Stats.CacheHits, Stats.CacheMisses,
					Paths->GetFollowerFrames(), Paths->GetFollowingPathFrames(), Paths->GetFollowerWaypointsAdvanced(),
					Paths->GetPathShortcutWalks(), Paths->GetPathShortcutPasses());
			}
		}));

	/**
	 * LNP.SurfaceNav.DrawNav [component|tile|group] [Radius] [Seconds]
	 * 로컬 폰 주변 반경 안의 Nav node·edge를 모드별 ID 색으로 그린다. 막힌 이음매 node·edge는 빨강,
	 * seam link는 청록 세로선, Layer portal은 자홍 굵은 선이다. 폰이 서 있는 node와 ID를 라벨로 표시한다.
	 */
	FAutoConsoleCommandWithWorldAndArgs GLNPDrawNav(
		TEXT("LNP.SurfaceNav.DrawNav"),
		TEXT("Draw Nav nodes/edges around the local pawn colored by StaticNavComponent, Tile, or ReachabilityGroup. ")
		TEXT("Red = blocked seam node/edge, cyan = seam link, magenta = Layer portal. ")
		TEXT("Args: [component|tile|group] (default component) [Radius] (default 3000) [Seconds] (default 15)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			constexpr TCHAR Tag[] = TEXT("DrawNav");
			const TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot = TakeReadySnapshot(World, Tag);
			FVector3d Center;
			if (!Snapshot.IsValid() || !GetLocalFeet(World, Center))
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[%s] Needs a published snapshot and a local pawn."), Tag);
				return;
			}
			ENavDrawMode Mode = ENavDrawMode::Component;
			if (Args.Num() > 0)
			{
				Mode = Args[0] == TEXT("tile") ? ENavDrawMode::Tile
					: Args[0] == TEXT("group") ? ENavDrawMode::Group
					: ENavDrawMode::Component;
			}
			const double Radius = Args.Num() > 1 ? FMath::Max(100.0, FCString::Atod(*Args[1])) : 3000.0;
			const float Seconds = Args.Num() > 2 ? FMath::Max(0.1f, FCString::Atof(*Args[2])) : 15.0f;
			const FLNPNavSnapshot& Nav = Snapshot->Nav;
			const ULNPNavPathSubsystem* NavPaths = World->GetSubsystem<ULNPNavPathSubsystem>();
			const TSharedPtr<const FLNPNavOverlay, ESPMode::ThreadSafe> Overlay = NavPaths ? NavPaths->TakeOverlay() : nullptr;
			// 지면과 겹쳐 가려지지 않도록 중심 방향(위)으로 살짝 띄운다.
			auto Lift = [](const FVector3d& Point) { return Point - Point.GetSafeNormal() * 8.0; };
			auto NodeColor = [&](const FLNPNavNodeRef& Node, const uint16 TileId)
			{
				uint32 Component = MAX_uint32;
				LNPNavQuery::GetStaticComponent(*Snapshot, Node, Component);
				switch (Mode)
				{
				case ENavDrawMode::Tile:
					return MakeIdColor(HashCombineFast(static_cast<uint32>(Node.RuntimeNavLayerId), TileId));
				case ENavDrawMode::Group:
					return MakeIdColor(LNPNavGraph::GetGroup(Nav, LNPNavGraph::ToGraphNode(Nav, Node)));
				default:
					return MakeIdColor(Component);
				}
			};

			int32 DrawnNodes = 0;
			int32 DrawnEdges = 0;
			int32 DrawnBlocked = 0;
			for (int32 Slot = 0; Slot < Snapshot->Slots.Num(); ++Slot)
			{
				const FLNPSurfaceDataSlotSnapshot& SlotSnapshot = Snapshot->Slots[Slot];
				const FLNPNavData& Navigation = *SlotSnapshot.Navigation;
				for (const FLNPNavLayer& Layer : Navigation.Layers)
				{
					const double LayerRadius = SlotSnapshot.Support->Layers[Layer.LocalSupportLayerId].BaseRadius;
					for (const FLNPNavTile& Tile : Layer.Tiles)
					{
						for (const FLNPNavCell& Cell : Tile.Cells)
						{
							const FIntPoint Coord = LNPNavData::DecodeTileAddress(Tile.TileX, Tile.TileY, Cell.LocalCellIndex);
							// Support 조회 전에 기준 반지름 구면 위 방향으로 먼 node를 먼저 거른다.
							const FVector3d Rough = SlotSnapshot.SlotRotation.RotateVector(
								LNPSupportAtlas::GetSampleDirection(Layer.Subdivisions, Coord.X, Coord.Y)) * LayerRadius;
							if (FVector3d::DistSquared(Rough, Center) > FMath::Square(Radius * 1.5 + 500.0))
							{
								continue;
							}
							FLNPNavNodeRef Node;
							FVector3d Point;
							FVector3f Normal;
							if (!LNPNavRuntime::MakeRuntimeNodeRef(Nav, Slot,
									{Layer.LocalNavLayerId, Tile.TileId, Cell.LocalCellIndex}, Node)
								|| !LNPNavQuery::GetNodeSupport(*Snapshot, Node, Point, Normal)
								|| FVector3d::DistSquared(Point, Center) > FMath::Square(Radius))
							{
								continue;
							}
						const bool bBlocked = LNPNavRuntime::IsBlockedSeamNode(Nav, Node);
						const bool bPodBlocked = LNPNavOverlay::IsBlocked(Overlay.Get(), LNPNavGraph::ToGraphNode(Nav, Node));
						const FColor Color = bPodBlocked ? FColor::Orange : bBlocked ? FColor::Red : NodeColor(Node, Tile.TileId);
						DrawDebugPoint(World, Lift(Point), bBlocked || bPodBlocked ? 16.0f : 9.0f, Color, false, Seconds);
							++DrawnNodes;
						DrawnBlocked += bBlocked || bPodBlocked ? 1 : 0;
							// 대칭 edge를 한 번만 그리도록 양의 방향(0, 2, 4)만 본다.
							for (const ELNPNavNeighbor Direction :
								{ELNPNavNeighbor::IPositive, ELNPNavNeighbor::JPositive, ELNPNavNeighbor::IPositiveJNegative})
							{
								FIntPoint NeighborCoord;
								FLNPLocalNavNodeRef NeighborLocal;
								FLNPNavNodeRef Neighbor;
								FVector3d NeighborPoint;
								FVector3f NeighborNormal;
								if ((Cell.EdgeMask & LNPNavData::GetNeighborBit(Direction)) == 0
									|| !LNPNavData::TryGetNeighborCoord(Layer.Subdivisions, Coord.X, Coord.Y, Direction, NeighborCoord)
									|| !LNPNavData::MakeLocalNodeRef(Navigation, Layer.LocalNavLayerId,
										NeighborCoord.X, NeighborCoord.Y, NeighborLocal)
									|| !LNPNavRuntime::MakeRuntimeNodeRef(Nav, Slot, NeighborLocal, Neighbor)
									|| !LNPNavQuery::GetNodeSupport(*Snapshot, Neighbor, NeighborPoint, NeighborNormal))
								{
									continue;
								}
								DrawDebugLine(World, Lift(Point), Lift(NeighborPoint), Color, false, Seconds, 0, 1.5f);
								++DrawnEdges;
							}
						}
					}
				}
			}

			auto DrawIfNear = [&](const FLNPNavNodeRef& A, const FLNPNavNodeRef& B, const FColor Color, const float Thickness)
			{
				FVector3d PointA;
				FVector3d PointB;
				FVector3f Normal;
				if (LNPNavQuery::GetNodeSupport(*Snapshot, A, PointA, Normal)
					&& LNPNavQuery::GetNodeSupport(*Snapshot, B, PointB, Normal)
					&& FVector3d::DistSquared(PointA, Center) <= FMath::Square(Radius))
				{
					DrawDebugLine(World, Lift(PointA), Lift(PointB), Color, false, Seconds, 0, Thickness);
					return true;
				}
				return false;
			};
			int32 DrawnSeamLinks = 0;
			for (const FLNPNavSeamLink& Link : Nav.SeamLinks)
			{
				FVector3d Point;
				FVector3f Normal;
				if (LNPNavQuery::GetNodeSupport(*Snapshot, Link.A, Point, Normal)
					&& FVector3d::DistSquared(Point, Center) <= FMath::Square(Radius))
				{
					DrawDebugLine(World, Point, Point - Point.GetSafeNormal() * 60.0, FColor::Cyan, false, Seconds, 0, 2.0f);
					++DrawnSeamLinks;
				}
			}
			for (const FLNPNavEdgeRef& Edge : Nav.BlockedSeamEdges)
			{
				DrawIfNear(Edge.From, Edge.To, FColor::Red, 4.0f);
			}
			int32 DrawnPortals = 0;
			for (int32 Slot = 0; Slot < Snapshot->Slots.Num(); ++Slot)
			{
				for (const FLNPNavPortal& Portal : Snapshot->Slots[Slot].Traversal->Portals)
				{
					FLNPNavNodeRef A;
					FLNPNavNodeRef B;
					if (LNPNavRuntime::MakeRuntimeNodeRef(Nav, Slot, Portal.A, A)
						&& LNPNavRuntime::MakeRuntimeNodeRef(Nav, Slot, Portal.B, B))
					{
						DrawnPortals += DrawIfNear(A, B, FColor::Magenta, 6.0f) ? 1 : 0;
					}
				}
			}

			FLNPNavProjection Projection;
			if (ProjectFeet(*Snapshot, Center, Projection))
			{
				uint32 Component = MAX_uint32;
				FLNPNavGroupRef Group;
				LNPNavQuery::GetStaticComponent(*Snapshot, Projection.Node, Component);
				LNPNavQuery::GetReachabilityGroup(*Snapshot, Projection.Node, Group);
				DrawDebugString(World, Lift(Projection.Point) - Projection.Point.GetSafeNormal() * 100.0,
					FString::Printf(TEXT("C%u G%u v%u"), Component, Group.Group, Group.ConnectivityGraphVersion),
					nullptr, FColor::White, Seconds);
				DrawDebugPoint(World, Lift(Projection.Point), 20.0f, FColor::White, false, Seconds);
				const TArray<uint64> Sizes = ComputeComponentSizes(*Snapshot);
				LogNodeLine(Tag, *Snapshot, Projection, Sizes);
			}
			int32 DrawnPaths = 0;
			if (NavPaths)
			{
				NavPaths->GetScheduler().VisitResults([&](FMassEntityHandle, const FLNPNavPathResult& Result)
				{
					if (!Result.Path.IsValid())
					{
						return;
					}
					const TArray<FLNPNavPathWaypoint>& Waypoints = Result.Path->Waypoints;
					for (int32 Index = 1; Index < Waypoints.Num(); ++Index)
					{
						if (FVector3d::DistSquared(Waypoints[Index].Location, Center) <= FMath::Square(Radius))
						{
							DrawDebugLine(World, Lift(Waypoints[Index - 1].Location), Lift(Waypoints[Index].Location),
								FColor::Yellow, false, Seconds, 0, 3.0f);
							++DrawnPaths;
						}
					}
				});
			}
			UE_LOG(LogLootNPop, Display,
				TEXT("[%s] mode=%s radius=%.0f nodes=%d edges=%d blockedNodes=%d seamLinks=%d portals=%d pathSegments=%d"),
				Tag, LexToString(Mode), Radius, DrawnNodes, DrawnEdges, DrawnBlocked, DrawnSeamLinks, DrawnPortals, DrawnPaths);
		}));
}
#endif
