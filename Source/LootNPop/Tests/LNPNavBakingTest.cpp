// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurfaceNavigation/LNPNavBaking.h"
#include "SurfaceNavigation/LNPNavRuntime.h"
#include "SurfaceNavigation/LNPNavPathfinding.h"

namespace LNPNavBakingTest
{
	FLNPSupportAtlasLayer MakeFullLayer(const int32 Subdivisions, const double Radius)
	{
		FLNPSupportAtlasLayer Layer;
		Layer.Layout = FLNPSupportLayout::MakeFull(Subdivisions);
		Layer.SourceIndex = 0;
		Layer.BaseRadius = Radius;
		Layer.RadiusStep = 0.25;
		Layer.RadiusQ.Init(0, Layer.Layout.Num());
		Layer.NormalQ.SetNumZeroed(Layer.Layout.Num() * 2);
		Layer.Flags.Init(static_cast<uint8>(
			ELNPSupportSampleFlags::Valid | ELNPSupportSampleFlags::Walkable), Layer.Layout.Num());
		for (int32 J = 0; J <= Subdivisions; ++J)
		{
			for (int32 I = 0; I + J <= Subdivisions; ++I)
			{
				const int32 Index = Layer.Layout.Find(I, J);
				const FVector3f Normal(-LNPSupportAtlas::GetSampleDirection(Subdivisions, I, J));
				LNPSupportAtlas::EncodeNormal(Normal, Layer.NormalQ[Index * 2], Layer.NormalQ[Index * 2 + 1]);
			}
		}
		return Layer;
	}

	FIntPoint PositionToCoord(const FVector3d& Position, const int32 Subdivisions)
	{
		const FVector3d Direction = Position.GetSafeNormal();
		const double Sum = Direction.X + Direction.Y + Direction.Z;
		return FIntPoint(
			FMath::RoundToInt32(Direction.X / Sum * Subdivisions),
			FMath::RoundToInt32(Direction.Y / Sum * Subdivisions));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavBakingTest,
	"LootNPop.SurfaceNavigation.Nav.Baking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPNavBakingTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavBakingTest;
	constexpr int32 SupportSubdivisions = 40;
	FLNPSupportAtlas Support;
	Support.Layers.Add(MakeFullLayer(SupportSubdivisions, 1000.0));
	Support.Layers.Add(MakeFullLayer(SupportSubdivisions, 970.0));

	FLNPNavBakeSettings Settings;
	Settings.CrustSpacing = 150.0;
	Settings.LayerSpacing = 150.0;
	Settings.PortalSearchDistance = 100.0;
	const int32 NavSubdivisions = LNPSupportAtlas::ComputeSubdivisionsForSpacing(1000.0, Settings.CrustSpacing);
	const int32 WallI = NavSubdivisions / 2;

	FLNPNavData Navigation;
	FLNPNavTraversalData Traversal;
	FLNPNavBakeReport Report;
	FString Error;
	const bool bBuilt = LNPNavBaking::Build(
		Support,
		Settings,
		[&](const uint16 LayerId, const FVector3d& Position, const FVector3f& Normal)
		{
			const int32 I = PositionToCoord(Position, NavSubdivisions).X;
			return LayerId == 0 ? I != WallI : I <= WallI;
		},
		[](const uint16 FromLayer, const FVector3d& FromPosition, const FVector3f& FromNormal,
			const uint16 ToLayer, const FVector3d& ToPosition, const FVector3f& ToNormal)
		{
			return true;
		},
		Navigation,
		Traversal,
		Report,
		Error);
	if (!TestTrue(TEXT("Synthetic Support Atlas builds Navigation and Traversal"), bBuilt))
	{
		AddError(Error);
		return false;
	}

	TestEqual(TEXT("Support Layers map one-to-one to Nav Layers"), Navigation.Layers.Num(), 2);
	TestEqual(TEXT("Crust uses the configured coarse spacing"), Navigation.Layers[0].Subdivisions, NavSubdivisions);
	TestTrue(TEXT("A removed crust grid line splits the static graph"), Navigation.LocalStaticComponentCount >= 3);
	TestTrue(TEXT("Cross-Layer boundary candidates produce portals"), !Traversal.Portals.IsEmpty());
	TestTrue(TEXT("Crust boundary nodes produce ordered seam endpoints"), !Traversal.SeamEndpoints.IsEmpty());
	uint32 ComponentNodeCount = 0;
	for (const FLNPNavStaticComponent& Component : Traversal.StaticComponents)
	{
		ComponentNodeCount += Component.NodeCount;
	}
	TestEqual(TEXT("Report mirrors Navigation cell count"), Report.CellCount, static_cast<int32>(ComponentNodeCount));

	for (const FLNPNavPortal& Portal : Traversal.Portals)
	{
		TestNotEqual(TEXT("Every portal crosses Nav Layers"), Portal.A.LocalNavLayerId, Portal.B.LocalNavLayerId);
		TestTrue(TEXT("Portal endpoints are canonical"), Portal.A.LocalNavLayerId < Portal.B.LocalNavLayerId);
	}

	FLNPLocalNavNodeRef MissingWallNode;
	TestFalse(TEXT("Capsule-rejected crust coordinates are absent"),
		LNPNavData::MakeLocalNodeRef(Navigation, 0, WallI, 0, MissingWallNode));
	FLNPLocalNavNodeRef WallNeighbor;
	if (TestTrue(TEXT("A node beside the rejected line exists"),
		LNPNavData::MakeLocalNodeRef(Navigation, 0, WallI - 1, 0, WallNeighbor)))
	{
		const FLNPNavCell* Cell = LNPNavData::ResolveLocalNode(Navigation, WallNeighbor);
		TestTrue(TEXT("A node beside rejected clearance is marked NearStaticBlocker"),
			Cell != nullptr && EnumHasAnyFlags(Cell->Flags, ELNPNavCellFlags::NearStaticBlocker));
	}

	TArray<uint8> NavigationPayload;
	TArray<uint8> TraversalPayload;
	TestTrue(TEXT("Baked Navigation encodes"), LNPNavData::EncodeNavigation(Navigation, NavigationPayload, Error));
	TestTrue(TEXT("Baked Traversal encodes"),
		LNPNavData::EncodeTraversal(Traversal, Navigation, TraversalPayload, Error));
	FLNPNavData NavigationAgain;
	FLNPNavTraversalData TraversalAgain;
	FLNPNavBakeReport ReportAgain;
	TestTrue(TEXT("The same immutable inputs bake again"), LNPNavBaking::Build(
		Support,
		Settings,
		[&](const uint16 LayerId, const FVector3d& Position, const FVector3f& Normal)
		{
			const int32 I = PositionToCoord(Position, NavSubdivisions).X;
			return LayerId == 0 ? I != WallI : I <= WallI;
		},
		[](const uint16 FromLayer, const FVector3d& FromPosition, const FVector3f& FromNormal,
			const uint16 ToLayer, const FVector3d& ToPosition, const FVector3f& ToNormal)
		{
			return true;
		},
		NavigationAgain,
		TraversalAgain,
		ReportAgain,
		Error));
	TArray<uint8> NavigationPayloadAgain;
	TArray<uint8> TraversalPayloadAgain;
	TestTrue(TEXT("Repeated Navigation encodes"),
		LNPNavData::EncodeNavigation(NavigationAgain, NavigationPayloadAgain, Error));
	TestTrue(TEXT("Repeated Traversal encodes"),
		LNPNavData::EncodeTraversal(TraversalAgain, NavigationAgain, TraversalPayloadAgain, Error));
	TestTrue(TEXT("Navigation baking is byte deterministic"), NavigationPayloadAgain == NavigationPayload);
	TestTrue(TEXT("Traversal baking is byte deterministic"), TraversalPayloadAgain == TraversalPayload);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavMultiplePortalsTest,
	"LootNPop.SurfaceNavigation.Nav.MultiplePortals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPNavMultiplePortalsTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavBakingTest;
	FLNPSupportAtlas Support;
	Support.Layers.Add(MakeFullLayer(80, 1000.0));
	Support.Layers.Add(MakeFullLayer(80, 1000.0));
	FLNPNavBakeSettings Settings;
	Settings.CrustSpacing = Settings.LayerSpacing = 100.0;
	Settings.PortalSearchDistance = 180.0;
	const int32 N = LNPSupportAtlas::ComputeSubdivisionsForSpacing(1000.0, 100.0);
	const int32 Wall = N / 3;
	const int32 Doors[] = {3, N - Wall - 4};
	auto NodeClearance = [&](uint16 Layer, const FVector3d& P, const FVector3f&)
	{
		const FIntPoint C = PositionToCoord(P, N);
		return Layer == 0 ? C.X <= Wall : C.X > Wall;
	};
	// 같은 component 쌍을 가르는 벽: 두 입구의 폭만 허용한다. 긴 경계 검사는 벽을 제거한 동일 입력이다.
	auto DoorClearance = [&](uint16 A, const FVector3d& PA, const FVector3f&, uint16 B, const FVector3d& PB, const FVector3f&)
	{
		if (A == B) { return true; }
		const FIntPoint CA = PositionToCoord(PA, N), CB = PositionToCoord(PB, N);
		return FMath::Abs(CA.X - CB.X) == 1 && CA.Y == CB.Y && (CA.Y == Doors[0] || CA.Y == Doors[1]);
	};
	FLNPNavData Navigation;
	FLNPNavTraversalData Traversal;
	FLNPNavBakeReport Report;
	FString Error;
	if (!TestTrue(TEXT("Two-door bake succeeds"), LNPNavBaking::Build(Support, Settings, NodeClearance,
		DoorClearance, Navigation, Traversal, Report, Error))) { AddError(Error); return false; }
	TestEqual(TEXT("Both doors survive for the same component pair"), Traversal.Portals.Num(), 2);
	for (const FLNPNavPortal& Portal : Traversal.Portals)
	{
		FIntPoint A, B;
		LNPNavData::ResolveLocalNode(Navigation, Portal.A, &A);
		LNPNavData::ResolveLocalNode(Navigation, Portal.B, &B);
		TestTrue(TEXT("No portal crosses the solid wall"), A.Y == B.Y && (A.Y == Doors[0] || A.Y == Doors[1]));
	}
	FLNPNavSlotInput Input;
	Input.Support = MakeShared<FLNPSupportAtlas, ESPMode::ThreadSafe>(Support);
	Input.Navigation = MakeShared<FLNPNavData, ESPMode::ThreadSafe>(Navigation);
	Input.Traversal = MakeShared<FLNPNavTraversalData, ESPMode::ThreadSafe>(Traversal);
	TArray<FLNPNavSlotInput> Slots;
	Slots.Init(Input, 8);
	const TArray<FRotator> Rotations = {FRotator(0,0,0), FRotator(0,90,0), FRotator(0,180,0), FRotator(0,270,0),
		FRotator(180,0,0), FRotator(180,90,0), FRotator(180,180,0), FRotator(180,270,0)};
	FLNPNavSnapshot Snapshot;
	if (!TestTrue(TEXT("Two-door runtime graph assembles"), LNPNavRuntime::BuildSnapshot(Slots, Rotations, 1, Snapshot, Error)))
	{ AddError(Error); return false; }
	for (const int32 Door : Doors)
	{
		auto GraphNode = [&](uint16 Layer, int32 I) -> int32
		{
			FLNPLocalNavNodeRef Local;
			FLNPNavNodeRef Runtime;
			if (!LNPNavData::MakeLocalNodeRef(Navigation, Layer, I, Door, Local)
				|| !LNPNavRuntime::MakeRuntimeNodeRef(Snapshot, 0, Local, Runtime)) { return INDEX_NONE; }
			return LNPNavGraph::ToGraphNode(Snapshot, Runtime);
		};
		FLNPNavSearchScratch Scratch;
		FLNPNavSearch Search;
		LNPNavPathfinding::BeginSearch(Snapshot, GraphNode(0, Wall - 2), GraphNode(1, Wall + 3), {}, Scratch, Search);
		while (Search.Status == ELNPNavSearchStatus::Running) { LNPNavPathfinding::StepSearch(Snapshot, Scratch, Search, 4000); }
		TestTrue(TEXT("A* finds a path near each door"), Search.Status == ELNPNavSearchStatus::Found);
		TArray<int32> Path;
		LNPNavPathfinding::ExtractNodePath(Scratch, Search, Path);
		int32 Crossings = 0;
		for (int32 Index = 1; Index < Path.Num(); ++Index)
		{
			const auto& A = Snapshot.Graph.GetNode(Snapshot.Graph.GetSlot(Path[Index - 1]), Path[Index - 1]);
			const auto& B = Snapshot.Graph.GetNode(Snapshot.Graph.GetSlot(Path[Index]), Path[Index]);
			if (A.LayerOrdinal != B.LayerOrdinal)
			{
				++Crossings;
				TestTrue(TEXT("A* selects the nearby door"), A.J == Door && B.J == Door);
			}
		}
		TestEqual(TEXT("A* crosses layers once"), Crossings, 1);
	}
	TArray<uint8> First, Again;
	TestTrue(TEXT("Two-door codec encodes"), LNPNavData::EncodeTraversal(Traversal, Navigation, First, Error));
	FLNPNavData RepeatNav;
	FLNPNavTraversalData RepeatTraversal;
	FLNPNavBakeReport RepeatReport;
	TestTrue(TEXT("Two-door repeat succeeds"), LNPNavBaking::Build(Support, Settings, NodeClearance,
		DoorClearance, RepeatNav, RepeatTraversal, RepeatReport, Error));
	TestTrue(TEXT("Repeated codec encodes"), LNPNavData::EncodeTraversal(RepeatTraversal, RepeatNav, Again, Error));
	TestTrue(TEXT("Portal selection is byte deterministic"), First == Again);
	TestTrue(TEXT("Long open boundary bakes"), LNPNavBaking::Build(Support, Settings, NodeClearance,
		[](uint16, const FVector3d&, const FVector3f&, uint16, const FVector3d&, const FVector3f&) { return true; },
		Navigation, Traversal, Report, Error));
	TestTrue(TEXT("Long boundary retains distributed portals"), Traversal.Portals.Num() > 2);
	TestTrue(TEXT("Nearby duplicates are suppressed"), Report.PortalSpacingRejectCount > 0);
	TArray<FVector3d> Midpoints;
	for (const FLNPNavPortal& Portal : Traversal.Portals)
	{
		auto Point = [&](const FLNPLocalNavNodeRef& Node)
		{
			FIntPoint C;
			LNPNavData::ResolveLocalNode(Navigation, Node, &C);
			return LNPSupportAtlas::GetSampleDirection(N, C.X, C.Y) * 1000.0;
		};
		const FVector3d Midpoint = (Point(Portal.A) + Point(Portal.B)) * 0.5;
		for (const FVector3d& Previous : Midpoints)
		{
			TestTrue(TEXT("Selected midpoints obey minimum spacing"), FVector3d::Distance(Previous, Midpoint) >= Settings.PortalMinSpacing - 0.01);
		}
		Midpoints.Add(Midpoint);
	}
	AddInfo(FString::Printf(TEXT("Long boundary: portals=%d candidates=%d/%d/%d suppressed=%d portalBake=%.3fms"),
		Report.PortalCount, Report.PortalDistanceCandidateCount, Report.PortalStepCandidateCount,
		Report.PortalClearanceCandidateCount, Report.PortalSpacingRejectCount, Report.PortalBakeSeconds * 1000.0));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavStepAndSlopeTest,
	"LootNPop.SurfaceNavigation.Nav.StepAndSlope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPNavStepAndSlopeTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavBakingTest;
	// 지각 R=30,000, Nav 간격 200cm. Support i 좌표로 평지 → 경사로(Nav 한 칸당 60cm 상승) → 평지 → 400cm 절벽.
	// 60cm는 step 한도 45cm보다 크지만 Nav 간격(81~200cm)의 45° 경사 안이므로 이어져야 한다.
	constexpr double Radius = 30000.0;
	FLNPNavBakeSettings Settings;
	const int32 NavSubdivisions = LNPSupportAtlas::ComputeSubdivisionsForSpacing(Radius, Settings.CrustSpacing);
	const int32 SupportSubdivisions = NavSubdivisions * 2;
	const int32 RampStart = 300;
	const int32 RampEnd = 340;
	const int32 Cliff = 500;
	FLNPSupportAtlasLayer Crust = MakeFullLayer(SupportSubdivisions, Radius);
	for (int32 J = 0; J <= SupportSubdivisions; ++J)
	{
		for (int32 I = 0; I + J <= SupportSubdivisions; ++I)
		{
			const double Rise = 30.0 * FMath::Clamp(I - RampStart, 0, RampEnd - RampStart) + (I >= Cliff ? 400.0 : 0.0);
			Crust.RadiusQ[Crust.Layout.Find(I, J)] = static_cast<int16>(FMath::RoundToInt32(-Rise / Crust.RadiusStep));
		}
	}
	FLNPSupportAtlas Support;
	Support.Layers.Add(MoveTemp(Crust));

	FLNPNavData Navigation;
	FLNPNavTraversalData Traversal;
	FLNPNavBakeReport Report;
	FString Error;
	if (!TestTrue(TEXT("Stepped Support Atlas builds Navigation"), LNPNavBaking::Build(
		Support,
		Settings,
		[](uint16, const FVector3d&, const FVector3f&) { return true; },
		[](uint16, const FVector3d&, const FVector3f&, uint16, const FVector3d&, const FVector3f&) { return true; },
		Navigation, Traversal, Report, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Nav uses the expected crust resolution"), Navigation.Layers[0].Subdivisions, NavSubdivisions);

	auto ComponentAt = [&Navigation](const int32 I, const int32 J)
	{
		FLNPLocalNavNodeRef Node;
		const FLNPNavCell* Cell = LNPNavData::MakeLocalNodeRef(Navigation, 0, I, J, Node)
			? LNPNavData::ResolveLocalNode(Navigation, Node)
			: nullptr;
		return Cell ? static_cast<int32>(Cell->LocalStaticComponentId) : INDEX_NONE;
	};
	const int32 Low = ComponentAt(RampStart / 2 - 20, 60);
	const int32 High = ComponentAt(RampEnd / 2 + 20, 60);
	const int32 BeyondCliff = ComponentAt(Cliff / 2 + 20, 60);
	TestTrue(TEXT("All probe nodes exist"), Low != INDEX_NONE && High != INDEX_NONE && BeyondCliff != INDEX_NONE);
	TestEqual(TEXT("A walkable ramp steeper than step-up per cell stays connected"), High, Low);
	TestNotEqual(TEXT("A cliff higher than step and slope allowance splits the crust"), BeyondCliff, Low);
	TestEqual(TEXT("Only the cliff splits the crust"), static_cast<int32>(Navigation.LocalStaticComponentCount), 2);
	return !HasAnyErrors();
}

#endif
