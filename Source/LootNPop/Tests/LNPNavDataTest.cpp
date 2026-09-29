// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "SurfaceNavigation/LNPNavData.h"

namespace LNPNavDataTest
{
	FLNPNavCell* FindCell(FLNPNavLayer& Layer, const int32 I, const int32 J)
	{
		uint16 TileX = 0;
		uint16 TileY = 0;
		uint8 LocalCell = 0;
		if (!LNPNavData::MakeTileAddress(I, J, TileX, TileY, LocalCell))
		{
			return nullptr;
		}
		for (FLNPNavTile& Tile : Layer.Tiles)
		{
			if (Tile.TileX == TileX && Tile.TileY == TileY)
			{
				return Tile.Cells.FindByPredicate([LocalCell](const FLNPNavCell& Cell)
				{
					return Cell.LocalCellIndex == LocalCell;
				});
			}
		}
		return nullptr;
	}

	void AddCell(FLNPNavLayer& Layer, const int32 I, const int32 J, const uint16 Component)
	{
		uint16 TileX = 0;
		uint16 TileY = 0;
		uint8 LocalCell = 0;
		check(LNPNavData::MakeTileAddress(I, J, TileX, TileY, LocalCell));
		FLNPNavTile* Tile = Layer.Tiles.FindByPredicate([TileX, TileY](const FLNPNavTile& Candidate)
		{
			return Candidate.TileX == TileX && Candidate.TileY == TileY;
		});
		if (Tile == nullptr)
		{
			Tile = &Layer.Tiles.AddDefaulted_GetRef();
			Tile->TileX = TileX;
			Tile->TileY = TileY;
		}
		FLNPNavCell& Cell = Tile->Cells.AddDefaulted_GetRef();
		Cell.LocalCellIndex = LocalCell;
		Cell.ClearanceClass = 4;
		Cell.Flags = ELNPNavCellFlags::Walkable;
		Cell.LocalStaticComponentId = Component;
	}

	void CanonicalizeTiles(FLNPNavLayer& Layer)
	{
		Layer.Tiles.Sort([](const FLNPNavTile& A, const FLNPNavTile& B)
		{
			return A.TileY != B.TileY ? A.TileY < B.TileY : A.TileX < B.TileX;
		});
		for (int32 TileIndex = 0; TileIndex < Layer.Tiles.Num(); ++TileIndex)
		{
			FLNPNavTile& Tile = Layer.Tiles[TileIndex];
			Tile.TileId = TileIndex;
			Tile.Cells.Sort([](const FLNPNavCell& A, const FLNPNavCell& B)
			{
				return A.LocalCellIndex < B.LocalCellIndex;
			});
		}
	}

	FLNPNavData MakeNavigation()
	{
		FLNPNavData Data;
		Data.LocalStaticComponentCount = 3;
		FLNPNavLayer& Crust = Data.Layers.AddDefaulted_GetRef();
		Crust.LocalNavLayerId = 0;
		Crust.LocalSupportLayerId = 0;
		Crust.Subdivisions = 40;
		constexpr int32 CenterI = 16;
		constexpr int32 CenterJ = 16;
		AddCell(Crust, CenterI, CenterJ, 0);
		for (uint8 Direction = 0; Direction < 6; ++Direction)
		{
			FIntPoint Neighbor;
			check(LNPNavData::TryGetNeighborCoord(
				Crust.Subdivisions, CenterI, CenterJ, static_cast<ELNPNavNeighbor>(Direction), Neighbor));
			AddCell(Crust, Neighbor.X, Neighbor.Y, 0);
		}
		// x=0 변의 step 8 좌표: (0, N-8) = (0, 32).
		AddCell(Crust, 0, 32, 1);
		CanonicalizeTiles(Crust);
		FLNPNavCell* Center = FindCell(Crust, CenterI, CenterJ);
		check(Center != nullptr);
		for (uint8 Direction = 0; Direction < 6; ++Direction)
		{
			FIntPoint Neighbor;
			LNPNavData::TryGetNeighborCoord(
				Crust.Subdivisions, CenterI, CenterJ, static_cast<ELNPNavNeighbor>(Direction), Neighbor);
			FLNPNavCell* NeighborCell = FindCell(Crust, Neighbor.X, Neighbor.Y);
			check(NeighborCell != nullptr);
			Center->EdgeMask |= LNPNavData::GetNeighborBit(static_cast<ELNPNavNeighbor>(Direction));
			NeighborCell->EdgeMask |= LNPNavData::GetNeighborBit(
				LNPNavData::GetOppositeNeighbor(static_cast<ELNPNavNeighbor>(Direction)));
		}

		FLNPNavLayer& Cave = Data.Layers.AddDefaulted_GetRef();
		Cave.LocalNavLayerId = 1;
		Cave.LocalSupportLayerId = 1;
		Cave.Subdivisions = 40;
		AddCell(Cave, 10, 10, 2);
		CanonicalizeTiles(Cave);
		return Data;
	}

	FLNPNavTraversalData MakeTraversal(const FLNPNavData& Navigation)
	{
		FLNPNavTraversalData Traversal;
		Traversal.StaticComponents = {{0, 7}, {1, 1}, {2, 1}};
		FLNPNavPortal& Portal = Traversal.Portals.AddDefaulted_GetRef();
		check(LNPNavData::MakeLocalNodeRef(Navigation, 0, 16, 16, Portal.A));
		check(LNPNavData::MakeLocalNodeRef(Navigation, 1, 10, 10, Portal.B));
		Portal.MinClearanceClass = 3;
		Portal.Flags = ELNPNavPortalFlags::Bidirectional;

		FLNPNavSeamEndpoint& Seam = Traversal.SeamEndpoints.AddDefaulted_GetRef();
		check(LNPNavData::MakeLocalNodeRef(Navigation, 0, 0, 32, Seam.Node));
		Seam.Edge = ELNPCrustSeamEdge::X0;
		Seam.SeamStep = 8;
		Seam.MinClearanceClass = 2;
		return Traversal;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavTileAddressTest,
	"LootNPop.SurfaceNavigation.Nav.TileAddress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPNavTileAddressTest::RunTest(const FString& Parameters)
{
	constexpr int32 N = 40;
	constexpr int32 I = 16;
	constexpr int32 J = 16;
	TSet<FIntPoint> NeighborCoords;
	for (uint8 Direction = 0; Direction < 6; ++Direction)
	{
		const ELNPNavNeighbor Neighbor = static_cast<ELNPNavNeighbor>(Direction);
		FIntPoint Coord;
		TestTrue(TEXT("All six center neighbors exist"), LNPNavData::TryGetNeighborCoord(N, I, J, Neighbor, Coord));
		NeighborCoords.Add(Coord);
		FIntPoint Back;
		TestTrue(TEXT("Opposite neighbor returns to the source"),
			LNPNavData::TryGetNeighborCoord(N, Coord.X, Coord.Y, LNPNavData::GetOppositeNeighbor(Neighbor), Back));
		TestEqual(TEXT("Opposite neighbor is exact"), Back, FIntPoint(I, J));

		uint16 TileX = 0;
		uint16 TileY = 0;
		uint8 LocalCell = 0;
		TestTrue(TEXT("Neighbor has a Tile address"), LNPNavData::MakeTileAddress(Coord.X, Coord.Y, TileX, TileY, LocalCell));
		TestEqual(TEXT("Tile address round trip"), LNPNavData::DecodeTileAddress(TileX, TileY, LocalCell), Coord);
	}
	TestEqual(TEXT("Triangular grid has six unique neighbors"), NeighborCoords.Num(), 6);

	uint16 CenterTileX = 0;
	uint16 CenterTileY = 0;
	uint8 CenterCell = 0;
	LNPNavData::MakeTileAddress(I, J, CenterTileX, CenterTileY, CenterCell);
	int32 CrossTileNeighbors = 0;
	for (const FIntPoint& Coord : NeighborCoords)
	{
		uint16 TileX = 0;
		uint16 TileY = 0;
		uint8 LocalCell = 0;
		LNPNavData::MakeTileAddress(Coord.X, Coord.Y, TileX, TileY, LocalCell);
		CrossTileNeighbors += TileX != CenterTileX || TileY != CenterTileY ? 1 : 0;
	}
	TestTrue(TEXT("The fixture crosses both 16-cell Tile axes"), CrossTileNeighbors >= 3);
	FIntPoint Outside;
	TestFalse(TEXT("A neighbor outside the triangular domain is rejected"),
		LNPNavData::TryGetNeighborCoord(N, N, 0, ELNPNavNeighbor::IPositive, Outside));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavNavigationCodecTest,
	"LootNPop.SurfaceNavigation.Nav.NavigationCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPNavNavigationCodecTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavDataTest;
	FLNPNavData Navigation = MakeNavigation();
	FString Error;
	TestTrue(TEXT("Navigation fixture validates"), LNPNavData::ValidateNavigation(Navigation, Error));

	TArray<uint8> Payload;
	if (!TestTrue(TEXT("Navigation encodes"), LNPNavData::EncodeNavigation(Navigation, Payload, Error)))
	{
		AddError(Error);
		return false;
	}
	FLNPNavData Decoded;
	if (!TestTrue(TEXT("Navigation decodes"), LNPNavData::DecodeNavigation(Payload, Decoded, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Layer count round trips"), Decoded.Layers.Num(), 2);
	TestEqual(TEXT("Static component count round trips"), Decoded.LocalStaticComponentCount, static_cast<uint16>(3));
	TestEqual(TEXT("Crust spans four non-empty Tiles"), Decoded.Layers[0].Tiles.Num(), 4);

	FLNPLocalNavNodeRef Center;
	TestTrue(TEXT("Center resolves by triangular coordinate"), LNPNavData::MakeLocalNodeRef(Decoded, 0, 16, 16, Center));
	FIntPoint CenterCoord;
	const FLNPNavCell* CenterCell = LNPNavData::ResolveLocalNode(Decoded, Center, &CenterCoord);
	TestNotNull(TEXT("Resolved center cell exists"), CenterCell);
	TestEqual(TEXT("Resolved center coordinate"), CenterCoord, FIntPoint(16, 16));
	TestEqual(TEXT("Center keeps all six edges"), CenterCell ? CenterCell->EdgeMask : 0, LNPNavData::NeighborMask);

	FLNPNavNodeRef RuntimeNode;
	RuntimeNode.RuntimeNavLayerId = 9;
	RuntimeNode.TileId = Center.TileId;
	RuntimeNode.LocalCellIndex = Center.LocalCellIndex;
	RuntimeNode.SnapshotGeneration = 17;
	TestTrue(TEXT("Runtime node accepts its full uint64 generation"), LNPNavData::IsCurrentNodeRef(RuntimeNode, 17));
	TestFalse(TEXT("Runtime node rejects a stale generation"), LNPNavData::IsCurrentNodeRef(RuntimeNode, 18));

	TArray<uint8> RoundTrip;
	TestTrue(TEXT("Decoded Navigation re-encodes"), LNPNavData::EncodeNavigation(Decoded, RoundTrip, Error));
	TestTrue(TEXT("Navigation codec is byte deterministic"), RoundTrip == Payload);

	FLNPNavData Reordered = Navigation;
	Algo::Reverse(Reordered.Layers);
	for (FLNPNavLayer& Layer : Reordered.Layers)
	{
		Algo::Reverse(Layer.Tiles);
		for (FLNPNavTile& Tile : Layer.Tiles)
		{
			Algo::Reverse(Tile.Cells);
		}
	}
	TArray<uint8> ReorderedPayload;
	TestTrue(TEXT("Reordered canonical IDs encode"), LNPNavData::EncodeNavigation(Reordered, ReorderedPayload, Error));
	TestTrue(TEXT("Input array order does not change Navigation bytes"), ReorderedPayload == Payload);

	FLNPNavData Asymmetric = Navigation;
	FLNPNavCell* MutableCenter = FindCell(Asymmetric.Layers[0], 16, 16);
	check(MutableCenter != nullptr);
	MutableCenter->EdgeMask &= ~LNPNavData::GetNeighborBit(ELNPNavNeighbor::IPositive);
	TArray<uint8> Rejected;
	TestFalse(TEXT("Asymmetric edge is rejected"), LNPNavData::EncodeNavigation(Asymmetric, Rejected, Error));

	FLNPNavData DuplicateTile = Navigation;
	const FLNPNavTile DuplicateTileValue = DuplicateTile.Layers[0].Tiles[0];
	DuplicateTile.Layers[0].Tiles.Add(DuplicateTileValue);
	TestFalse(TEXT("Duplicate Tile is rejected"), LNPNavData::EncodeNavigation(DuplicateTile, Rejected, Error));

	TestFalse(TEXT("Truncated Navigation payload is rejected"),
		LNPNavData::DecodeNavigation(MakeArrayView(Payload.GetData(), Payload.Num() - 1), Decoded, Error));
	TArray<uint8> Trailing = Payload;
	Trailing.Add(0);
	TestFalse(TEXT("Trailing Navigation byte is rejected"), LNPNavData::DecodeNavigation(Trailing, Decoded, Error));
	TArray<uint8> WrongVersion = Payload;
	++WrongVersion[0];
	TestFalse(TEXT("Unknown Navigation codec version is rejected"), LNPNavData::DecodeNavigation(WrongVersion, Decoded, Error));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNavTraversalCodecTest,
	"LootNPop.SurfaceNavigation.Nav.TraversalCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPNavTraversalCodecTest::RunTest(const FString& Parameters)
{
	using namespace LNPNavDataTest;
	const FLNPNavData Navigation = MakeNavigation();
	FLNPNavTraversalData Traversal = MakeTraversal(Navigation);
	FString Error;
	TestTrue(TEXT("Traversal fixture validates"), LNPNavData::ValidateTraversal(Traversal, Navigation, Error));

	TArray<uint8> Payload;
	if (!TestTrue(TEXT("Traversal encodes"), LNPNavData::EncodeTraversal(Traversal, Navigation, Payload, Error)))
	{
		AddError(Error);
		return false;
	}
	FLNPNavTraversalData Decoded;
	if (!TestTrue(TEXT("Traversal decodes"), LNPNavData::DecodeTraversal(Payload, Navigation, Decoded, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Static component descriptors round trip"), Decoded.StaticComponents.Num(), 3);
	TestEqual(TEXT("Portal count round trips"), Decoded.Portals.Num(), 1);
	TestEqual(TEXT("Seam endpoint count round trips"), Decoded.SeamEndpoints.Num(), 1);
	TestEqual(TEXT("Seam edge round trips"), Decoded.SeamEndpoints[0].Edge, ELNPCrustSeamEdge::X0);
	TestEqual(TEXT("Seam step round trips"), Decoded.SeamEndpoints[0].SeamStep, static_cast<uint16>(8));

	TArray<uint8> RoundTrip;
	TestTrue(TEXT("Decoded Traversal re-encodes"), LNPNavData::EncodeTraversal(Decoded, Navigation, RoundTrip, Error));
	TestTrue(TEXT("Traversal codec is byte deterministic"), RoundTrip == Payload);

	FLNPNavTraversalData MissingNode = Traversal;
	MissingNode.Portals[0].B.TileId = 99;
	TArray<uint8> Rejected;
	TestFalse(TEXT("Portal to a missing node is rejected"),
		LNPNavData::EncodeTraversal(MissingNode, Navigation, Rejected, Error));

	FLNPNavTraversalData WrongSeam = Traversal;
	WrongSeam.SeamEndpoints[0].SeamStep = 9;
	TestFalse(TEXT("Seam step that disagrees with node coordinates is rejected"),
		LNPNavData::EncodeTraversal(WrongSeam, Navigation, Rejected, Error));

	FLNPNavTraversalData DuplicatePortal = Traversal;
	const FLNPNavPortal DuplicatePortalValue = DuplicatePortal.Portals[0];
	DuplicatePortal.Portals.Add(DuplicatePortalValue);
	TestFalse(TEXT("Duplicate portal is rejected"),
		LNPNavData::EncodeTraversal(DuplicatePortal, Navigation, Rejected, Error));

	TestFalse(TEXT("Truncated Traversal payload is rejected"),
		LNPNavData::DecodeTraversal(MakeArrayView(Payload.GetData(), Payload.Num() - 1), Navigation, Decoded, Error));
	TArray<uint8> Trailing = Payload;
	Trailing.Add(0);
	TestFalse(TEXT("Trailing Traversal byte is rejected"),
		LNPNavData::DecodeTraversal(Trailing, Navigation, Decoded, Error));
	return !HasAnyErrors();
}

#endif
