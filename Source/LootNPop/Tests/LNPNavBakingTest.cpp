// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurfaceNavigation/LNPNavBaking.h"

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
