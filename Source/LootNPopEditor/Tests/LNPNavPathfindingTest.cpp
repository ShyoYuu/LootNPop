// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Config/LNPSettings.h"
#include "Enemy/LNPEnemyNavigation.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPTargetingSubsystem.h"
#include "Mass/EntityFragments.h"
#include "MassEntityManager.h"
#include "DataAsset/LNPOctantPoolData.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "SurfaceNavigation/LNPCrustAtlas.h"
#include "SurfaceNavigation/LNPNavOverlay.h"
#include "SurfaceNavigation/LNPNavPathScheduler.h"
#include "SurfaceNavigation/LNPNavPathfinding.h"
#include "SurfaceNavigation/LNPNavQuery.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"
#include "SurfaceNavigation/LNPSpawnData.h"

namespace LNPNavPathfindingTest
{
	constexpr TCHAR RegressionSurfacePath[] =
		TEXT("/Game/Maps/SurfaceNavigation/Fixtures/DA_OctantSurface_Fixture_Regression.DA_OctantSurface_Fixture_Regression");
	constexpr double FeetLift = 5.0;

	template <typename EnumType>
	bool TestEnum(FAutomationTestBase& Test, const FString& What, const EnumType Actual, const EnumType Expected)
	{
		return Test.TestTrue(FString::Printf(TEXT("%s (got %d, expected %d)"), *What,
			static_cast<int32>(Actual), static_cast<int32>(Expected)), Actual == Expected);
	}

	bool BuildRegressionSnapshot(FAutomationTestBase& Test, FLNPSurfaceDataSnapshot& OutSnapshot)
	{
		ULNPOctantSurfaceData* Regression = TSoftObjectPtr<ULNPOctantSurfaceData>(
			FSoftObjectPath(RegressionSurfacePath)).LoadSynchronous();
		if (!Test.TestNotNull(TEXT("Regression SurfaceData loads"), Regression))
		{
			return false;
		}
		TArray<FLNPOctantDefinition> Definitions;
		Definitions.SetNum(8);
		for (FLNPOctantDefinition& Definition : Definitions)
		{
			Definition.LevelAsset = TSoftObjectPtr<UWorld>(FSoftObjectPath(LNPRegressionFixture::LevelPath));
			Definition.SurfaceData = TSoftObjectPtr<ULNPOctantSurfaceData>(FSoftObjectPath(RegressionSurfacePath));
			Definition.SeamSignature = TEXT("RegressionSeam");
		}
		TArray<ULNPOctantSurfaceData*> LoadedData;
		LoadedData.Init(Regression, 8);
		FString Error;
		if (!LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, LoadedData, 3, OutSnapshot, Error))
		{
			Test.AddError(Error);
			return false;
		}
		return true;
	}

	bool BuildProductionSnapshot(FAutomationTestBase& Test, FLNPSurfaceDataSnapshot& OutSnapshot, double& OutBuildMs)
	{
		const ULNPSettings* Settings = GetDefault<ULNPSettings>();
		ULNPOctantPoolData* Pool = Settings ? Settings->OctantPool.LoadSynchronous() : nullptr;
		if (!Test.TestNotNull(TEXT("Production OctantPool loads"), Pool))
		{
			return false;
		}
		TArray<FLNPOctantDefinition> EffectiveDefinitions;
		Pool->BuildEffectiveDefinitions(EffectiveDefinitions);
		TArray<FLNPOctantDefinition> SelectedDefinitions;
		FString Error;
		if (!ULNPOctantSpawnSubsystem::SelectOctantDefinitions(EffectiveDefinitions, 135792468, SelectedDefinitions, nullptr, &Error))
		{
			Test.AddError(Error);
			return false;
		}
		TArray<ULNPOctantSurfaceData*> LoadedData;
		for (const FLNPOctantDefinition& Definition : SelectedDefinitions)
		{
			LoadedData.Add(Definition.SurfaceData.LoadSynchronous());
		}
		const double StartSeconds = FPlatformTime::Seconds();
		const bool bBuilt = LNPSurfaceDataLoading::ValidateAndBuildSnapshot(SelectedDefinitions, LoadedData, 1, OutSnapshot, Error);
		OutBuildMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
		if (!bBuilt)
		{
			Test.AddError(Error);
		}
		return bBuilt;
	}

	/** 발 위치의 Support handle. HighConfidence가 아니면 invalid handle이다. */
	FLNPSurfaceHandle FindHandle(const FLNPSurfaceDataSnapshot& Snapshot, const FVector3d& Feet)
	{
		FLNPSurfaceQuery Query;
		Query.WorldPosition = Feet;
		Query.MaxStepUp = 50.0;
		Query.MaxDrop = 100.0;
		FLNPSurfaceQueryResult Result;
		return LNPSurfaceDataLoading::QuerySupport(Snapshot, Query, Result) == ELNPSurfaceQueryStatus::HighConfidence
			? Result.Surface : FLNPSurfaceHandle();
	}

	FLNPNavEndpoints Resolve(
		const FLNPSurfaceDataSnapshot& Snapshot, const FVector3d& Start, const FLNPSurfaceHandle& StartHandle,
		const FVector3d& Goal, const FLNPSurfaceHandle& GoalHandle)
	{
		FLNPNavEndpointQuery Query;
		Query.StartPosition = Start;
		Query.StartSurface = &StartHandle;
		Query.GoalPosition = Goal;
		Query.GoalSurface = &GoalHandle;
		return LNPNavGraph::ResolveEndpoints(Snapshot.Nav, Query);
	}

	struct FPathResult
	{
		ELNPNavSearchStatus Status = ELNPNavSearchStatus::Invalid;
		TArray<int32> Nodes;
		double Cost = 0.0;
		int32 Expansions = 0;
	};

	FPathResult FindPath(
		const FLNPNavSnapshot& Nav, const int32 Start, const int32 Goal, FLNPNavSearchScratch& Scratch,
		const bool bUseHeuristic = true, const int32 Budget = MAX_int32, const int32 MaxExpansions = 1000000)
	{
		FLNPNavSearchParams Params;
		Params.bUseHeuristic = bUseHeuristic;
		Params.MaxExpansions = MaxExpansions;
		FLNPNavSearch Search;
		FPathResult Result;
		Result.Status = LNPNavPathfinding::BeginSearch(Nav, Start, Goal, Params, Scratch, Search);
		while (Result.Status == ELNPNavSearchStatus::Running)
		{
			Result.Status = LNPNavPathfinding::StepSearch(Nav, Scratch, Search, Budget);
		}
		Result.Expansions = Search.Expansions;
		Result.Cost = Search.PathCost;
		LNPNavPathfinding::ExtractNodePath(Scratch, Search, Result.Nodes);
		return Result;
	}

	double Percentile(TArray<double> Values, const double Fraction)
	{
		if (Values.IsEmpty())
		{
			return 0.0;
		}
		Values.Sort();
		return Values[FMath::Clamp(FMath::CeilToInt32(Fraction * Values.Num()) - 1, 0, Values.Num() - 1)];
	}

	/** 가장 큰 group. production에서는 지각 주 component다. */
	uint32 FindLargestGroup(const FLNPNavSnapshot& Nav)
	{
		TArray<int32> Counts;
		Counts.SetNumZeroed(Nav.ReachabilityGroupCount);
		for (int32 Node = 0; Node < Nav.Graph.GetNodeCount(); ++Node)
		{
			const uint32 Group = LNPNavGraph::GetGroup(Nav, Node);
			if (Counts.IsValidIndex(Group))
			{
				++Counts[Group];
			}
		}
		int32 Best = 0;
		for (int32 Group = 1; Group < Counts.Num(); ++Group)
		{
			Best = Counts[Group] > Counts[Best] ? Group : Best;
		}
		return static_cast<uint32>(Best);
	}

	/** 조밀 view가 7a 조회와 같은 node 지면점·component를 내고, node ref와 왕복하며, 이웃이 대칭인지 본다. */
	void CheckGraphMatchesQuery(FAutomationTestBase& Test, const FLNPSurfaceDataSnapshot& Snapshot, const TCHAR* Label)
	{
		const FLNPNavSnapshot& Nav = Snapshot.Nav;
		const FLNPNavGraph& Graph = Nav.Graph;
		int32 RoundTripFailures = 0;
		int32 PointFailures = 0;
		int32 ComponentFailures = 0;
		int32 AsymmetricEdges = 0;
		double MaxPointError = 0.0;
		for (int32 Node = 0; Node < Graph.GetNodeCount(); ++Node)
		{
			FLNPNavNodeRef Ref;
			if (!LNPNavGraph::ToNodeRef(Nav, Node, Ref) || LNPNavGraph::ToGraphNode(Nav, Ref) != Node)
			{
				++RoundTripFailures;
				continue;
			}
			const int32 Slot = Graph.GetSlot(Node);
			FVector3d QueryPoint;
			FVector3f QueryNormal;
			if (LNPNavQuery::GetNodeSupport(Snapshot, Ref, QueryPoint, QueryNormal))
			{
				const double Error = FVector3d::Dist(QueryPoint, Graph.GetWorldPoint(Slot, Node));
				MaxPointError = FMath::Max(MaxPointError, Error);
				PointFailures += Error > 1.0 ? 1 : 0;
			}
			else
			{
				++PointFailures;
			}
			uint32 Component = MAX_uint32;
			const bool bBlocked = Graph.BlockedNodes[Node];
			const bool bHasComponent = LNPNavQuery::GetStaticComponent(Snapshot, Ref, Component);
			// 7a는 막힌 이음매 node의 component 조회를 거부한다.
			if (bBlocked ? bHasComponent
				: !bHasComponent || Nav.ReachabilityGroupByStaticComponent[Component] != LNPNavGraph::GetGroup(Nav, Node))
			{
				++ComponentFailures;
			}
			const FLNPNavGraphNode& GraphNode = Graph.GetNode(Slot, Node);
			for (uint8 Direction = 0; Direction < 6; ++Direction)
			{
				const int32 Neighbor = GraphNode.Neighbors[Direction];
				if (Neighbor != INDEX_NONE)
				{
					const FLNPNavGraphNode& Other = Graph.SlotGraphs[Slot]->Nodes[Neighbor];
					AsymmetricEdges += Other.Neighbors[Direction ^ 1] != Node - Graph.SlotNodeBase[Slot] ? 1 : 0;
				}
			}
		}
		Test.TestEqual(FString::Printf(TEXT("%s node ref round trip"), Label), RoundTripFailures, 0);
		Test.TestEqual(FString::Printf(TEXT("%s node point matches 7a Support query"), Label), PointFailures, 0);
		Test.TestEqual(FString::Printf(TEXT("%s node group matches 7a component"), Label), ComponentFailures, 0);
		Test.TestEqual(FString::Printf(TEXT("%s grid edges are reciprocal"), Label), AsymmetricEdges, 0);
		Test.AddInfo(FString::Printf(TEXT("%s graph: nodes=%d extraLinks=%d blockedEdges=%d maxPointError=%.4fcm resident=%.2fMiB"),
			Label, Graph.GetNodeCount(), Graph.ExtraLinks.Num(), Graph.BlockedEdges.Num(), MaxPointError,
			Graph.GetAllocatedBytes() / (1024.0 * 1024.0)));

		// 창 조회의 최근접 node는 7a ProjectToNode와 같아야 한다. Spawn 후보 위치로 비교한다.
		int32 Compared = 0;
		int32 Mismatches = 0;
		int32 Ties = 0;
		double MaxTieDelta = 0.0;
		TArray<FLNPNavGraphCandidate> Candidates;
		for (int32 Slot = 0; Slot < Snapshot.Slots.Num(); ++Slot)
		{
			const FLNPSurfaceDataSlotSnapshot& SlotSnapshot = Snapshot.Slots[Slot];
			for (int32 Index = 0; Index < SlotSnapshot.Spawn->RandomCandidates.Num(); Index += 7)
			{
				const FLNPSpawnRandomCandidate& Candidate = SlotSnapshot.Spawn->RandomCandidates[Index];
				const FVector3d World = SlotSnapshot.SlotRotation.RotateVector(FVector3d(Candidate.LocalPosition));
				FLNPNavProjection Projection;
				const bool bProjected = LNPNavQuery::ProjectToNode(Snapshot, World,
					{static_cast<uint16>(Slot), Candidate.LocalLayerId, Snapshot.Generation},
					LNPNavQuery::DefaultProjectionRadius, Projection);
				LNPNavGraph::CollectNodesNear(Nav, Slot, Candidate.LocalLayerId, World,
					LNPNavQuery::DefaultProjectionRadius, Candidates);
				TArray<FLNPNavGraphCandidate> Nearest;
				LNPNavGraph::CollectNodesNear(Nav, Slot, Candidate.LocalLayerId, World,
					LNPNavQuery::DefaultProjectionRadius, Nearest, nullptr, true);
				Test.TestEqual(TEXT("Nearest-only projection keeps one result"), Nearest.Num(), Candidates.IsEmpty() ? 0 : 1);
				if (!Nearest.IsEmpty() && !Candidates.IsEmpty())
				{
					Test.TestEqual(TEXT("Nearest-only projection matches sorted node"), Nearest[0].Node, Candidates[0].Node);
					Test.TestEqual(TEXT("Nearest-only projection matches sorted distance"), Nearest[0].Distance, Candidates[0].Distance);
				}
				++Compared;
				if (bProjected != !Candidates.IsEmpty())
				{
					++Mismatches;
					continue;
				}
				if (!bProjected || LNPNavGraph::ToGraphNode(Nav, Projection.Node) == Candidates[0].Node)
				{
					continue;
				}
				// 등거리 node는 동률 규칙이 달라(7a는 (J, I) 순, 조밀 view는 index 순) 다른 node를 고를 수 있다.
				// float 지면점 오차(0.01cm 미만)를 넘는 거리 차이만 불일치로 센다.
				const double Delta = FMath::Abs(Projection.Distance - Candidates[0].Distance);
				MaxTieDelta = FMath::Max(MaxTieDelta, Delta);
				++Ties;
				Mismatches += Delta > 0.01 ? 1 : 0;
			}
		}
		Test.TestEqual(FString::Printf(TEXT("%s window scan nearest matches 7a ProjectToNode (%d samples)"), Label, Compared),
			Mismatches, 0);
		Test.AddInfo(FString::Printf(TEXT("%s window scan: samples=%d equidistantTies=%d maxTieDelta=%.4fcm"),
			Label, Compared, Ties, MaxTieDelta));

		// Tile 경계·삼각 격자 끝·희소 Layer에서 반경 내 모든 후보를 빠짐없이 방문한다.
		// 주소 조회나 검색 창을 쓰지 않는 독립 oracle: 전 node를 거리만으로 걸러 비교한다.
		int32 WindowMismatches = 0;
		int32 WindowSamples = 0;
		for (int32 Slot = 0; Slot < Graph.SlotGraphs.Num(); ++Slot)
		{
			const FLNPNavAssetGraph& Asset = *Graph.SlotGraphs[Slot];
			for (const FLNPNavGraphLayer& Layer : Asset.Layers)
			{
				if (Layer.NodeBegin == Layer.NodeEnd)
				{
					continue;
				}
				for (const int32 Center : {Layer.NodeBegin, (Layer.NodeBegin + Layer.NodeEnd) / 2, Layer.NodeEnd - 1})
				{
					const FVector3d World = Graph.GetWorldPoint(Slot, Graph.SlotNodeBase[Slot] + Center) + FVector3d(97.0, -61.0, 43.0);
					for (const double Radius : {300.0, 3000.0})
					{
						LNPNavGraph::CollectNodesNear(Nav, Slot, Layer.LocalNavLayerId, World, Radius, Candidates);
						TArray<int32> Actual;
						for (const FLNPNavGraphCandidate& Candidate : Candidates)
						{
							Actual.Add(Candidate.Node);
						}
						Actual.Sort();
						TArray<int32> Expected;
						for (int32 Node = Layer.NodeBegin; Node < Layer.NodeEnd; ++Node)
						{
							const int32 Global = Graph.SlotNodeBase[Slot] + Node;
							if (!Graph.BlockedNodes[Global]
								&& FVector3d::DistSquared(Graph.GetWorldPoint(Slot, Global), World) <= FMath::Square(Radius))
							{
								Expected.Add(Global);
							}
						}
						WindowMismatches += Actual != Expected ? 1 : 0;
						++WindowSamples;
					}
				}
			}
		}
		Test.TestEqual(FString::Printf(TEXT("%s tiled window matches exhaustive node scan (%d samples)"), Label, WindowSamples),
			WindowMismatches, 0);
	}

	/** scheduler 요청 하나의 입력. */
	struct FSchedulerCase
	{
		const TCHAR* Label = TEXT("");
		FVector3d Start = FVector3d::ZeroVector;
		FLNPSurfaceHandle StartHandle;
		FVector3d Goal = FVector3d::ZeroVector;
		FLNPSurfaceHandle GoalHandle;
	};

	FLNPNavPathRequest MakeRequest(const FSchedulerCase& Case, const int32 OwnerIndex,
		const ELNPNavPathPriority Priority = ELNPNavPathPriority::Chase, const double ApproachRadius = 0.0)
	{
		FLNPNavPathRequest Request;
		Request.Owner = FMassEntityHandle(OwnerIndex, 1);
		Request.Priority = Priority;
		Request.StartPosition = Case.Start;
		Request.StartSurface = Case.StartHandle;
		Request.GoalPosition = Case.Goal;
		Request.GoalSurface = Case.GoalHandle;
		Request.ApproachRadius = ApproachRadius;
		return Request;
	}

	/** scheduler 없이 같은 요청을 한 번에 푼 waypoint node 열. 도달 불가면 비어 있다. */
	TArray<int32> ReferenceWaypoints(const FLNPSurfaceDataSnapshot& Snapshot, const FSchedulerCase& Case, FLNPNavSearchScratch& Scratch)
	{
		TArray<int32> Waypoints;
		const FLNPNavEndpoints Ends = Resolve(Snapshot, Case.Start, Case.StartHandle, Case.Goal, Case.GoalHandle);
		if (Ends.Status == ELNPNavEndpointStatus::Reachable)
		{
			const FPathResult Path = FindPath(Snapshot.Nav, Ends.StartNode, Ends.GoalNode, Scratch, true, MAX_int32, 30000);
			LNPNavPathfinding::SimplifyPath(Snapshot.Nav, Path.Nodes, Waypoints);
		}
		return Waypoints;
	}

	TArray<int32> WaypointNodes(const FLNPNavPathResult& Result)
	{
		TArray<int32> Nodes;
		if (Result.Path.IsValid())
		{
			for (const FLNPNavPathWaypoint& Waypoint : Result.Path->Waypoints)
			{
				Nodes.Add(Waypoint.Node);
			}
		}
		return Nodes;
	}

	/** 일이 남지 않을 때까지 tick한다. 반환값은 tick 수다. */
	int32 DrainScheduler(FLNPNavPathScheduler& Scheduler, const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay,
		TArray<double>* OutTickMicroseconds = nullptr, const int32 MaxTicks = 100000)
	{
		int32 Ticks = 0;
		while (Scheduler.HasWork() && Ticks < MaxTicks)
		{
			const double StartSeconds = FPlatformTime::Seconds();
			Scheduler.Tick(Nav, Overlay, [](FMassEntityHandle) { return true; });
			if (OutTickMicroseconds)
			{
				OutTickMicroseconds->Add((FPlatformTime::Seconds() - StartSeconds) * 1.e6);
			}
			++Ticks;
		}
		return Ticks;
	}

	/** 주 group 지각 node에서 직선 20~80m 떨어진 결정론적 무작위 도달 가능 쌍. */
	void CollectProductionPairs(const FLNPSurfaceDataSnapshot& Snapshot, const uint32 MainGroup, const int32 Count,
		const int32 Seed, TArray<FSchedulerCase>& OutCases)
	{
		const FLNPNavSnapshot& Nav = Snapshot.Nav;
		const FLNPNavGraph& Graph = Nav.Graph;
		FRandomStream Random(Seed);
		for (int32 Attempts = 0; OutCases.Num() < Count && Attempts < Count * 20; ++Attempts)
		{
			const int32 Start = Random.RandRange(0, Graph.GetNodeCount() - 1);
			const int32 Slot = Graph.GetSlot(Start);
			if (Graph.GetNode(Slot, Start).LayerOrdinal != 0 || Graph.BlockedNodes[Start] || LNPNavGraph::GetGroup(Nav, Start) != MainGroup)
			{
				continue;
			}
			const FVector3d StartPoint = Graph.GetWorldPoint(Slot, Start);
			const FVector3d Up = -StartPoint.GetSafeNormal();
			const FVector3d Tangent = FVector3d::CrossProduct(Up, FVector3d(Random.FRandRange(-1, 1), Random.FRandRange(-1, 1),
				Random.FRandRange(-1, 1))).GetSafeNormal();
			if (Tangent.IsNearlyZero())
			{
				continue;
			}
			const FVector3d Goal = (StartPoint + Tangent * Random.FRandRange(2000.0, 8000.0)).GetSafeNormal() * StartPoint.Length();
			FSchedulerCase Case;
			Case.Label = TEXT("Production");
			Case.Start = StartPoint;
			Case.StartHandle = {static_cast<uint16>(Slot), 0, Snapshot.Generation};
			Case.Goal = Goal;
			Case.GoalHandle = FindHandle(Snapshot, Goal + Up * FeetLift);
			if (Case.GoalHandle.IsValid()
				&& Resolve(Snapshot, Case.Start, Case.StartHandle, Case.Goal, Case.GoalHandle).Status == ELNPNavEndpointStatus::Reachable)
			{
				OutCases.Add(Case);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavPathSchedulerTest,
	"LootNPop.SurfaceNavigation.Nav.PathScheduler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavProductionSchedulerTest,
	"LootNPop.SurfaceNavigation.Nav.ProductionScheduler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavRequestCostReplayTest,
	"LootNPop.SurfaceNavigation.Nav.RequestCostReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavGraphViewTest,
	"LootNPop.SurfaceNavigation.Nav.GraphView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavRegressionPathTest,
	"LootNPop.SurfaceNavigation.Nav.RegressionPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavEnemyReachabilityTest,
	"LootNPop.SurfaceNavigation.Nav.EnemyReachability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPNavEnemyReachabilityTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavPathfindingTest;
	namespace Fixture = LNPRegressionFixture;
	FLNPSurfaceDataSnapshot Snapshot;
	if (!BuildRegressionSnapshot(*this, Snapshot))
	{
		return false;
	}
	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	const FQuat4d& Rotation = Snapshot.Slots[0].SlotRotation;
	auto World = [&Rotation](const FVector& Local) { return Rotation.RotateVector(FVector3d(Local)); };
	const FVector3d Island = World(Fixture::IslandOne().At(Fixture::IslandOneTop - FeetLift));
	const FVector3d Crust = World(Fixture::IslandOne().At(Fixture::CrustRadius - FeetLift, 1500.0));
	const FLNPSurfaceHandle IslandSurface = FindHandle(Snapshot, Island);
	const FLNPSurfaceHandle CrustSurface = FindHandle(Snapshot, Crust);
	if (!TestTrue(TEXT("Landing fixture resolves both surfaces"), IslandSurface.IsValid() && CrustSurface.IsValid()))
	{
		return false;
	}
	auto GroupAt = [&](const FVector3d& Feet, const FLNPSurfaceHandle& Surface)
	{
		TArray<FLNPNavGraphCandidate> Candidates;
		LNPNavGraph::CollectNodesNear(Nav, Surface.OctantSlot, Surface.LocalLayerId, Feet, 300.0, Candidates);
		return FLNPNavGroupRef{Candidates.IsEmpty() ? MAX_uint32 : Candidates[0].Group,
			Nav.ConnectivityGraphVersion, Nav.SnapshotGeneration};
	};
	const FLNPNavGroupRef IslandGroup = GroupAt(Island, IslandSurface);
	const FLNPNavGroupRef CrustGroup = GroupAt(Crust, CrustSurface);
	const FMassEntityHandle Player(1, 1);
	FLNPEnemySlotReachability State;
	TestFalse(TEXT("Disconnected island player receives no new melee slot"), LNPEnemyNavigation::UpdateMeleeSlot(
		Nav, ELNPNavEndpointStatus::Unreachable, Player, IslandGroup, true, false, 10.0, State));
	TestTrue(TEXT("Existing melee slot survives first disconnected grounded frame"), LNPEnemyNavigation::UpdateMeleeSlot(
		Nav, ELNPNavEndpointStatus::Unreachable, Player, IslandGroup, true, true, 10.0, State));
	TestTrue(TEXT("Slot remains during grace"), LNPEnemyNavigation::UpdateMeleeSlot(
		Nav, ELNPNavEndpointStatus::Unreachable, Player, IslandGroup, true, true, 11.49, State));
	TestFalse(TEXT("Slot returns at 1.5 seconds"), LNPEnemyNavigation::UpdateMeleeSlot(
		Nav, ELNPNavEndpointStatus::Unreachable, Player, IslandGroup, true, true, 11.5, State));
	TestTrue(TEXT("Jump preserves occupied slot"), LNPEnemyNavigation::UpdateMeleeSlot(
		Nav, ELNPNavEndpointStatus::NoNode, Player, IslandGroup, false, true, 12.0, State));
	TestFalse(TEXT("Jump cannot grant a new slot"), LNPEnemyNavigation::UpdateMeleeSlot(
		Nav, ELNPNavEndpointStatus::Reachable, Player, IslandGroup, false, false, 12.0, State));
	TestTrue(TEXT("Grounded reconnection immediately allows slot"), LNPEnemyNavigation::UpdateMeleeSlot(
		Nav, ELNPNavEndpointStatus::Reachable, Player, CrustGroup, true, false, 13.0, State));
	FLNPNavGroupRef StaleGroup = CrustGroup;
	++StaleGroup.ConnectivityGraphVersion;
	TestTrue(TEXT("Stale connectivity defers existing slot release"), LNPEnemyNavigation::UpdateMeleeSlot(
		Nav, ELNPNavEndpointStatus::Unreachable, Player, StaleGroup, true, true, 20.0, State));
	TestFalse(TEXT("Stale connectivity cannot grant slot"), LNPEnemyNavigation::UpdateMeleeSlot(
		Nav, ELNPNavEndpointStatus::Reachable, Player, StaleGroup, true, false, 20.0, State));

	// 슬롯 subsystem을 실제 Mass 엔티티로 실행해 풀 예산과 grace 필터의 연결을 검증한다.
	{
		const TSharedRef<FMassEntityManager> ManagerOwner = MakeShared<FMassEntityManager>();
		FMassEntityManager& Manager = ManagerOwner.Get();
		FMassEntityManagerStorageInitParams InitParams{TInPlaceType<FMassEntityManager_InitParams_Concurrent>()};
		Manager.Initialize(InitParams);
		ULNPTargetingSubsystem* Slots = NewObject<ULNPTargetingSubsystem>();
		TArray<FInstancedStruct> PlayerFragments;
		PlayerFragments.Add(FInstancedStruct::Make<FTransformFragment>());
		PlayerFragments.Add(FInstancedStruct::Make<FLNPPlayerNavFragment>());
		const FMassEntityHandle PlayerEntity = Manager.CreateEntity(PlayerFragments);
		FLNPPlayerNavFragment& PlayerData = Manager.GetFragmentDataChecked<FLNPPlayerNavFragment>(PlayerEntity);
		PlayerData.Surface = IslandSurface;
		PlayerData.GroundPoint = Island;
		PlayerData.GroundGroup = IslandGroup;
		PlayerData.bGrounded = true;
		auto MakeEnemy = [&](const ELNPEnemyAttackType AttackType)
		{
			ULNPEnemyConfig* Config = NewObject<ULNPEnemyConfig>();
			Config->AttackType = AttackType;
			FLNPEnemySharedFragment Shared;
			Shared.Config = Config;
			FMassArchetypeSharedFragmentValues SharedValues;
			SharedValues.Add(Manager.GetOrCreateConstSharedFragment(Shared));
			SharedValues.Sort();
			TArray<FInstancedStruct> Fragments;
			Fragments.Add(FInstancedStruct::Make<FTransformFragment>());
			Fragments.Add(FInstancedStruct::Make<FLNPEnemyFragment>());
			Fragments.Add(FInstancedStruct::Make<FLNPEnemyTargetingFragment>());
			const FMassEntityHandle Entity = Manager.CreateEntity(Fragments, SharedValues);
			Manager.GetFragmentDataChecked<FLNPEnemyFragment>(Entity).SurfaceHandle = CrustSurface;
			Manager.GetFragmentDataChecked<FTransformFragment>(Entity).SetTransform(
				FTransform(FQuat::Identity, FVector(Crust - Crust.GetSafeNormal() * Config->CapsuleHalfHeight)));
			Manager.GetFragmentDataChecked<FLNPEnemyTargetingFragment>(Entity).TargetPlayer = PlayerEntity;
			return Entity;
		};
		const FMassEntityHandle Melee = MakeEnemy(ELNPEnemyAttackType::Melee);
		const FMassEntityHandle Promoted = MakeEnemy(ELNPEnemyAttackType::Melee);
		const FMassEntityHandle Ranged = MakeEnemy(ELNPEnemyAttackType::Ranged);
		auto Rebalance = [&](const double Now)
		{
			Slots->RegisterEnemyInterest(Melee, PlayerEntity, 100.f, ELNPTargetSlotPool::Melee);
			Slots->RegisterEnemyInterest(Promoted, PlayerEntity, 100.f, ELNPTargetSlotPool::Promoted);
			Slots->RegisterEnemyInterest(Ranged, PlayerEntity, 100.f, ELNPTargetSlotPool::Ranged);
			Slots->RebalanceSlots(Manager, &Nav, Now);
		};
		Rebalance(0.0);
		TestFalse(TEXT("Actual melee pool rejects disconnected player"), Slots->IsSlotConfirmed(Melee, PlayerEntity));
		TestFalse(TEXT("Promoted melee also rejects disconnected player"), Slots->IsSlotConfirmed(Promoted, PlayerEntity));
		TestTrue(TEXT("Ranged pool keeps its existing non-Nav eligibility"), Slots->IsSlotConfirmed(Ranged, PlayerEntity));
		PlayerData.Surface = CrustSurface;
		PlayerData.GroundPoint = Crust;
		PlayerData.GroundGroup = CrustGroup;
		Rebalance(1.0);
		TestTrue(TEXT("Actual melee pool grants reconnected player"), Slots->IsSlotConfirmed(Melee, PlayerEntity));
		PlayerData.Surface = IslandSurface;
		PlayerData.GroundPoint = Island;
		PlayerData.GroundGroup = IslandGroup;
		Rebalance(2.0);
		TestTrue(TEXT("Actual existing melee slot starts grace"), Slots->IsSlotConfirmed(Melee, PlayerEntity));
		Rebalance(3.49);
		TestTrue(TEXT("Actual existing melee slot survives grace"), Slots->IsSlotConfirmed(Melee, PlayerEntity));
		Rebalance(3.5);
		TestFalse(TEXT("Actual melee slot is released after grace"), Slots->IsSlotConfirmed(Melee, PlayerEntity));
		TestFalse(TEXT("Actual promoted melee slot is released after grace"), Slots->IsSlotConfirmed(Promoted, PlayerEntity));
		TestTrue(TEXT("Ranged slot is unchanged by ground group switch"), Slots->IsSlotConfirmed(Ranged, PlayerEntity));
		Manager.Deinitialize();
	}

	TArray<FLNPEnemyHomePod> Pods;
	Pods.Add({FMassEntityHandle(10, 1), Island, IslandSurface, IslandGroup});
	Pods.Add({FMassEntityHandle(20, 1), Crust, CrustSurface, CrustGroup});
	int32 Selected;
	TestEnum(*this, TEXT("Knockback landing rehomes to reachable crust pod"),
		LNPEnemyNavigation::SelectHomePod(Nav, Crust, CrustSurface, Pods[0].Entity, Pods, Selected), ELNPNavEndpointStatus::Reachable);
	TestEqual(TEXT("New parent is crust pod"), Selected, 1);
	TestEnum(*this, TEXT("Same-group landing keeps existing parent"),
		LNPEnemyNavigation::SelectHomePod(Nav, Crust, CrustSurface, Pods[1].Entity, Pods, Selected), ELNPNavEndpointStatus::Reachable);
	TestEqual(TEXT("Existing parent retained"), Selected, 1);
	Pods.RemoveAt(1);
	TestEnum(*this, TEXT("No active pod in landing group becomes orphaned"),
		LNPEnemyNavigation::SelectHomePod(Nav, Crust, CrustSurface, Pods[0].Entity, Pods, Selected), ELNPNavEndpointStatus::Unreachable);
	TestEqual(TEXT("Orphan has no selected parent"), Selected, INDEX_NONE);
	FLNPSurfaceHandle StaleSurface = CrustSurface;
	++StaleSurface.Generation;
	TestEnum(*this, TEXT("Stale landing handle defers rehome"),
		LNPEnemyNavigation::SelectHomePod(Nav, Crust, StaleSurface, Pods[0].Entity, Pods, Selected), ELNPNavEndpointStatus::Stale);

	// 양방향 접근점 경로가 실제로 계산되고 마지막 node가 자기 group 내부에 남는다.
	for (int32 Direction = 0; Direction < 2; ++Direction)
	{
		FSchedulerCase Case;
		Case.Start = Direction == 0 ? Crust : Island;
		Case.StartHandle = Direction == 0 ? CrustSurface : IslandSurface;
		Case.Goal = Direction == 0 ? Island : Crust;
		Case.GoalHandle = Direction == 0 ? IslandSurface : CrustSurface;
		FLNPNavPathScheduler Scheduler;
		const uint32 Serial = Scheduler.Submit(MakeRequest(Case, 1, ELNPNavPathPriority::Background, 3000.0));
		DrainScheduler(Scheduler, Nav, nullptr);
		FLNPNavPathResult Result;
		TestEnum(*this, TEXT("Alert approach stays unreachable"), Scheduler.GetResult(Player, Serial, Result), ELNPNavPathStatus::Unreachable);
		if (TestTrue(TEXT("Alert approach has a path"), Result.Path.IsValid() && !Result.Path->Waypoints.IsEmpty()))
		{
			const int32 Last = Result.Path->Waypoints.Last().Node;
			TestEqual(TEXT("Approach stays in own group"), LNPNavGraph::GetGroup(Nav, Last),
				Direction == 0 ? CrustGroup.Group : IslandGroup.Group);
			TestEqual(TEXT("Approach stops inside six open grid edges"),
				static_cast<int32>(Nav.Graph.GetNode(Nav.Graph.GetSlot(Last), Last).GridDegree), 6);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavProductionPathTest,
	"LootNPop.SurfaceNavigation.Nav.ProductionPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavPodOverlayTest,
	"LootNPop.SurfaceNavigation.Nav.PodOverlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPNavGraphViewTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavPathfindingTest;
	FLNPSurfaceDataSnapshot Regression;
	if (BuildRegressionSnapshot(*this, Regression))
	{
		CheckGraphMatchesQuery(*this, Regression, TEXT("Regression"));
	}
	FLNPSurfaceDataSnapshot Production;
	double BuildMs = 0.0;
	if (BuildProductionSnapshot(*this, Production, BuildMs))
	{
		CheckGraphMatchesQuery(*this, Production, TEXT("Production"));
		AddInfo(FString::Printf(TEXT("Production snapshot build including Nav graph: %.1fms"), BuildMs));
	}
	return !HasAnyErrors();
}

bool FLNPNavRegressionPathTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavPathfindingTest;
	namespace Fixture = LNPRegressionFixture;

	FLNPSurfaceDataSnapshot Snapshot;
	if (!BuildRegressionSnapshot(*this, Snapshot))
	{
		return false;
	}
	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	const FLNPNavGraph& Graph = Nav.Graph;
	FLNPNavSearchScratch Scratch;
	const double R = Fixture::CrustRadius;
	const FQuat4d& Rotation = Snapshot.Slots[0].SlotRotation;
	auto World = [&Rotation](const FVector& Local) { return Rotation.RotateVector(FVector3d(Local)); };

	// 최적성: 같은 쌍을 휴리스틱 0(Dijkstra)으로 풀어 cost가 같아야 한다.
	auto CheckOptimal = [&](const TCHAR* Label, const int32 Start, const int32 Goal, const FPathResult& Path)
	{
		const FPathResult Dijkstra = FindPath(Nav, Start, Goal, Scratch, false);
		TestEnum(*this, FString::Printf(TEXT("%s Dijkstra also finds a path"), Label), Dijkstra.Status, ELNPNavSearchStatus::Found);
		TestTrue(FString::Printf(TEXT("%s A* cost %.3f equals Dijkstra %.3f"), Label, Path.Cost, Dijkstra.Cost),
			FMath::Abs(Path.Cost - Dijkstra.Cost) <= FMath::Max(0.5, Dijkstra.Cost * 1.e-4));
		TestTrue(FString::Printf(TEXT("%s heuristic expands no more than Dijkstra (%d <= %d)"), Label,
			Path.Expansions, Dijkstra.Expansions), Path.Expansions <= Dijkstra.Expansions);
	};

	// 1) 나무 우회: 나무 중심을 지나는 직선은 막히고, 경로는 나무 dilation 밖으로 돈다.
	const Fixture::FCaseFrame Props = Fixture::StaticProps();
	const FVector3d TreeStart = World(Props.At(R - FeetLift, Fixture::TreeTangent - 600.0));
	const FVector3d TreeGoal = World(Props.At(R - FeetLift, Fixture::TreeTangent + 500.0));
	const FVector3d TreeCenter = World(Props.At(R - FeetLift, Fixture::TreeTangent));
	const FLNPSurfaceHandle CrustHandle = FindHandle(Snapshot, TreeStart);
	const FLNPNavEndpoints TreeEnds = Resolve(Snapshot, TreeStart, CrustHandle, TreeGoal, FindHandle(Snapshot, TreeGoal));
	if (TestEnum(*this, TEXT("Tree pair resolves reachable"), TreeEnds.Status, ELNPNavEndpointStatus::Reachable))
	{
		TestFalse(TEXT("Straight walk through the tree is blocked"),
			LNPNavGraph::IsDirectWalkable(Nav, TreeEnds.StartNode, TreeEnds.GoalNode));
		const FPathResult Path = FindPath(Nav, TreeEnds.StartNode, TreeEnds.GoalNode, Scratch);
		if (TestEnum(*this, TEXT("Tree detour path is found"), Path.Status, ELNPNavSearchStatus::Found))
		{
			double Closest = TNumericLimits<double>::Max();
			for (const int32 Node : Path.Nodes)
			{
				Closest = FMath::Min(Closest, FVector3d::Dist(Graph.GetWorldPoint(0, Node), TreeCenter));
			}
			TestTrue(FString::Printf(TEXT("Tree detour keeps clear of the trunk (closest %.0fcm)"), Closest),
				Closest >= Fixture::TreeDiameter * 0.5 + 25.0);
			CheckOptimal(TEXT("Tree"), TreeEnds.StartNode, TreeEnds.GoalNode, Path);

			// 예산 분할 결정론: 7개씩 나눠 확장해도 같은 경로다.
			const FPathResult Split = FindPath(Nav, TreeEnds.StartNode, TreeEnds.GoalNode, Scratch, true, 7);
			TestTrue(TEXT("Split-budget search returns the same node path"), Split.Nodes == Path.Nodes);

			// 단순화: 끝점 유지, 인접 waypoint 사이가 모두 직선 보행 가능.
			TArray<int32> Waypoints;
			LNPNavPathfinding::SimplifyPath(Nav, Path.Nodes, Waypoints);
			bool bWaypointsWalkable = Waypoints.Num() >= 2 && Waypoints[0] == Path.Nodes[0] && Waypoints.Last() == Path.Nodes.Last();
			for (int32 Index = 1; Index < Waypoints.Num(); ++Index)
			{
				bWaypointsWalkable &= LNPNavGraph::IsDirectWalkable(Nav, Waypoints[Index - 1], Waypoints[Index]);
			}
			TestTrue(FString::Printf(TEXT("Tree waypoints (%d from %d nodes) are pairwise walkable"),
				Waypoints.Num(), Path.Nodes.Num()), bWaypointsWalkable);
			TestTrue(TEXT("Tree waypoints are fewer than nodes"), Waypoints.Num() < Path.Nodes.Num());
		}
	}

	// 2) 트인 지각: 직선 보행 검사가 통과한다.
	{
		const FVector3d Start = World(Fixture::BasicCrust().At(R - FeetLift, -800.0));
		const FVector3d Goal = World(Fixture::BasicCrust().At(R - FeetLift, 800.0, 300.0));
		const FLNPNavEndpoints Ends = Resolve(Snapshot, Start, FindHandle(Snapshot, Start), Goal, FindHandle(Snapshot, Goal));
		TestTrue(TEXT("Open crust pair is directly walkable"),
			Ends.Status == ELNPNavEndpointStatus::Reachable && LNPNavGraph::IsDirectWalkable(Nav, Ends.StartNode, Ends.GoalNode));
	}

	// 3) 동굴: 지각에서 공동 anchor까지 portal 두 개(지각↔통로↔공동)를 지난다.
	const FVector3d CaveStart = World(Fixture::Cave().At(R - FeetLift, -1500.0));
	const FLNPSpawnAuthoredAnchor* CaveAnchor = Snapshot.Slots[0].Spawn->AuthoredAnchors.FindByPredicate(
		[R](const FLNPSpawnAuthoredAnchor& Anchor) { return Anchor.LocalTransform.GetLocation().Length() > R + 100.0; });
	if (TestNotNull(TEXT("Slot 0 has a cave anchor"), CaveAnchor))
	{
		const FVector3d AnchorWorld = World(FVector(CaveAnchor->LocalTransform.GetLocation()));
		const FLNPSurfaceHandle AnchorHandle{0, CaveAnchor->LocalLayerId, Snapshot.Generation};
		const FLNPNavEndpoints Ends = Resolve(Snapshot, CaveStart, FindHandle(Snapshot, CaveStart), AnchorWorld, AnchorHandle);
		if (TestEnum(*this, TEXT("Cave anchor resolves reachable"), Ends.Status, ELNPNavEndpointStatus::Reachable))
		{
			const FPathResult Path = FindPath(Nav, Ends.StartNode, Ends.GoalNode, Scratch);
			if (TestEnum(*this, TEXT("Cave path is found"), Path.Status, ELNPNavSearchStatus::Found))
			{
				int32 LayerChanges = 0;
				for (int32 Index = 1; Index < Path.Nodes.Num(); ++Index)
				{
					LayerChanges += Graph.GetNode(0, Path.Nodes[Index]).LayerOrdinal
						!= Graph.GetNode(0, Path.Nodes[Index - 1]).LayerOrdinal ? 1 : 0;
				}
				TestEqual(TEXT("Cave path crosses crust->corridor->cavity portals"), LayerChanges, 2);
				CheckOptimal(TEXT("Cave"), Ends.StartNode, Ends.GoalNode, Path);
			}
		}
	}

	// 4) 이음매: 각 seam 중점 양쪽 slot 안쪽 1,500cm 사이 경로가 두 slot을 모두 지난다.
	TArray<FLNPCrustSeamPair> Pairs;
	FString Error;
	if (TestTrue(TEXT("Seam pairs compute"),
		LNPCrustAtlas::ComputeSeamPairs(MakeArrayView(ULNPOctantSpawnSubsystem::OctantRotations), Pairs, Error)))
	{
		const int32 N = Snapshot.Slots[0].Navigation->Layers[0].Subdivisions;
		int32 SeamPaths = 0;
		for (const FLNPCrustSeamPair& Pair : Pairs)
		{
			const FIntPoint Coord = LNPCrustAtlas::GetSeamSampleCoord(N, Pair.A.Edge, N / 2);
			const FVector3d Mid = Snapshot.Slots[Pair.A.Slot].SlotRotation.RotateVector(
				LNPSupportAtlas::GetSampleDirection(N, Coord.X, Coord.Y));
			auto Inward = [&](const int32 Slot)
			{
				const FVector3d Center = Snapshot.Slots[Slot].SlotRotation.RotateVector(FVector3d(1.0).GetSafeNormal());
				return (Mid + (Center - Mid).GetSafeNormal() * (1500.0 / R)).GetSafeNormal() * (R - FeetLift);
			};
			const FVector3d Start = Inward(Pair.A.Slot);
			const FVector3d Goal = Inward(Pair.B.Slot);
			const FLNPSurfaceHandle StartHandle{static_cast<uint16>(Pair.A.Slot), 0, Snapshot.Generation};
			const FLNPSurfaceHandle GoalHandle{static_cast<uint16>(Pair.B.Slot), 0, Snapshot.Generation};
			const FLNPNavEndpoints Ends = Resolve(Snapshot, Start, StartHandle, Goal, GoalHandle);
			const FPathResult Path = Ends.Status == ELNPNavEndpointStatus::Reachable
				? FindPath(Nav, Ends.StartNode, Ends.GoalNode, Scratch) : FPathResult();
			bool bCrossed = false;
			for (const int32 Node : Path.Nodes)
			{
				bCrossed |= Graph.GetSlot(Node) == Pair.B.Slot;
			}
			const bool bOk = Path.Status == ELNPNavSearchStatus::Found && Graph.GetSlot(Path.Nodes[0]) == Pair.A.Slot && bCrossed;
			SeamPaths += bOk ? 1 : 0;
			TestTrue(FString::Printf(TEXT("Seam %d:%d-%d:%d path crosses between slots"),
				Pair.A.Slot, static_cast<int32>(Pair.A.Edge), Pair.B.Slot, static_cast<int32>(Pair.B.Edge)), bOk);
			if (bOk && SeamPaths == 1)
			{
				CheckOptimal(TEXT("Seam"), Ends.StartNode, Ends.GoalNode, Path);
			}
		}
	}
	else
	{
		AddError(Error);
	}

	// 5) 끊긴 섬: 탐색 없이 도달 불가, 지각 쪽 접근점은 섬 아래 지각 내부 node(D-063).
	const FVector3d IslandTop = World(Fixture::IslandOne().At(Fixture::IslandOneTop - FeetLift));
	const FLNPSurfaceHandle IslandHandle = FindHandle(Snapshot, IslandTop);
	const FVector3d CrustNearIsland = World(Fixture::IslandOne().At(R - FeetLift, 1500.0));
	const FLNPSurfaceHandle CrustNearHandle = FindHandle(Snapshot, CrustNearIsland);
	if (TestTrue(TEXT("Island and nearby crust handles resolve"), IslandHandle.IsValid() && CrustNearHandle.IsValid()))
	{
		const FLNPNavEndpoints ToIsland = Resolve(Snapshot, CrustNearIsland, CrustNearHandle, IslandTop, IslandHandle);
		TestEnum(*this, TEXT("Crust to island is unreachable"), ToIsland.Status, ELNPNavEndpointStatus::Unreachable);
		TestTrue(TEXT("Crust to island has an approach node in the crust group"),
			ToIsland.ApproachNode != INDEX_NONE
			&& LNPNavGraph::GetGroup(Nav, ToIsland.ApproachNode) == LNPNavGraph::GetGroup(Nav, ToIsland.StartNode));
		if (ToIsland.ApproachNode != INDEX_NONE)
		{
			const int32 Slot = Graph.GetSlot(ToIsland.ApproachNode);
			TestEqual(TEXT("Approach node is interior"), static_cast<int32>(Graph.GetNode(Slot, ToIsland.ApproachNode).GridDegree), 6);
			TestTrue(FString::Printf(TEXT("Approach node lies under the island (%.0fcm from the top)"), ToIsland.GoalDistance),
				ToIsland.GoalDistance < (R - Fixture::IslandOneTop) + 300.0);
			const FPathResult Approach = FindPath(Nav, ToIsland.StartNode, ToIsland.ApproachNode, Scratch);
			TestEnum(*this, TEXT("Path to the approach node is found"), Approach.Status, ELNPNavSearchStatus::Found);
		}

		// 섬 node를 직접 넣어도 group 선판정으로 확장 0에 끝난다.
		TArray<FLNPNavGraphCandidate> IslandNodes;
		LNPNavGraph::CollectNodesNear(Nav, IslandHandle.OctantSlot, IslandHandle.LocalLayerId, IslandTop, 300.0, IslandNodes);
		if (TestFalse(TEXT("Island top has nodes"), IslandNodes.IsEmpty()) && ToIsland.StartNode != INDEX_NONE)
		{
			const FPathResult Direct = FindPath(Nav, ToIsland.StartNode, IslandNodes[0].Node, Scratch);
			TestTrue(TEXT("Island search is rejected without expansion"),
				Direct.Status == ELNPNavSearchStatus::Unreachable && Direct.Expansions == 0);
		}

		// 섬 위 적 → 지각 목표: 섬 가장자리 안쪽 내부 node로 접근한다.
		const FLNPNavEndpoints FromIsland = Resolve(Snapshot, IslandTop, IslandHandle, CrustNearIsland, CrustNearHandle);
		TestEnum(*this, TEXT("Island to crust is unreachable"), FromIsland.Status, ELNPNavEndpointStatus::Unreachable);
		if (TestTrue(TEXT("Island to crust has an island approach node"), FromIsland.ApproachNode != INDEX_NONE))
		{
			const int32 Slot = Graph.GetSlot(FromIsland.ApproachNode);
			TestTrue(TEXT("Island approach node is an interior island node"),
				Graph.GetNode(Slot, FromIsland.ApproachNode).GridDegree == 6
				&& LNPNavGraph::GetGroup(Nav, FromIsland.ApproachNode) == LNPNavGraph::GetGroup(Nav, IslandNodes.IsEmpty() ? INDEX_NONE : IslandNodes[0].Node));
		}
	}

	// 6) 실행 중 connectivity version이 바뀌면 결과를 섞지 않는다.
	if (TreeEnds.Status == ELNPNavEndpointStatus::Reachable)
	{
		FLNPNavSearch Search;
		LNPNavPathfinding::BeginSearch(Nav, TreeEnds.StartNode, TreeEnds.GoalNode, FLNPNavSearchParams(), Scratch, Search);
		LNPNavPathfinding::StepSearch(Nav, Scratch, Search, 3);
		FLNPNavSnapshot Changed = Nav;
		++Changed.ConnectivityGraphVersion;
		TestEnum(*this, TEXT("Search stops when ConnectivityGraphVersion changes"),
			LNPNavPathfinding::StepSearch(Changed, Scratch, Search, 1000), ELNPNavSearchStatus::Invalid);

		const FPathResult Capped = FindPath(Nav, TreeEnds.StartNode, TreeEnds.GoalNode, Scratch, true, MAX_int32, 2);
		TestEnum(*this, TEXT("Search over the expansion cap ends as NoPath"), Capped.Status, ELNPNavSearchStatus::NoPath);

		const FLNPSurfaceHandle StaleHandle{0, 0, Snapshot.Generation + 1};
		TestEnum(*this, TEXT("Stale handle resolves as Stale"),
			Resolve(Snapshot, TreeStart, StaleHandle, TreeGoal, CrustHandle).Status, ELNPNavEndpointStatus::Stale);
	}
	return !HasAnyErrors();
}

bool FLNPNavProductionPathTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavPathfindingTest;

	FLNPSurfaceDataSnapshot Snapshot;
	double BuildMs = 0.0;
	if (!BuildProductionSnapshot(*this, Snapshot, BuildMs))
	{
		return false;
	}
	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	const FLNPNavGraph& Graph = Nav.Graph;
	const uint32 MainGroup = FindLargestGroup(Nav);
	FLNPNavSearchScratch Scratch;

	// D-062: 주 group 옆 고립 node 위에 선 목표는 반경 안 주 group node로 다시 스냅된다.
	int32 IsolatedCases = 0;
	int32 Reselected = 0;
	int32 NoAlternative = 0;
	TArray<FLNPNavGraphCandidate> Nearby;
	for (int32 Node = 0; Node < Graph.GetNodeCount() && IsolatedCases < 40; ++Node)
	{
		const int32 Slot = Graph.GetSlot(Node);
		const FLNPNavGraphNode& GraphNode = Graph.GetNode(Slot, Node);
		if (GraphNode.GridDegree != 0 || Graph.BlockedNodes[Node] || Graph.HasExtraLinks[Node]
			|| LNPNavGraph::GetGroup(Nav, Node) == MainGroup)
		{
			continue;
		}
		const uint16 LayerId = Graph.SlotGraphs[Slot]->Layers[GraphNode.LayerOrdinal].LocalNavLayerId;
		const FVector3d Point = Graph.GetWorldPoint(Slot, Node);
		LNPNavGraph::CollectNodesNear(Nav, Slot, LayerId, Point, 300.0, Nearby);
		const FLNPNavGraphCandidate* Main = Nearby.FindByPredicate(
			[MainGroup](const FLNPNavGraphCandidate& Candidate) { return Candidate.Group == MainGroup; });
		if (Main == nullptr)
		{
			continue;
		}
		++IsolatedCases;
		// 시작은 같은 Layer 주 group node 중 반경 300cm 밖의 하나(거리 1,000cm 근처)다.
		const FVector3d Offset = FVector3d::CrossProduct(Point.GetSafeNormal(), FVector3d::UpVector).GetSafeNormal() * 1000.0;
		const FLNPSurfaceHandle Handle{static_cast<uint16>(Slot), LayerId, Snapshot.Generation};
		const FLNPNavEndpoints Ends = Resolve(Snapshot, Point + Offset, Handle, Point, Handle);
		if (Ends.Status == ELNPNavEndpointStatus::Reachable && Ends.bReselected
			&& LNPNavGraph::GetGroup(Nav, Ends.GoalNode) == LNPNavGraph::GetGroup(Nav, Ends.StartNode))
		{
			++Reselected;
		}
		else if (LNPNavGraph::GetGroup(Nav, Ends.StartNode) != MainGroup)
		{
			// 시작 쪽도 작은 component에 걸린 경우다. 이 sample은 판정하지 않는다.
			++NoAlternative;
		}
	}
	TestTrue(TEXT("Production has isolated nodes next to the main group"), IsolatedCases > 0);
	TestEqual(FString::Printf(TEXT("Isolated-node goals reselect to the main group (%d/%d, skipped %d)"),
		Reselected, IsolatedCases, NoAlternative), Reselected + NoAlternative, IsolatedCases);

	// 벤치마크: 주 group 지각 node에서 직선 20~80m 떨어진 결정론적 무작위 쌍. 판정 없이 분포만 기록한다.
	FRandomStream Random(20261001);
	TArray<double> ExpansionSamples;
	TArray<double> MicrosecondSamples;
	TArray<double> ResolveMicroseconds;
	TArray<double> WaypointCounts;
	int32 Pairs = 0;
	int32 Found = 0;
	int32 NoPath = 0;
	int32 Direct = 0;
	int32 Attempts = 0;
	double TotalExpansions = 0.0;
	double TotalSearchSeconds = 0.0;
	while (Pairs < 300 && Attempts < 5000)
	{
		++Attempts;
		const int32 Start = Random.RandRange(0, Graph.GetNodeCount() - 1);
		const int32 Slot = Graph.GetSlot(Start);
		const FLNPNavGraphNode& StartNode = Graph.GetNode(Slot, Start);
		if (StartNode.LayerOrdinal != 0 || Graph.BlockedNodes[Start] || LNPNavGraph::GetGroup(Nav, Start) != MainGroup)
		{
			continue;
		}
		const FVector3d StartPoint = Graph.GetWorldPoint(Slot, Start);
		const FVector3d Up = -StartPoint.GetSafeNormal();
		const FVector3d Tangent = FVector3d::CrossProduct(Up, FVector3d(Random.FRandRange(-1, 1), Random.FRandRange(-1, 1),
			Random.FRandRange(-1, 1))).GetSafeNormal();
		if (Tangent.IsNearlyZero())
		{
			continue;
		}
		const double Distance = Random.FRandRange(2000.0, 8000.0);
		const FVector3d GoalGuess = (StartPoint + Tangent * Distance).GetSafeNormal() * StartPoint.Length();
		const FLNPSurfaceHandle GoalHandle = FindHandle(Snapshot, GoalGuess + Up * FeetLift);
		if (!GoalHandle.IsValid())
		{
			continue;
		}
		const FLNPSurfaceHandle StartHandle{static_cast<uint16>(Slot), 0, Snapshot.Generation};
		const double ResolveStart = FPlatformTime::Seconds();
		const FLNPNavEndpoints Ends = Resolve(Snapshot, StartPoint, StartHandle, GoalGuess, GoalHandle);
		ResolveMicroseconds.Add((FPlatformTime::Seconds() - ResolveStart) * 1.e6);
		if (Ends.Status != ELNPNavEndpointStatus::Reachable)
		{
			continue;
		}
		++Pairs;
		Direct += LNPNavGraph::IsDirectWalkable(Nav, Ends.StartNode, Ends.GoalNode) ? 1 : 0;
		const double SearchStart = FPlatformTime::Seconds();
		const FPathResult Path = FindPath(Nav, Ends.StartNode, Ends.GoalNode, Scratch, true, MAX_int32, 30000);
		const double SearchSeconds = FPlatformTime::Seconds() - SearchStart;
		TotalSearchSeconds += SearchSeconds;
		TotalExpansions += Path.Expansions;
		ExpansionSamples.Add(Path.Expansions);
		MicrosecondSamples.Add(SearchSeconds * 1.e6);
		if (Path.Status == ELNPNavSearchStatus::Found)
		{
			++Found;
			TArray<int32> Waypoints;
			LNPNavPathfinding::SimplifyPath(Nav, Path.Nodes, Waypoints);
			WaypointCounts.Add(Waypoints.Num());
		}
		else
		{
			++NoPath;
		}
	}
	TestTrue(TEXT("Benchmark collected at least 200 reachable pairs"), Pairs >= 200);
	AddInfo(FString::Printf(
		TEXT("Production A* benchmark: pairs=%d found=%d noPath(cap 30000)=%d directWalkable=%d expansions P50=%.0f P95=%.0f max=%.0f time P50=%.1fus P95=%.1fus max=%.1fus perExpansion=%.3fus waypoints P50=%.0f P95=%.0f resolve P50=%.1fus P95=%.1fus"),
		Pairs, Found, NoPath, Direct,
		Percentile(ExpansionSamples, 0.5), Percentile(ExpansionSamples, 0.95), Percentile(ExpansionSamples, 1.0),
		Percentile(MicrosecondSamples, 0.5), Percentile(MicrosecondSamples, 0.95), Percentile(MicrosecondSamples, 1.0),
		TotalExpansions > 0.0 ? TotalSearchSeconds * 1.e6 / TotalExpansions : 0.0,
		Percentile(WaypointCounts, 0.5), Percentile(WaypointCounts, 0.95),
		Percentile(ResolveMicroseconds, 0.5), Percentile(ResolveMicroseconds, 0.95)));
	AddInfo(FString::Printf(TEXT("Production Nav graph resident=%.2fMiB scratch=%.2fMiB snapshotBuild=%.1fms"),
		Graph.GetAllocatedBytes() / (1024.0 * 1024.0), Scratch.GetAllocatedBytes() / (1024.0 * 1024.0), BuildMs));
	return !HasAnyErrors();
}

bool FLNPNavPathSchedulerTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavPathfindingTest;
	namespace Fixture = LNPRegressionFixture;

	FLNPSurfaceDataSnapshot Snapshot;
	if (!BuildRegressionSnapshot(*this, Snapshot))
	{
		return false;
	}
	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	const double R = Fixture::CrustRadius;
	const FQuat4d& Rotation = Snapshot.Slots[0].SlotRotation;
	auto World = [&Rotation](const FVector& Local) { return Rotation.RotateVector(FVector3d(Local)); };
	auto MakeCase = [&Snapshot](const TCHAR* Label, const FVector3d& Start, const FVector3d& Goal,
		const FLNPSurfaceHandle* GoalHandle = nullptr)
	{
		FSchedulerCase Case;
		Case.Label = Label;
		Case.Start = Start;
		Case.StartHandle = FindHandle(Snapshot, Start);
		Case.Goal = Goal;
		Case.GoalHandle = GoalHandle ? *GoalHandle : FindHandle(Snapshot, Goal);
		return Case;
	};

	// 고정 입력: 나무 우회, 동굴 portal 2회, 트인 지각 직선, 끊긴 섬(도달 불가·접근점).
	TArray<FSchedulerCase> Cases;
	Cases.Add(MakeCase(TEXT("Tree"), World(Fixture::StaticProps().At(R - FeetLift, Fixture::TreeTangent - 600.0)),
		World(Fixture::StaticProps().At(R - FeetLift, Fixture::TreeTangent + 500.0))));
	const FLNPSpawnAuthoredAnchor* CaveAnchor = Snapshot.Slots[0].Spawn->AuthoredAnchors.FindByPredicate(
		[R](const FLNPSpawnAuthoredAnchor& Anchor) { return Anchor.LocalTransform.GetLocation().Length() > R + 100.0; });
	if (!TestNotNull(TEXT("Slot 0 has a cave anchor"), CaveAnchor))
	{
		return false;
	}
	const FLNPSurfaceHandle CaveHandle{0, CaveAnchor->LocalLayerId, Snapshot.Generation};
	Cases.Add(MakeCase(TEXT("Cave"), World(Fixture::Cave().At(R - FeetLift, -1500.0)),
		World(FVector(CaveAnchor->LocalTransform.GetLocation())), &CaveHandle));
	Cases.Add(MakeCase(TEXT("OpenCrust"), World(Fixture::BasicCrust().At(R - FeetLift, -800.0)),
		World(Fixture::BasicCrust().At(R - FeetLift, 800.0, 300.0))));
	const FSchedulerCase Island = MakeCase(TEXT("Island"), World(Fixture::IslandOne().At(R - FeetLift, 1500.0)),
		World(Fixture::IslandOne().At(Fixture::IslandOneTop - FeetLift)));
	for (const FSchedulerCase& Case : Cases)
	{
		TestTrue(FString::Printf(TEXT("%s handles resolve"), Case.Label), Case.StartHandle.IsValid() && Case.GoalHandle.IsValid());
	}

	FLNPNavSearchScratch ReferenceScratch;
	TArray<TArray<int32>> References;
	for (const FSchedulerCase& Case : Cases)
	{
		References.Add(ReferenceWaypoints(Snapshot, Case, ReferenceScratch));
		TestTrue(FString::Printf(TEXT("%s reference path exists"), Case.Label), References.Last().Num() >= 2);
	}

	// 1) 예산 분할 결정론: 한 tick 전량, 병렬 소예산 다중 tick, 직렬 초소예산이 모두 직접 탐색과 같은 waypoint를 낸다.
	struct FBudgetMode
	{
		const TCHAR* Label;
		int32 Budget;
		int32 Scratches;
		bool bParallel;
	};
	const FBudgetMode Modes[] =
	{
		{TEXT("single tick"), 1000000, 4, true},
		{TEXT("parallel split"), 40, 2, true},
		{TEXT("serial split"), 7, 1, false},
	};
	for (const FBudgetMode& Mode : Modes)
	{
		FLNPNavPathScheduler Scheduler;
		Scheduler.Settings.ExpansionsPerFrame = Mode.Budget;
		Scheduler.Settings.ScratchCount = Mode.Scratches;
		Scheduler.Settings.bParallel = Mode.bParallel;
		Scheduler.Settings.CacheCapacity = 0;
		TArray<uint32> Serials;
		for (int32 Index = 0; Index < Cases.Num(); ++Index)
		{
			Serials.Add(Scheduler.Submit(MakeRequest(Cases[Index], Index + 1)));
		}
		const int32 Ticks = DrainScheduler(Scheduler, Nav, nullptr);
		int32 MaxRunningTicks = 0;
		for (int32 Index = 0; Index < Cases.Num(); ++Index)
		{
			FLNPNavPathResult Result;
			TestEnum(*this, FString::Printf(TEXT("%s %s succeeds"), Mode.Label, Cases[Index].Label),
				Scheduler.GetResult(FMassEntityHandle(Index + 1, 1), Serials[Index], Result), ELNPNavPathStatus::Succeeded);
			TestTrue(FString::Printf(TEXT("%s %s waypoints equal the direct search"), Mode.Label, Cases[Index].Label),
				WaypointNodes(Result) == References[Index]);
			MaxRunningTicks = FMath::Max(MaxRunningTicks, Result.RunningTicks);
		}
		const FLNPNavPathSchedulerStats& Stats = Scheduler.GetStats();
		AddInfo(FString::Printf(TEXT("Scheduler %s: ticks=%d maxRunningTicks=%d maxConcurrent=%d expansions=%llu"),
			Mode.Label, Ticks, MaxRunningTicks, Stats.MaxConcurrentRunning, Stats.Expansions));
		if (Mode.Budget < 1000)
		{
			TestTrue(FString::Printf(TEXT("%s spans several ticks"), Mode.Label), MaxRunningTicks > 1);
			TestEqual(FString::Printf(TEXT("%s runs up to the scratch count concurrently"), Mode.Label),
				Stats.MaxConcurrentRunning, Mode.Scratches);
		}
		else
		{
			TestEqual(TEXT("Single tick budget finishes everything in one tick"), Ticks, 1);
		}
	}

	// 2) 도달 불가: 접근점 반경이 없으면 확장 없이 끝나고, 있으면 지각 내부 접근점까지 경로가 온다(D-063).
	{
		FLNPNavPathScheduler Scheduler;
		const uint32 Plain = Scheduler.Submit(MakeRequest(Island, 1));
		const uint32 WithApproach = Scheduler.Submit(MakeRequest(Island, 2, ELNPNavPathPriority::Chase, 3000.0));
		DrainScheduler(Scheduler, Nav, nullptr);
		FLNPNavPathResult Result;
		TestEnum(*this, TEXT("Island without approach is Unreachable"),
			Scheduler.GetResult(FMassEntityHandle(1, 1), Plain, Result), ELNPNavPathStatus::Unreachable);
		TestTrue(TEXT("Island without approach has no path and no expansion"), !Result.Path.IsValid() && Result.Expansions == 0);
		TestEnum(*this, TEXT("Island with approach stays Unreachable"),
			Scheduler.GetResult(FMassEntityHandle(2, 1), WithApproach, Result), ELNPNavPathStatus::Unreachable);
		FLNPNavEndpointQuery Query;
		Query.StartPosition = Island.Start;
		Query.StartSurface = &Island.StartHandle;
		Query.GoalPosition = Island.Goal;
		Query.GoalSurface = &Island.GoalHandle;
		Query.ApproachRadius = 3000.0;
		const FLNPNavEndpoints Ends = LNPNavGraph::ResolveEndpoints(Nav, Query);
		TestTrue(TEXT("Island approach path ends at the approach node"),
			Result.Path.IsValid() && Ends.ApproachNode != INDEX_NONE && Result.Path->Waypoints.Last().Node == Ends.ApproachNode);
		TestEqual(TEXT("Approach paths are not cached"), Scheduler.GetCacheCount(), 0);
	}

	const FSchedulerCase& Tree = Cases[0];
	const FMassEntityHandle OwnerA(1, 1);
	const FMassEntityHandle OwnerB(2, 1);

	// 3) Stale: 실행 중 ConnectivityGraphVersion이나 overlay revision이 바뀌면 결과를 섞지 않는다. 옛 handle도 Stale이다.
	{
		FLNPNavPathScheduler Scheduler;
		Scheduler.Settings.ExpansionsPerFrame = 20;
		uint32 Serial = Scheduler.Submit(MakeRequest(Tree, 1));
		Scheduler.Tick(Nav, nullptr, [](FMassEntityHandle) { return true; });
		FLNPNavPathResult Result;
		TestEnum(*this, TEXT("Small budget leaves the tree request running"),
			Scheduler.GetResult(OwnerA, Serial, Result), ELNPNavPathStatus::Running);
		FLNPNavSnapshot Changed = Nav;
		++Changed.ConnectivityGraphVersion;
		Scheduler.Tick(Changed, nullptr, [](FMassEntityHandle) { return true; });
		TestEnum(*this, TEXT("ConnectivityGraphVersion change ends the request as Stale"),
			Scheduler.GetResult(OwnerA, Serial, Result), ELNPNavPathStatus::Stale);
		TestEqual(TEXT("Stale request releases its scratch"), Scheduler.GetRunningCount(), 0);

		Serial = Scheduler.Submit(MakeRequest(Tree, 1));
		FLNPNavOverlay Overlay;
		Scheduler.Tick(Nav, &Overlay, [](FMassEntityHandle) { return true; });
		Overlay.Revision = 1;
		Scheduler.Tick(Nav, &Overlay, [](FMassEntityHandle) { return true; });
		TestEnum(*this, TEXT("Overlay revision change ends the request as Stale"),
			Scheduler.GetResult(OwnerA, Serial, Result), ELNPNavPathStatus::Stale);

		FSchedulerCase OldHandle = Tree;
		++OldHandle.StartHandle.Generation;
		Serial = Scheduler.Submit(MakeRequest(OldHandle, 1));
		DrainScheduler(Scheduler, Nav, nullptr);
		TestEnum(*this, TEXT("Old snapshot handle is Stale"), Scheduler.GetResult(OwnerA, Serial, Result), ELNPNavPathStatus::Stale);
	}

	// 4) Cancelled: 재요청은 옛 serial을 버리고, 취소·owner 소멸은 기록과 scratch를 정리한다. 상한 초과는 NoPath다.
	{
		FLNPNavPathScheduler Scheduler;
		Scheduler.Settings.ExpansionsPerFrame = 20;
		const uint32 First = Scheduler.Submit(MakeRequest(Tree, 1));
		Scheduler.Tick(Nav, nullptr, [](FMassEntityHandle) { return true; });
		const uint32 Second = Scheduler.Submit(MakeRequest(Tree, 1));
		FLNPNavPathResult Result;
		TestEnum(*this, TEXT("Resubmission cancels the older serial"),
			Scheduler.GetResult(OwnerA, First, Result), ELNPNavPathStatus::Cancelled);
		TestEqual(TEXT("Resubmission releases the running scratch"), Scheduler.GetRunningCount(), 0);
		DrainScheduler(Scheduler, Nav, nullptr);
		TestEnum(*this, TEXT("Newer serial completes"), Scheduler.GetResult(OwnerA, Second, Result), ELNPNavPathStatus::Succeeded);
		TestTrue(TEXT("Newer serial equals the direct search"), WaypointNodes(Result) == References[0]);

		// cache를 비워 다음 요청이 cache hit로 바로 끝나지 않게 한다.
		Scheduler.Reset();
		const uint32 Cancelled = Scheduler.Submit(MakeRequest(Tree, 1));
		Scheduler.Tick(Nav, nullptr, [](FMassEntityHandle) { return true; });
		TestEnum(*this, TEXT("Request is running before Cancel"),
			Scheduler.GetResult(OwnerA, Cancelled, Result), ELNPNavPathStatus::Running);
		Scheduler.Cancel(OwnerA);
		TestEnum(*this, TEXT("Cancel drops the record"), Scheduler.GetResult(OwnerA, Cancelled, Result), ELNPNavPathStatus::None);
		TestTrue(TEXT("Cancel leaves no work"), !Scheduler.HasWork());

		const uint64 CancelledBefore = Scheduler.GetStats().Finished[static_cast<int32>(ELNPNavPathStatus::Cancelled)];
		const uint32 Orphan = Scheduler.Submit(MakeRequest(Tree, 2));
		Scheduler.Tick(Nav, nullptr, [OwnerB](const FMassEntityHandle Owner) { return Owner != OwnerB; });
		TestEnum(*this, TEXT("Vanished owner's request is dropped"), Scheduler.GetResult(OwnerB, Orphan, Result), ELNPNavPathStatus::None);
		TestEqual(TEXT("Vanished owner counts as Cancelled"),
			Scheduler.GetStats().Finished[static_cast<int32>(ELNPNavPathStatus::Cancelled)], CancelledBefore + 1);

		Scheduler.Settings.ExpansionsPerFrame = 4000;
		Scheduler.Settings.MaxExpansionsPerRequest = 2;
		const uint32 Capped = Scheduler.Submit(MakeRequest(Tree, 1));
		DrainScheduler(Scheduler, Nav, nullptr);
		TestEnum(*this, TEXT("Request over the expansion cap ends as NoPath"),
			Scheduler.GetResult(OwnerA, Capped, Result), ELNPNavPathStatus::NoPath);
	}

	// 5) 우선순위: scratch 하나에서 나중에 온 추격 요청이 먼저 온 배회 요청보다 먼저 시작한다.
	{
		FLNPNavPathScheduler Scheduler;
		Scheduler.Settings.ExpansionsPerFrame = 20;
		Scheduler.Settings.ScratchCount = 1;
		const uint32 Wander = Scheduler.Submit(MakeRequest(Cases[1], 1, ELNPNavPathPriority::Background));
		const uint32 Chase = Scheduler.Submit(MakeRequest(Tree, 2, ELNPNavPathPriority::Chase));
		Scheduler.Tick(Nav, nullptr, [](FMassEntityHandle) { return true; });
		FLNPNavPathResult Result;
		TestEnum(*this, TEXT("Background request waits behind the chase request"),
			Scheduler.GetResult(OwnerA, Wander, Result), ELNPNavPathStatus::Queued);
		TestTrue(TEXT("Chase request started first"), Scheduler.GetResult(OwnerB, Chase, Result) != ELNPNavPathStatus::Queued);
		DrainScheduler(Scheduler, Nav, nullptr);
		TestEnum(*this, TEXT("Background request completes afterwards"),
			Scheduler.GetResult(OwnerA, Wander, Result), ELNPNavPathStatus::Succeeded);
	}

	// 6) cache: 같은 Tile 쌍은 공유 경로를 확장 없이 받는다. 지나는 Tile의 revision이 바뀌면 다시 계산하고, 무관한 Tile은 영향이 없다.
	{
		FLNPNavPathScheduler Scheduler;
		auto RunOne = [&](const int32 OwnerIndex, const FLNPNavOverlay* Overlay, FLNPNavPathResult& OutResult)
		{
			const uint32 Serial = Scheduler.Submit(MakeRequest(Tree, OwnerIndex));
			DrainScheduler(Scheduler, Nav, Overlay);
			return Scheduler.GetResult(FMassEntityHandle(OwnerIndex, 1), Serial, OutResult);
		};
		FLNPNavPathResult First;
		FLNPNavPathResult Shared;
		RunOne(1, nullptr, First);
		RunOne(2, nullptr, Shared);
		TestTrue(TEXT("First tree request is computed"), First.Path.IsValid() && !First.bFromCache && First.Expansions > 0);
		TestTrue(TEXT("Second tree request shares the cached path without expansion"),
			Shared.bFromCache && Shared.Path == First.Path && Shared.Expansions == 0);
		if (!TestTrue(TEXT("Path fingerprint lists traversed tiles"),
			First.Path.IsValid() && !First.Path->TraversedTiles.IsEmpty()
			&& First.Path->TraversedTiles.Num() == First.Path->TileRevisions.Num()))
		{
			return false;
		}

		FLNPNavOverlay Touched;
		Touched.Revision = 1;
		Touched.TileRevisions.Add(First.Path->TraversedTiles[0], 1);
		FLNPNavPathResult Recomputed;
		RunOne(3, &Touched, Recomputed);
		TestTrue(TEXT("Traversed tile revision change forces recomputation"),
			!Recomputed.bFromCache && Recomputed.Expansions > 0 && Scheduler.GetStats().CacheRejects == 1);
		TestFalse(TEXT("Old path is no longer current under the new overlay"), First.Path->IsCurrent(Nav, &Touched));

		FLNPNavOverlay Unrelated = Touched;
		Unrelated.Revision = 2;
		Unrelated.TileRevisions.Add(MAX_uint32 - 1, 1);
		FLNPNavPathResult Hit;
		RunOne(4, &Unrelated, Hit);
		TestTrue(TEXT("Unrelated tile revision keeps the cache hit"), Hit.bFromCache && Hit.Path == Recomputed.Path);
	}
	return !HasAnyErrors();
}

bool FLNPNavProductionSchedulerTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavPathfindingTest;

	FLNPSurfaceDataSnapshot Snapshot;
	double BuildMs = 0.0;
	if (!BuildProductionSnapshot(*this, Snapshot, BuildMs))
	{
		return false;
	}
	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	TArray<FSchedulerCase> Cases;
	CollectProductionPairs(Snapshot, FindLargestGroup(Nav), 300, 20261002, Cases);
	TestTrue(FString::Printf(TEXT("Collected production pairs (%d)"), Cases.Num()), Cases.Num() >= 200);

	FLNPNavSearchScratch ReferenceScratch;
	TArray<TArray<int32>> References;
	for (const FSchedulerCase& Case : Cases)
	{
		References.Add(ReferenceWaypoints(Snapshot, Case, ReferenceScratch));
	}

	// 모든 요청을 한 프레임에 넣고 기본 예산으로 처리한다. cache는 끄고 직접 탐색과의 일치와 프레임당 시간을 본다.
	for (const bool bParallel : {true, false})
	{
		FLNPNavPathScheduler Scheduler;
		Scheduler.Settings.CacheCapacity = 0;
		Scheduler.Settings.bParallel = bParallel;
		TArray<uint32> Serials;
		for (int32 Index = 0; Index < Cases.Num(); ++Index)
		{
			Serials.Add(Scheduler.Submit(MakeRequest(Cases[Index], Index + 1)));
		}
		TArray<double> TickMicroseconds;
		const int32 Ticks = DrainScheduler(Scheduler, Nav, nullptr, &TickMicroseconds);
		int32 Mismatches = 0;
		int32 Succeeded = 0;
		TArray<double> RunningTicks;
		for (int32 Index = 0; Index < Cases.Num(); ++Index)
		{
			FLNPNavPathResult Result;
			Succeeded += Scheduler.GetResult(FMassEntityHandle(Index + 1, 1), Serials[Index], Result) == ELNPNavPathStatus::Succeeded ? 1 : 0;
			Mismatches += WaypointNodes(Result) != References[Index] ? 1 : 0;
			RunningTicks.Add(Result.RunningTicks);
		}
		const TCHAR* Label = bParallel ? TEXT("parallel") : TEXT("serial");
		TestEqual(FString::Printf(TEXT("Production %s scheduler results equal the direct search"), Label), Mismatches, 0);
		TestEqual(FString::Printf(TEXT("Production %s scheduler requests all succeed"), Label), Succeeded, Cases.Num());
		const FLNPNavPathSchedulerStats& Stats = Scheduler.GetStats();
		AddInfo(FString::Printf(
			TEXT("Production scheduler %s: requests=%d ticks=%d budget=%d scratches=%d expansions=%llu tick P50=%.1fus P95=%.1fus max=%.1fus requestTicks P50=%.0f P95=%.0f max=%.0f scratch=%.2fMiB"),
			Label, Cases.Num(), Ticks, Scheduler.Settings.ExpansionsPerFrame, Scheduler.Settings.ScratchCount, Stats.Expansions,
			Percentile(TickMicroseconds, 0.5), Percentile(TickMicroseconds, 0.95), Percentile(TickMicroseconds, 1.0),
			Percentile(RunningTicks, 0.5), Percentile(RunningTicks, 0.95), Percentile(RunningTicks, 1.0),
			Scheduler.GetScratchBytes() / (1024.0 * 1024.0)));
	}
	return !HasAnyErrors();
}

bool FLNPNavRequestCostReplayTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavPathfindingTest;
	FString InputPath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("LNPNavReplayCsv="), InputPath))
	{
		AddInfo(TEXT("Request cost replay skipped: supply -LNPNavReplayCsv=<capture.csv>."));
		return true;
	}
	FString OutputPath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("LNPNavReplayOutput="), OutputPath))
	{
		AddError(TEXT("Supply -LNPNavReplayOutput=<tick-cost.csv>."));
		return false;
	}
	TArray<FString> Lines;
	if (!TestTrue(TEXT("Request CSV loads"), FFileHelper::LoadFileToStringArray(Lines, *InputPath)))
	{
		return false;
	}
	const FString Header = TEXT("time,owner,ownerSerial,requestSerial,priority,startX,startY,startZ,startSlot,startLayer,startGeneration,goalX,goalY,goalZ,goalSlot,goalLayer,goalGeneration,snapRadius,approachRadius,graphVersion,overlayRevision");
	if (!TestTrue(TEXT("Capture schema and rows exist"), Lines.Num() > 1 && Lines[0] == Header))
	{
		return false;
	}
	FLNPSurfaceDataSnapshot Snapshot;
	double BuildMs = 0.0;
	if (!BuildProductionSnapshot(*this, Snapshot, BuildMs))
	{
		return false;
	}
	TArray<FLNPNavPathRequest> Requests;
	TArray<double> Times;
	for (int32 Row = 1; Row < Lines.Num(); ++Row)
	{
		TArray<FString> Fields;
		Lines[Row].ParseIntoArray(Fields, TEXT(","), false);
		double Values[21] = {};
		bool bValid = Fields.Num() == UE_ARRAY_COUNT(Values);
		for (int32 Column = 0; bValid && Column < Fields.Num(); ++Column)
		{
			bValid = LexTryParseString(Values[Column], *Fields[Column]) && FMath::IsFinite(Values[Column]);
		}
		bValid = bValid && Values[0] >= 0.0 && (Times.IsEmpty() || Values[0] >= Times.Last())
			&& Values[4] >= 0 && Values[4] <= 2 && Values[4] == FMath::FloorToDouble(Values[4])
			&& Values[10] == Snapshot.Generation && Values[16] == Snapshot.Generation
			&& Values[19] == Snapshot.Nav.ConnectivityGraphVersion && Values[17] > 0 && Values[18] >= 0;
		for (const int32 Column : {8, 9, 14, 15})
		{
			bValid = bValid && Values[Column] >= 0 && Values[Column] <= MAX_uint16
				&& Values[Column] == FMath::FloorToDouble(Values[Column]);
		}
		if (!TestTrue(FString::Printf(TEXT("CSV row %d is compatible with this snapshot"), Row), bValid))
		{
			return false;
		}
		FLNPNavPathRequest& Request = Requests.AddDefaulted_GetRef();
		// 취소 시점 차이를 없애고 모든 CSV 요청의 비용을 비교한다. 원래 owner·serial은 재생하지 않는다.
		Request.Owner = FMassEntityHandle(Row, 1);
		Request.Priority = static_cast<ELNPNavPathPriority>(static_cast<int32>(Values[4]));
		Request.StartPosition = FVector3d(Values[5], Values[6], Values[7]);
		Request.StartSurface = {static_cast<uint16>(Values[8]), static_cast<uint16>(Values[9]), Snapshot.Generation};
		Request.GoalPosition = FVector3d(Values[11], Values[12], Values[13]);
		Request.GoalSurface = {static_cast<uint16>(Values[14]), static_cast<uint16>(Values[15]), Snapshot.Generation};
		Request.SnapRadius = Values[17];
		Request.ApproachRadius = Values[18];
		Times.Add(Values[0]);
	}
	AddInfo(FString::Printf(TEXT("Request cost replay: rows=%d input=%s; timestamp batches drained independently, unique owners, no Pod overlay, empty initial cache; not a gameplay Gate."), Requests.Num(), *InputPath));

	struct FReference
	{
		ELNPNavPathStatus Status;
		TArray<int32> Waypoints;
		int32 Expansions;
		double Cost;
	};
	TArray<FReference> References;
	FString Output = TEXT("variant,batch,tick,started,expansions,queued,running,tickUs,startUs,stepUs,finishUs,workerSumUs\n");
	for (int32 Variant = 0; Variant < 5; ++Variant)
	{
		FLNPNavPathScheduler Scheduler;
		Scheduler.Settings.CacheCapacity = Variant == 4 ? 256 : 0;
		Scheduler.Settings.ExpansionsPerFrame = Variant == 1 ? 2000 : 4000;
		Scheduler.Settings.ResolveCostInExpansions = Variant == 2 ? 64 : 16;
		Scheduler.Settings.bParallel = Variant != 3;
		const TCHAR* Labels[] = {TEXT("base"), TEXT("budget2000"), TEXT("resolve64"), TEXT("serial"), TEXT("cache256")};
		TArray<double> TickUs, StartUs, StepUs, FinishUs, QueueSamples;
		double TotalStart = 0, TotalStep = 0, TotalFinish = 0, TotalTick = 0;
		int32 Batch = 0, Mismatches = 0, MaxCharged = 0;
		for (int32 First = 0; First < Requests.Num(); ++Batch)
		{
			int32 End = First + 1;
			while (End < Requests.Num() && Times[End] == Times[First]) { ++End; }
			TArray<uint32> Serials;
			for (int32 Index = First; Index < End; ++Index) { Serials.Add(Scheduler.Submit(Requests[Index])); }
			int32 Tick = 0;
			while (Scheduler.HasWork() && Tick < 10000)
			{
				const double Begin = FPlatformTime::Seconds();
				Scheduler.Tick(Snapshot.Nav, nullptr, [](FMassEntityHandle) { return true; });
				const double Elapsed = (FPlatformTime::Seconds() - Begin) * 1.e6;
				const auto& Stats = Scheduler.GetStats();
				const double Start = Stats.LastTickStartSeconds * 1.e6;
				const double Step = Stats.LastTickStepSeconds * 1.e6;
				const double Finish = Stats.LastTickFinishSeconds * 1.e6;
				TickUs.Add(Elapsed); StartUs.Add(Start); StepUs.Add(Step); FinishUs.Add(Finish);
				QueueSamples.Add(Scheduler.GetQueuedCount());
				TotalTick += Elapsed; TotalStart += Start; TotalStep += Step; TotalFinish += Finish;
				MaxCharged = FMath::Max(MaxCharged, Stats.LastTickExpansions + Stats.LastTickStarted * Scheduler.Settings.ResolveCostInExpansions);
				Output += FString::Printf(TEXT("%s,%d,%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f\n"), Labels[Variant], Batch, Tick++,
					Stats.LastTickStarted, Stats.LastTickExpansions, Scheduler.GetQueuedCount(), Scheduler.GetRunningCount(),
					Elapsed, Start, Step, Finish, Stats.LastTickSearchSeconds * 1.e6);
			}
			if (!TestTrue(TEXT("Replay batch drains"), !Scheduler.HasWork())) { return false; }
			for (int32 Index = First; Index < End; ++Index)
			{
				FLNPNavPathResult Result;
				Scheduler.GetResult(Requests[Index].Owner, Serials[Index - First], Result);
				const double Cost = Result.Path.IsValid() ? Result.Path->Cost : 0.0;
				if (Variant == 0) { References.Add({Result.Status, WaypointNodes(Result), Result.Expansions, Cost}); }
				else if (Variant != 4)
				{
					const FReference& Reference = References[Index];
					Mismatches += Result.Status != Reference.Status || WaypointNodes(Result) != Reference.Waypoints
						|| Result.Expansions != Reference.Expansions || !FMath::IsNearlyEqual(Cost, Reference.Cost, 1.e-6) ? 1 : 0;
				}
				// 완료된 owner가 누적되어 관리 비용을 왜곡하지 않도록 제거한다. cache는 유지한다.
				Scheduler.Cancel(Requests[Index].Owner);
			}
			First = End;
		}
		TestEqual(TEXT("Uncached variants preserve statuses, waypoints, expansions and costs"), Mismatches, 0);
		const auto& Stats = Scheduler.GetStats();
		AddInfo(FString::Printf(TEXT("Replay %s: batches=%d ticks=%d requests=%llu expansions=%llu hits=%llu mismatches=%d maxCharged=%d budget=%d; tick P50/P95/max=%.1f/%.1f/%.1fus startP95=%.1f stepP95=%.1f finishP95=%.1f queuedP95=%.0f; total tick/start/step/finish=%.1f/%.1f/%.1f/%.1fms"),
			Labels[Variant], Batch, TickUs.Num(), Stats.Submitted, Stats.Expansions, Stats.CacheHits, Mismatches, MaxCharged,
			Scheduler.Settings.ExpansionsPerFrame, Percentile(TickUs, .5), Percentile(TickUs, .95), Percentile(TickUs, 1),
			Percentile(StartUs, .95), Percentile(StepUs, .95), Percentile(FinishUs, .95), Percentile(QueueSamples, .95),
			TotalTick / 1000, TotalStart / 1000, TotalStep / 1000, TotalFinish / 1000));
	}
	TestTrue(TEXT("Replay tick costs saved"), FFileHelper::SaveStringToFile(Output, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	return !HasAnyErrors();
}

bool FLNPNavPodOverlayTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavPathfindingTest;
	FLNPSurfaceDataSnapshot Snapshot;
	double BuildMs = 0.0;
	if (!BuildProductionSnapshot(*this, Snapshot, BuildMs))
	{
		return false;
	}
	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	const FLNPNavGraph& Graph = Nav.Graph;
	const FLNPNavAssetGraph& Asset = *Graph.SlotGraphs[0];
	const FLNPNavLayer* SurfaceLayer = LNPNavData::FindLayer(*Snapshot.Slots[0].Navigation,
		Asset.Layers[0].LocalNavLayerId);
	if (!TestTrue(TEXT("First Nav layer has a Support layer"), SurfaceLayer != nullptr))
	{
		return false;
	}
	FLNPSurfaceHandle Handle;
	Handle.OctantSlot = 0;
	Handle.LocalLayerId = SurfaceLayer->LocalSupportLayerId;
	Handle.Generation = Snapshot.Generation;
	const int32 Base = Graph.SlotNodeBase[0];
	int32 Center = INDEX_NONE;
	int32 Start = INDEX_NONE;
	int32 Goal = INDEX_NONE;
	FLNPNavOverlay Blocked;
	for (int32 Local = Asset.Layers[0].NodeBegin; Local < Asset.Layers[0].NodeEnd; ++Local)
	{
		const FLNPNavGraphNode& Node = Asset.Nodes[Local];
		if (Node.GridDegree != 6 || Node.Neighbors[0] == INDEX_NONE || Node.Neighbors[1] == INDEX_NONE)
		{
			continue;
		}
		const int32 Candidate = Base + Local;
		const FVector3d Position = Graph.GetWorldPoint(0, Candidate);
		const FLNPNavPodBlocker Pod{1, Position, Handle};
		if (!LNPNavOverlay::BuildPodOverlay(Snapshot, MakeArrayView(&Pod, 1), nullptr, Blocked))
		{
			continue;
		}
		const int32 A = Base + Node.Neighbors[0];
		const int32 B = Base + Node.Neighbors[1];
		if (!Blocked.IsBlocked(A) && !Blocked.IsBlocked(B)
			&& LNPNavGraph::IsDirectWalkable(Nav, A, B)
			&& !LNPNavGraph::IsDirectWalkable(Nav, A, B, &Blocked))
		{
			Center = Candidate;
			Start = A;
			Goal = B;
			break;
		}
	}
	if (!TestTrue(TEXT("Pod blocks a direct walk while leaving both endpoints open"), Center != INDEX_NONE))
	{
		return false;
	}
	TestTrue(TEXT("Pod center is blocked"), Blocked.IsBlocked(Center));
	const uint32 Tile = LNPNavGraph::GetTileKey(Nav, Center);
	TestEqual(TEXT("Pod tile revision after spawn"), Blocked.GetTileRevision(Tile), 1u);
	FLNPNavSearchScratch Scratch;
	FLNPNavSearch Search;
	FLNPNavSearchParams Params;
	LNPNavPathfinding::BeginSearch(Nav, Start, Goal, Params, Scratch, Search, &Blocked);
	while (Search.Status == ELNPNavSearchStatus::Running)
	{
		LNPNavPathfinding::StepSearch(Nav, Scratch, Search, 4000, &Blocked);
	}
	TestEnum(*this, TEXT("A* routes around the Pod"), Search.Status, ELNPNavSearchStatus::Found);
	TArray<int32> Nodes;
	if (TestTrue(TEXT("Detour path extracts"), LNPNavPathfinding::ExtractNodePath(Scratch, Search, Nodes)))
	{
		TestTrue(TEXT("Detour excludes blocked center"), !Nodes.Contains(Center));
	}
	FLNPNavOverlay Cleared;
	TestTrue(TEXT("Popped Pod changes the overlay"), LNPNavOverlay::BuildPodOverlay(Snapshot, {}, &Blocked, Cleared));
	TestTrue(TEXT("Popped Pod clears the center"), !Cleared.IsBlocked(Center));
	TestEqual(TEXT("Popped Pod advances the tile revision"), Cleared.GetTileRevision(Tile), 2u);
	TestTrue(TEXT("Direct walk returns after Pod removal"), LNPNavGraph::IsDirectWalkable(Nav, Start, Goal, &Cleared));
	FSchedulerCase Case;
	Case.Start = Graph.GetWorldPoint(0, Start);
	Case.StartHandle = Handle;
	Case.Goal = Graph.GetWorldPoint(0, Goal);
	Case.GoalHandle = Handle;
	FLNPNavPathScheduler Scheduler;
	const uint32 Serial = Scheduler.Submit(MakeRequest(Case, 1));
	DrainScheduler(Scheduler, Nav, &Blocked);
	FLNPNavPathResult Result;
	TestEnum(*this, TEXT("Blocked route is planned"),
		Scheduler.GetResult(FMassEntityHandle(1, 1), Serial, Result), ELNPNavPathStatus::Succeeded);
	TestEqual(TEXT("Pod removal queues the affected path"), Scheduler.RequeueInvalidatedPaths(Nav, &Cleared), 1);
	TestEnum(*this, TEXT("Same serial waits for replanning"),
		Scheduler.GetResult(FMassEntityHandle(1, 1), Serial, Result), ELNPNavPathStatus::Queued);
	DrainScheduler(Scheduler, Nav, &Cleared);
	TestEnum(*this, TEXT("Popped Pod route is replanned"),
		Scheduler.GetResult(FMassEntityHandle(1, 1), Serial, Result), ELNPNavPathStatus::Succeeded);
	TestEqual(TEXT("Unchanged overlay queues nothing"), Scheduler.RequeueInvalidatedPaths(Nav, &Cleared), 0);

	if (!Nav.SeamLinks.IsEmpty())
	{
		const FLNPNavSeamLink& Link = Nav.SeamLinks[0];
		const int32 A = LNPNavGraph::ToGraphNode(Nav, Link.A);
		const int32 B = LNPNavGraph::ToGraphNode(Nav, Link.B);
		if (!TestTrue(TEXT("Seam endpoints resolve"), A != INDEX_NONE && B != INDEX_NONE))
		{
			return false;
		}
		const int32 Slot = Graph.GetSlot(A);
		const FLNPNavGraphNode& SeamNode = Graph.GetNode(Slot, A);
		const uint16 LayerId = Graph.SlotGraphs[Slot]->Layers[SeamNode.LayerOrdinal].LocalNavLayerId;
		const FLNPNavLayer* SeamLayer = LNPNavData::FindLayer(*Snapshot.Slots[Slot].Navigation, LayerId);
		if (TestTrue(TEXT("Seam Nav layer has a Support layer"), SeamLayer != nullptr))
		{
			FLNPSurfaceHandle SeamHandle{static_cast<uint16>(Slot), SeamLayer->LocalSupportLayerId, Snapshot.Generation};
			const FLNPNavPodBlocker Pod{2, Graph.GetWorldPoint(Slot, A), SeamHandle};
			FLNPNavOverlay SeamOverlay;
			TestTrue(TEXT("Seam Pod publishes an overlay"),
				LNPNavOverlay::BuildPodOverlay(Snapshot, MakeArrayView(&Pod, 1), nullptr, SeamOverlay));
			TestTrue(TEXT("Both seam copies are blocked"), SeamOverlay.IsBlocked(A) && SeamOverlay.IsBlocked(B));
		}
	}
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
