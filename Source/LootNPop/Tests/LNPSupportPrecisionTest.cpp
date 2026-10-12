// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataAsset/LNPOctantSurfaceData.h"
#include "SurfaceNavigation/LNPNavGraph.h"
#include "SurfaceNavigation/LNPSupportAtlas.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSupportPrecisionTest,
	"LootNPop.SurfaceNavigation.Runtime.SupportPrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FLNPSupportPrecisionTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : {TEXT("Meadow_00"), TEXT("Bastion_00"), TEXT("Hollow_00")})
	{
		const FString Path = FString::Printf(TEXT("/Game/Maps/%s/DA_OctantSurface_%s.DA_OctantSurface_%s"), Name, Name, Name);
		const ULNPOctantSurfaceData* Data = LoadObject<ULNPOctantSurfaceData>(nullptr, *Path);
		if (!TestNotNull(Path + TEXT(": saved data loads"), Data)) { continue; }
		FLNPSupportAtlas Support;
		FLNPNavData Navigation;
		FLNPNavTraversalData Traversal;
		FString Error;
		if (!LNPSupportAtlas::Decode(Data->SupportPayload, Support, Error)
			|| !LNPNavData::DecodeNavigation(Data->NavigationPayload, Navigation, Error)
			|| !LNPNavData::DecodeTraversal(Data->TraversalPayload, Navigation, Traversal, Error))
		{
			AddError(Path + TEXT(": ") + Error);
			continue;
		}
		if (FCString::Strcmp(Name, TEXT("Bastion_00")) == 0)
		{
			// V007 패키지 실패점: Support (244.5, 202.5)의 삼각형 경계다. 위쪽 삼각형에는 NeedsExact가 있다.
			const FLNPNavLayer& NavLayer = Navigation.Layers[1];
			const FLNPSupportAtlasLayer& Layer = Support.Layers[NavLayer.LocalSupportLayerId];
			const FVector3d Direction = LNPSupportAtlas::GetSampleDirection(NavLayer.Subdivisions, 326, 270);
			FLNPSupportLayerQuery Query;
			TestTrue(TEXT("Bastion saved Nav node (326,270) has interpolated Support"), LNPSupportAtlas::QueryLayer(Layer, Direction, Query));
			AddInfo(FString::Printf(TEXT("Bastion NavN=%d SupportN=%d direction=(%.17g,%.17g,%.17g) validCorners=%d interpolated=%d"),
				NavLayer.Subdivisions, Layer.Layout.Subdivisions, Direction.X, Direction.Y, Direction.Z, Query.ValidCorners, Query.bInterpolated));
			const FVector3d RiskDirection(326.00001, 270.00001, NavLayer.Subdivisions - 596.00002);
			TestFalse(TEXT("Bastion adjacent NeedsExact triangle remains non-interpolated"), LNPSupportAtlas::QueryLayer(Layer, RiskDirection, Query));
		}
		FLNPNavAssetGraph Graph;
		if (!LNPNavGraph::BuildAssetGraph(Support, Navigation, Traversal, Graph, Error))
		{
			AddError(Path + TEXT(": ") + Error);
		}
		else
		{
			AddInfo(FString::Printf(TEXT("%s: %d saved Nav nodes resolve Support"), Name, Graph.Nodes.Num()));
		}
	}
	return !HasAnyErrors();
}

#endif
