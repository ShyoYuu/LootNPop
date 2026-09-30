// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Config/LNPSettings.h"
#include "DataAsset/LNPOctantPoolData.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "SurfaceNavigation/LNPCrustAtlas.h"
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
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavGraphViewTest,
	"LootNPop.SurfaceNavigation.Nav.GraphView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavRegressionPathTest,
	"LootNPop.SurfaceNavigation.Nav.RegressionPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavProductionPathTest,
	"LootNPop.SurfaceNavigation.Nav.ProductionPath",
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

#endif // WITH_DEV_AUTOMATION_TESTS
