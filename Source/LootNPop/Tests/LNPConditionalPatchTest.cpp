// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurfaceNavigation/LNPConditionalPatch.h"
#include "SurfaceNavigation/LNPNavBaking.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPConditionalPatchCodecTest,
	"LootNPop.SurfaceNavigation.ConditionalPatch.Codec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPConditionalPatchCodecTest::RunTest(const FString& Parameters)
{
	FString Error;
	FLNPSupportLayerRaster Raster;
	Raster.Layout = FLNPSupportLayout::MakeFull(32);
	Raster.Samples.SetNum(Raster.Layout.Num());
	for (int32 J = 0; J <= 32; ++J)
	{
		for (int32 I = 0; I + J <= 32; ++I)
		{
			Raster.Samples[Raster.Layout.Find(I, J)] = {3000.0,
				FVector3f(-LNPSupportAtlas::GetSampleDirection(32, I, J)),
				ELNPSupportSampleFlags::Valid | ELNPSupportSampleFlags::Walkable};
		}
	}
	FLNPSupportCodecSettings Codec;
	Codec.BaseRadius = 3000.0;
	FLNPSupportAtlas Base;
	Base.Layers.SetNum(1);
	if (!LNPSupportAtlas::QuantizeSparseLayer(Raster, Codec, Base.Layers[0], Error)) { AddError(Error); return false; }
	FLNPNavBakeSettings Settings;
	Settings.CrustSpacing = 1000.0;
	FLNPNavData Nav;
	FLNPNavTraversalData Traversal;
	FLNPNavBakeReport Report;
	if (!LNPNavBaking::Build(Base, Settings,
		[](uint16, const FVector3d&, const FVector3f&) { return true; },
		[](uint16, const FVector3d&, const FVector3f&, uint16, const FVector3d&, const FVector3f&) { return true; },
		Nav, Traversal, Report, Error)) { AddError(Error); return false; }
	FLNPLocalNavNodeRef BaseA, BaseB;
	if (!LNPNavData::MakeLocalNodeRef(Nav, 0, 2, 2, BaseA)
		|| !LNPNavData::MakeLocalNodeRef(Nav, 0, 3, 2, BaseB)) { AddError(TEXT("Synthetic base nodes are missing.")); return false; }
	FLNPConditionalPatchData Data;
	FLNPConditionalPatch& Patch = Data.Patches.AddDefaulted_GetRef();
	Patch.MarkerId = FGuid(1, 2, 3, 4);
	Patch.FirstSupportLayer = 1;
	Patch.Navigation.Agent = Nav.Agent;
	Patch.Navigation.LocalStaticComponentCount = 1;
	FLNPNavLayer& NavLayer = Patch.Navigation.Layers.AddDefaulted_GetRef();
	NavLayer.LocalNavLayerId = NavLayer.LocalSupportLayerId = 1;
	NavLayer.Subdivisions = 8;
	FLNPNavTile& Tile = NavLayer.Tiles.AddDefaulted_GetRef();
	Tile.TileId = 0;
	FLNPNavCell& Cell = Tile.Cells.AddDefaulted_GetRef();
	Cell.LocalCellIndex = 2 * 16 + 2;
	Cell.Flags = ELNPNavCellFlags::Walkable;
	Cell.LocalStaticComponentId = 0;
	Cell.ClearanceClass = 2;
	Raster.Layout = FLNPSupportLayout();
	Raster.Layout.Subdivisions = 32;
	Raster.Layout.J0 = 6;
	Raster.Layout.Rows.Init({6, 5}, 5);
	Raster.Layout.BuildOffsets();
	Raster.Samples.Init({2500.0, FVector3f(-LNPSupportAtlas::GetSampleDirection(32, 8, 8)),
		ELNPSupportSampleFlags::Valid | ELNPSupportSampleFlags::Walkable}, Raster.Layout.Num());
	Raster.SourceIndex = 0;
	Patch.SupportLayers.SetNum(1);
	if (!LNPSupportAtlas::QuantizeSparseLayer(Raster, Codec, Patch.SupportLayers[0], Error)) { AddError(Error); return false; }
	Patch.FaceMaps.Add({0, {1, {}}});
	Patch.BlockedNodes = {BaseA};
	Patch.BlockedEdges = {{BaseA, BaseB}};
	Patch.WalkEdges = {{BaseA, {1, 0, Cell.LocalCellIndex}}};
	LNPConditionalPatch::Canonicalize(Patch);
	TArray<uint8> Payload, RoundTrip;
	FLNPConditionalPatchData Decoded;
	if (!TestTrue(TEXT("Patch encodes"), LNPConditionalPatch::Encode(Data, Base, Nav, Payload, Error))
		|| !TestTrue(TEXT("Patch decodes"), LNPConditionalPatch::Decode(Payload, Base, Nav, Decoded, Error)))
	{
		AddError(Error); return false;
	}
	TestTrue(TEXT("Decoded patch re-encodes identically"),
		LNPConditionalPatch::Encode(Decoded, Base, Nav, RoundTrip, Error) && Payload == RoundTrip);
	TestEqual(TEXT("Patch stores only its 25 samples, no duplicate crust"), Decoded.Patches[0].SupportLayers[0].Num(), 25);
	TestEqual(TEXT("Sparse row offsets rebuilt on decode"), Decoded.Patches[0].SupportLayers[0].Layout.Find(8, 8), 12);
	TestEqual(TEXT("Face map preserves appended Layer ID"), Decoded.Patches[0].FaceMaps[0].FaceMap.Resolve(0), static_cast<uint16>(1));
	FLNPConditionalPatchData Empty;
	TestTrue(TEXT("Empty patch stream encodes a versioned header"), LNPConditionalPatch::Encode(Empty, Base, Nav, RoundTrip, Error));
	TestEqual(TEXT("Empty stream uses four bytes"), RoundTrip.Num(), 4);
	TestTrue(TEXT("Empty stream decodes"), LNPConditionalPatch::Decode(RoundTrip, Base, Nav, Empty, Error));
	for (const int32 Bytes : {0, 3, Payload.Num() - 1})
	{
		TestFalse(TEXT("Truncated patch is rejected"), LNPConditionalPatch::Decode(MakeArrayView(Payload.GetData(), Bytes), Base, Nav, Empty, Error));
		TestTrue(TEXT("Rejected decode clears partial output"), Empty.Patches.IsEmpty());
	}
	RoundTrip = Payload; RoundTrip.Add(0);
	TestFalse(TEXT("Trailing bytes are rejected"), LNPConditionalPatch::Decode(RoundTrip, Base, Nav, Empty, Error));
	RoundTrip = Payload; RoundTrip[0] = 2;
	TestFalse(TEXT("Wrong codec version is rejected"), LNPConditionalPatch::Decode(RoundTrip, Base, Nav, Empty, Error));
	FLNPConditionalPatchData Invalid = Data;
	Invalid.Patches.Add(Patch);
	TestFalse(TEXT("Duplicate MarkerId is rejected"), LNPConditionalPatch::Validate(Invalid, Base, Nav, Error));
	Invalid = Data; Invalid.Patches[0].FirstSupportLayer = 0;
	TestFalse(TEXT("Patch cannot reuse static Layer ID"), LNPConditionalPatch::Validate(Invalid, Base, Nav, Error));
	Invalid = Data; Invalid.Patches[0].SupportLayers[0].Layout.Rows[0].Count = MAX_int32;
	TestFalse(TEXT("Invalid sparse footprint cannot overflow or leave the grid"), LNPConditionalPatch::Validate(Invalid, Base, Nav, Error));
	Invalid = Data; Invalid.Patches[0].FaceMaps[0].FaceMap.UniformLayer = 0;
	TestFalse(TEXT("Patch face map cannot alias static Support"), LNPConditionalPatch::Validate(Invalid, Base, Nav, Error));
	Invalid = Data; Invalid.Patches[0].WalkEdges[0].B.LocalNavLayerId = 2;
	TestFalse(TEXT("Missing patch endpoint is rejected"), LNPConditionalPatch::Validate(Invalid, Base, Nav, Error));
	FLNPNavData Combined = Nav;
	Combined.Layers.Append(Patch.Navigation.Layers);
	Combined.LocalStaticComponentCount += 1;
	Combined.Layers.Last().Tiles[0].Cells[0].LocalStaticComponentId = Nav.LocalStaticComponentCount;
	FLNPNavTraversalData CombinedTraversal = Traversal;
	CombinedTraversal.Portals.Add({BaseA, {1, 0, Cell.LocalCellIndex}});
	FLNPConditionalPatch Extracted = Patch;
	Extracted.WalkEdges.Empty();
	TestTrue(TEXT("Delta extracts additional Nav with patch-local component ID"),
		LNPConditionalPatch::ExtractNavigationDelta(Nav, Combined, CombinedTraversal, Extracted, Error));
	TestEqual(TEXT("Extracted component begins at zero"), Extracted.Navigation.Layers[0].Tiles[0].Cells[0].LocalStaticComponentId, static_cast<uint16>(0));
	Combined.Layers[0].Tiles[0].Cells[0].ClearanceClass += 1;
	TestFalse(TEXT("Delta refuses a changed base graph"), LNPConditionalPatch::ExtractNavigationDelta(Nav, Combined, CombinedTraversal, Extracted, Error));
	return !HasAnyErrors();
}

#endif
