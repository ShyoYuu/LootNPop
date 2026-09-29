// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavData.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	constexpr int32 MaxLayerCount = MAX_uint16 - 1;
	constexpr int32 MaxTileCount = 4 * 1024 * 1024;
	constexpr int32 MaxCellCount = 16 * 1024 * 1024;
	constexpr int32 MaxTraversalRecordCount = 4 * 1024 * 1024;

	bool IsFinite(const FLNPNavAgentProfile& Agent)
	{
		return FMath::IsFinite(Agent.Radius) && FMath::IsFinite(Agent.HalfHeight)
			&& FMath::IsFinite(Agent.MaxStepUp) && FMath::IsFinite(Agent.MaxStepDown)
			&& FMath::IsFinite(Agent.WalkableMinDot);
	}

	bool LayerLess(const FLNPNavLayer& A, const FLNPNavLayer& B)
	{
		return A.LocalNavLayerId < B.LocalNavLayerId;
	}

	bool TileLess(const FLNPNavTile& A, const FLNPNavTile& B)
	{
		return A.TileY != B.TileY ? A.TileY < B.TileY : A.TileX < B.TileX;
	}

	bool NodeLess(const FLNPLocalNavNodeRef& A, const FLNPLocalNavNodeRef& B)
	{
		return A.LocalNavLayerId != B.LocalNavLayerId ? A.LocalNavLayerId < B.LocalNavLayerId
			: A.TileId != B.TileId ? A.TileId < B.TileId
			: A.LocalCellIndex < B.LocalCellIndex;
	}

	bool NodeEqual(const FLNPLocalNavNodeRef& A, const FLNPLocalNavNodeRef& B)
	{
		return !NodeLess(A, B) && !NodeLess(B, A);
	}

	bool PortalLess(const FLNPNavPortal& A, const FLNPNavPortal& B)
	{
		return NodeLess(A.A, B.A) || (NodeEqual(A.A, B.A)
			&& (NodeLess(A.B, B.B) || (NodeEqual(A.B, B.B)
				&& (static_cast<uint8>(A.Flags) != static_cast<uint8>(B.Flags)
					? static_cast<uint8>(A.Flags) < static_cast<uint8>(B.Flags)
					: A.MinClearanceClass < B.MinClearanceClass))));
	}

	bool SeamLess(const FLNPNavSeamEndpoint& A, const FLNPNavSeamEndpoint& B)
	{
		return A.Edge != B.Edge ? static_cast<uint8>(A.Edge) < static_cast<uint8>(B.Edge)
			: A.SeamStep != B.SeamStep ? A.SeamStep < B.SeamStep
			: NodeLess(A.Node, B.Node);
	}

	uint64 CoordKey(const int32 I, const int32 J)
	{
		return (static_cast<uint64>(static_cast<uint32>(I)) << 32)
			| static_cast<uint32>(J);
	}

	FLNPNavData MakeCanonicalNavigation(const FLNPNavData& Data)
	{
		FLNPNavData Canonical = Data;
		Canonical.Layers.Sort(LayerLess);
		for (FLNPNavLayer& Layer : Canonical.Layers)
		{
			Layer.Tiles.Sort(TileLess);
			for (FLNPNavTile& Tile : Layer.Tiles)
			{
				Tile.Cells.Sort([](const FLNPNavCell& A, const FLNPNavCell& B)
				{
					return A.LocalCellIndex < B.LocalCellIndex;
				});
			}
		}
		return Canonical;
	}

	FLNPNavTraversalData MakeCanonicalTraversal(const FLNPNavTraversalData& Traversal)
	{
		FLNPNavTraversalData Canonical = Traversal;
		Canonical.StaticComponents.Sort([](const FLNPNavStaticComponent& A, const FLNPNavStaticComponent& B)
		{
			return A.LocalStaticComponentId < B.LocalStaticComponentId;
		});
		for (FLNPNavPortal& Portal : Canonical.Portals)
		{
			if (EnumHasAnyFlags(Portal.Flags, ELNPNavPortalFlags::Bidirectional) && NodeLess(Portal.B, Portal.A))
			{
				Swap(Portal.A, Portal.B);
			}
		}
		Canonical.Portals.Sort(PortalLess);
		Canonical.SeamEndpoints.Sort(SeamLess);
		return Canonical;
	}

	void SerializeNode(FArchive& Archive, FLNPLocalNavNodeRef& Node)
	{
		uint16 ReservedWord = 0;
		Archive << Node.LocalNavLayerId << Node.TileId << Node.LocalCellIndex << ReservedWord;
		if (Archive.IsLoading() && ReservedWord != 0)
		{
			Archive.SetError();
		}
	}
}

uint8 LNPNavData::GetNeighborBit(const ELNPNavNeighbor Neighbor)
{
	return 1u << static_cast<uint8>(Neighbor);
}

ELNPNavNeighbor LNPNavData::GetOppositeNeighbor(const ELNPNavNeighbor Neighbor)
{
	return static_cast<ELNPNavNeighbor>(static_cast<uint8>(Neighbor) ^ 1u);
}

bool LNPNavData::IsValidGridCoord(const int32 Subdivisions, const int32 I, const int32 J)
{
	return Subdivisions >= 0 && I >= 0 && J >= 0 && I + J <= Subdivisions;
}

bool LNPNavData::TryGetNeighborCoord(
	const int32 Subdivisions, const int32 I, const int32 J,
	const ELNPNavNeighbor Neighbor, FIntPoint& OutCoord)
{
	static constexpr int32 Offsets[6][2] = {
		{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, -1}, {-1, 1}
	};
	const uint8 Direction = static_cast<uint8>(Neighbor);
	if (Direction >= UE_ARRAY_COUNT(Offsets))
	{
		return false;
	}
	OutCoord = FIntPoint(I + Offsets[Direction][0], J + Offsets[Direction][1]);
	return IsValidGridCoord(Subdivisions, OutCoord.X, OutCoord.Y);
}

bool LNPNavData::MakeTileAddress(
	const int32 I, const int32 J, uint16& OutTileX, uint16& OutTileY, uint8& OutLocalCellIndex)
{
	if (I < 0 || J < 0 || I / TileSide > MAX_uint16 || J / TileSide > MAX_uint16)
	{
		return false;
	}
	OutTileX = static_cast<uint16>(I / TileSide);
	OutTileY = static_cast<uint16>(J / TileSide);
	OutLocalCellIndex = static_cast<uint8>((J % TileSide) * TileSide + I % TileSide);
	return true;
}

FIntPoint LNPNavData::DecodeTileAddress(
	const uint16 TileX, const uint16 TileY, const uint8 LocalCellIndex)
{
	return FIntPoint(
		static_cast<int32>(TileX) * TileSide + LocalCellIndex % TileSide,
		static_cast<int32>(TileY) * TileSide + LocalCellIndex / TileSide);
}

const FLNPNavLayer* LNPNavData::FindLayer(const FLNPNavData& Data, const uint16 LocalNavLayerId)
{
	return Data.Layers.FindByPredicate([LocalNavLayerId](const FLNPNavLayer& Layer)
	{
		return Layer.LocalNavLayerId == LocalNavLayerId;
	});
}

const FLNPNavCell* LNPNavData::ResolveLocalNode(
	const FLNPNavData& Data, const FLNPLocalNavNodeRef& Node, FIntPoint* OutCoord)
{
	const FLNPNavLayer* Layer = FindLayer(Data, Node.LocalNavLayerId);
	if (Layer == nullptr)
	{
		return nullptr;
	}
	const FLNPNavTile* Tile = Layer->Tiles.FindByPredicate([&Node](const FLNPNavTile& Candidate)
	{
		return Candidate.TileId == Node.TileId;
	});
	if (Tile == nullptr)
	{
		return nullptr;
	}
	const FLNPNavCell* Cell = Tile->Cells.FindByPredicate([&Node](const FLNPNavCell& Candidate)
	{
		return Candidate.LocalCellIndex == Node.LocalCellIndex;
	});
	if (Cell != nullptr && OutCoord != nullptr)
	{
		*OutCoord = DecodeTileAddress(Tile->TileX, Tile->TileY, Cell->LocalCellIndex);
	}
	return Cell;
}

bool LNPNavData::MakeLocalNodeRef(
	const FLNPNavData& Data, const uint16 LocalNavLayerId,
	const int32 I, const int32 J, FLNPLocalNavNodeRef& OutNode)
{
	OutNode = FLNPLocalNavNodeRef();
	const FLNPNavLayer* Layer = FindLayer(Data, LocalNavLayerId);
	uint16 TileX = 0;
	uint16 TileY = 0;
	uint8 LocalCellIndex = 0;
	if (Layer == nullptr || !IsValidGridCoord(Layer->Subdivisions, I, J)
		|| !MakeTileAddress(I, J, TileX, TileY, LocalCellIndex))
	{
		return false;
	}
	const FLNPNavTile* Tile = Layer->Tiles.FindByPredicate([TileX, TileY](const FLNPNavTile& Candidate)
	{
		return Candidate.TileX == TileX && Candidate.TileY == TileY;
	});
	if (Tile == nullptr || !Tile->Cells.ContainsByPredicate([LocalCellIndex](const FLNPNavCell& Cell)
		{ return Cell.LocalCellIndex == LocalCellIndex; }))
	{
		return false;
	}
	OutNode.LocalNavLayerId = LocalNavLayerId;
	OutNode.TileId = Tile->TileId;
	OutNode.LocalCellIndex = LocalCellIndex;
	return true;
}

bool LNPNavData::IsCurrentNodeRef(const FLNPNavNodeRef& Node, const uint64 SnapshotGeneration)
{
	return Node.IsValid() && SnapshotGeneration != 0 && Node.SnapshotGeneration == SnapshotGeneration;
}

bool LNPNavData::ValidateNavigation(const FLNPNavData& Data, FString& OutError)
{
	OutError.Reset();
	auto Fail = [&OutError](FString Error)
	{
		OutError = MoveTemp(Error);
		return false;
	};
	if (!IsFinite(Data.Agent) || !(Data.Agent.Radius > 0.0f) || Data.Agent.HalfHeight < Data.Agent.Radius
		|| Data.Agent.MaxStepUp < 0.0f || Data.Agent.MaxStepDown < 0.0f
		|| Data.Agent.WalkableMinDot < 0.0f || Data.Agent.WalkableMinDot > 1.0f
		|| Data.Layers.Num() > MaxLayerCount || Data.LocalStaticComponentCount == MAX_uint16)
	{
		return Fail(TEXT("Invalid Navigation header or agent profile."));
	}

	int64 TotalTiles = 0;
	int64 TotalCells = 0;
	TArray<uint32> ComponentNodeCounts;
	ComponentNodeCounts.SetNumZeroed(Data.LocalStaticComponentCount);
	uint16 PreviousLayer = 0;
	bool bHavePreviousLayer = false;
	for (const FLNPNavLayer& Layer : Data.Layers)
	{
		if (Layer.LocalNavLayerId == MAX_uint16 || Layer.LocalSupportLayerId != Layer.LocalNavLayerId
			|| Layer.Subdivisions < 1 || (bHavePreviousLayer && PreviousLayer >= Layer.LocalNavLayerId))
		{
			return Fail(FString::Printf(TEXT("Invalid or unsorted Navigation Layer %u."), Layer.LocalNavLayerId));
		}
		PreviousLayer = Layer.LocalNavLayerId;
		bHavePreviousLayer = true;
		TotalTiles += Layer.Tiles.Num();
		if (TotalTiles > MaxTileCount || Layer.Tiles.Num() > MAX_uint16)
		{
			return Fail(FString::Printf(TEXT("Navigation Layer %u has too many Tiles."), Layer.LocalNavLayerId));
		}

		TMap<uint64, uint8> EdgeMaskByCoord;
		for (int32 TileIndex = 0; TileIndex < Layer.Tiles.Num(); ++TileIndex)
		{
			const FLNPNavTile& Tile = Layer.Tiles[TileIndex];
			if (Tile.TileId != TileIndex || Tile.InitialRevision != 0 || Tile.Cells.IsEmpty()
				|| (TileIndex > 0 && !TileLess(Layer.Tiles[TileIndex - 1], Tile)))
			{
				return Fail(FString::Printf(TEXT("Navigation Layer %u has a non-canonical Tile %u."),
					Layer.LocalNavLayerId, Tile.TileId));
			}
			TotalCells += Tile.Cells.Num();
			if (TotalCells > MaxCellCount || Tile.Cells.Num() > MaxCellsPerTile)
			{
				return Fail(TEXT("Navigation contains too many cells."));
			}
			uint8 PreviousCell = 0;
			bool bHavePreviousCell = false;
			for (const FLNPNavCell& Cell : Tile.Cells)
			{
				const FIntPoint Coord = DecodeTileAddress(Tile.TileX, Tile.TileY, Cell.LocalCellIndex);
				const uint8 Flags = static_cast<uint8>(Cell.Flags);
				const uint8 AllowedFlags = static_cast<uint8>(
					ELNPNavCellFlags::Walkable | ELNPNavCellFlags::NearStaticBlocker | ELNPNavCellFlags::NeedsExact);
				if ((bHavePreviousCell && PreviousCell >= Cell.LocalCellIndex)
					|| !IsValidGridCoord(Layer.Subdivisions, Coord.X, Coord.Y)
					|| (Cell.EdgeMask & ~NeighborMask) != 0 || (Flags & ~AllowedFlags) != 0
					|| !EnumHasAnyFlags(Cell.Flags, ELNPNavCellFlags::Walkable)
					|| Cell.LocalStaticComponentId >= Data.LocalStaticComponentCount)
				{
					return Fail(FString::Printf(TEXT("Navigation Layer %u Tile %u has invalid cell %u."),
						Layer.LocalNavLayerId, Tile.TileId, Cell.LocalCellIndex));
				}
				PreviousCell = Cell.LocalCellIndex;
				bHavePreviousCell = true;
				const uint64 Key = CoordKey(Coord.X, Coord.Y);
				if (EdgeMaskByCoord.Contains(Key))
				{
					return Fail(FString::Printf(TEXT("Navigation Layer %u has duplicate coordinate (%d,%d)."),
						Layer.LocalNavLayerId, Coord.X, Coord.Y));
				}
				EdgeMaskByCoord.Add(Key, Cell.EdgeMask);
				++ComponentNodeCounts[Cell.LocalStaticComponentId];
			}
		}

		for (const TPair<uint64, uint8>& Entry : EdgeMaskByCoord)
		{
			const int32 I = static_cast<int32>(Entry.Key >> 32);
			const int32 J = static_cast<int32>(Entry.Key & 0xffffffffu);
			for (uint8 Direction = 0; Direction < 6; ++Direction)
			{
				const uint8 Bit = 1u << Direction;
				if ((Entry.Value & Bit) == 0)
				{
					continue;
				}
				FIntPoint NeighborCoord;
				if (!TryGetNeighborCoord(Layer.Subdivisions, I, J, static_cast<ELNPNavNeighbor>(Direction), NeighborCoord))
				{
					return Fail(FString::Printf(TEXT("Navigation edge leaves Layer %u at (%d,%d)."),
						Layer.LocalNavLayerId, I, J));
				}
				const uint8* NeighborMaskValue = EdgeMaskByCoord.Find(CoordKey(NeighborCoord.X, NeighborCoord.Y));
				const uint8 OppositeBit = GetNeighborBit(GetOppositeNeighbor(static_cast<ELNPNavNeighbor>(Direction)));
				if (NeighborMaskValue == nullptr || (*NeighborMaskValue & OppositeBit) == 0)
				{
					return Fail(FString::Printf(TEXT("Navigation edge is missing its reciprocal in Layer %u at (%d,%d)."),
						Layer.LocalNavLayerId, I, J));
				}
			}
		}
	}
	for (int32 Component = 0; Component < ComponentNodeCounts.Num(); ++Component)
	{
		if (ComponentNodeCounts[Component] == 0)
		{
			return Fail(FString::Printf(TEXT("Navigation StaticComponent %d has no nodes."), Component));
		}
	}
	return true;
}

bool LNPNavData::EncodeNavigation(const FLNPNavData& Data, TArray<uint8>& OutPayload, FString& OutError)
{
	OutPayload.Reset();
	FLNPNavData Canonical = MakeCanonicalNavigation(Data);
	if (!ValidateNavigation(Canonical, OutError))
	{
		return false;
	}
	uint32 TileCount = 0;
	uint32 CellCount = 0;
	for (const FLNPNavLayer& Layer : Canonical.Layers)
	{
		TileCount += Layer.Tiles.Num();
		for (const FLNPNavTile& Tile : Layer.Tiles)
		{
			CellCount += Tile.Cells.Num();
		}
	}

	FMemoryWriter Writer(OutPayload, true);
	Writer.SetByteSwapping(!PLATFORM_LITTLE_ENDIAN);
	uint16 Version = NavigationCodecVersion;
	uint16 TileSideValue = TileSide;
	uint16 LayerCount = static_cast<uint16>(Canonical.Layers.Num());
	uint16 ComponentCount = Canonical.LocalStaticComponentCount;
	Writer << Version << TileSideValue << LayerCount << ComponentCount << TileCount << CellCount;
	Writer << Canonical.Agent.Radius << Canonical.Agent.HalfHeight << Canonical.Agent.MaxStepUp
		<< Canonical.Agent.MaxStepDown << Canonical.Agent.WalkableMinDot;
	for (FLNPNavLayer& Layer : Canonical.Layers)
	{
		uint32 LayerTileCount = Layer.Tiles.Num();
		Writer << Layer.LocalNavLayerId << Layer.LocalSupportLayerId << Layer.Subdivisions << LayerTileCount;
		for (FLNPNavTile& Tile : Layer.Tiles)
		{
			uint16 Reserved = 0;
			uint16 CellCountInTile = static_cast<uint16>(Tile.Cells.Num());
			Writer << Tile.TileId << Tile.TileX << Tile.TileY << Reserved << Tile.InitialRevision << CellCountInTile << Reserved;
			for (FLNPNavCell& Cell : Tile.Cells)
			{
				uint8 Flags = static_cast<uint8>(Cell.Flags);
				Writer << Cell.LocalCellIndex << Cell.EdgeMask << Cell.ClearanceClass << Flags
					<< Cell.LocalStaticComponentId << Reserved;
			}
		}
	}
	if (Writer.IsError())
	{
		OutPayload.Reset();
		OutError = TEXT("Failed to encode Navigation payload.");
		return false;
	}
	return true;
}

bool LNPNavData::DecodeNavigation(
	const TConstArrayView<uint8> Payload, FLNPNavData& OutData, FString& OutError)
{
	OutData = FLNPNavData();
	OutError.Reset();
	if (Payload.Num() < 36)
	{
		OutError = TEXT("Navigation payload is truncated before its header.");
		return false;
	}
	FMemoryReaderView Reader(Payload, true);
	Reader.SetByteSwapping(!PLATFORM_LITTLE_ENDIAN);
	uint16 Version = 0;
	uint16 TileSideValue = 0;
	uint16 LayerCount = 0;
	uint16 ComponentCount = 0;
	uint32 ExpectedTileCount = 0;
	uint32 ExpectedCellCount = 0;
	Reader << Version << TileSideValue << LayerCount << ComponentCount << ExpectedTileCount << ExpectedCellCount;
	Reader << OutData.Agent.Radius << OutData.Agent.HalfHeight << OutData.Agent.MaxStepUp
		<< OutData.Agent.MaxStepDown << OutData.Agent.WalkableMinDot;
	if (Version != NavigationCodecVersion || TileSideValue != TileSide || LayerCount > MaxLayerCount
		|| ExpectedTileCount > MaxTileCount || ExpectedCellCount > MaxCellCount || ComponentCount == MAX_uint16)
	{
		OutError = FString::Printf(TEXT("Invalid Navigation payload header (version=%u tileSide=%u layers=%u tiles=%u cells=%u)."),
			Version, TileSideValue, LayerCount, ExpectedTileCount, ExpectedCellCount);
		return false;
	}
	OutData.LocalStaticComponentCount = ComponentCount;
	OutData.Layers.SetNum(LayerCount);
	uint32 ActualTileCount = 0;
	uint32 ActualCellCount = 0;
	for (FLNPNavLayer& Layer : OutData.Layers)
	{
		uint32 LayerTileCount = 0;
		Reader << Layer.LocalNavLayerId << Layer.LocalSupportLayerId << Layer.Subdivisions << LayerTileCount;
		if (Reader.IsError() || LayerTileCount > MaxTileCount - ActualTileCount)
		{
			OutData = FLNPNavData();
			OutError = TEXT("Navigation payload has an invalid Layer table.");
			return false;
		}
		ActualTileCount += LayerTileCount;
		Layer.Tiles.SetNum(LayerTileCount);
		for (FLNPNavTile& Tile : Layer.Tiles)
		{
			uint16 ReservedA = 0;
			uint16 ReservedB = 0;
			uint16 CellCountInTile = 0;
			Reader << Tile.TileId << Tile.TileX << Tile.TileY << ReservedA << Tile.InitialRevision << CellCountInTile << ReservedB;
			if (Reader.IsError() || ReservedA != 0 || ReservedB != 0 || CellCountInTile > MaxCellsPerTile
				|| CellCountInTile > ExpectedCellCount - FMath::Min(ExpectedCellCount, ActualCellCount))
			{
				OutData = FLNPNavData();
				OutError = TEXT("Navigation payload has an invalid Tile table.");
				return false;
			}
			ActualCellCount += CellCountInTile;
			Tile.Cells.SetNum(CellCountInTile);
			for (FLNPNavCell& Cell : Tile.Cells)
			{
				uint8 Flags = 0;
				uint16 Reserved = 0;
				Reader << Cell.LocalCellIndex << Cell.EdgeMask << Cell.ClearanceClass << Flags
					<< Cell.LocalStaticComponentId << Reserved;
				Cell.Flags = static_cast<ELNPNavCellFlags>(Flags);
				if (Reserved != 0)
				{
					Reader.SetError();
				}
			}
		}
	}
	if (Reader.IsError() || Reader.Tell() != Payload.Num()
		|| ActualTileCount != ExpectedTileCount || ActualCellCount != ExpectedCellCount
		|| !ValidateNavigation(OutData, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Navigation payload is truncated, malformed, has trailing bytes, or mismatched counts.");
		}
		OutData = FLNPNavData();
		return false;
	}
	return true;
}

bool LNPNavData::ValidateTraversal(
	const FLNPNavTraversalData& Traversal, const FLNPNavData& Navigation, FString& OutError)
{
	OutError.Reset();
	auto Fail = [&OutError](FString Error)
	{
		OutError = MoveTemp(Error);
		return false;
	};
	FString NavigationError;
	if (!ValidateNavigation(Navigation, NavigationError))
	{
		return Fail(FString::Printf(TEXT("Traversal references invalid Navigation: %s"), *NavigationError));
	}
	if (Traversal.StaticComponents.Num() != Navigation.LocalStaticComponentCount
		|| Traversal.Portals.Num() > MaxTraversalRecordCount || Traversal.SeamEndpoints.Num() > MaxTraversalRecordCount)
	{
		return Fail(TEXT("Traversal header counts do not match Navigation."));
	}
	TArray<uint32> ActualNodeCounts;
	ActualNodeCounts.SetNumZeroed(Navigation.LocalStaticComponentCount);
	for (const FLNPNavLayer& Layer : Navigation.Layers)
	{
		for (const FLNPNavTile& Tile : Layer.Tiles)
		{
			for (const FLNPNavCell& Cell : Tile.Cells)
			{
				++ActualNodeCounts[Cell.LocalStaticComponentId];
			}
		}
	}
	for (int32 Index = 0; Index < Traversal.StaticComponents.Num(); ++Index)
	{
		const FLNPNavStaticComponent& Component = Traversal.StaticComponents[Index];
		if (Component.LocalStaticComponentId != Index || Component.NodeCount != ActualNodeCounts[Index])
		{
			return Fail(FString::Printf(TEXT("Traversal StaticComponent %d has a non-canonical ID or node count."), Index));
		}
	}

	for (int32 Index = 0; Index < Traversal.Portals.Num(); ++Index)
	{
		const FLNPNavPortal& Portal = Traversal.Portals[Index];
		const uint8 Flags = static_cast<uint8>(Portal.Flags);
		const FLNPNavCell* A = ResolveLocalNode(Navigation, Portal.A);
		const FLNPNavCell* B = ResolveLocalNode(Navigation, Portal.B);
		if (A == nullptr || B == nullptr || NodeEqual(Portal.A, Portal.B)
			|| Portal.A.LocalNavLayerId == Portal.B.LocalNavLayerId
			|| (Flags & ~static_cast<uint8>(ELNPNavPortalFlags::Bidirectional)) != 0
			|| Portal.MinClearanceClass > FMath::Min(A->ClearanceClass, B->ClearanceClass)
			|| (EnumHasAnyFlags(Portal.Flags, ELNPNavPortalFlags::Bidirectional) && NodeLess(Portal.B, Portal.A))
			|| (Index > 0 && !PortalLess(Traversal.Portals[Index - 1], Portal)))
		{
			return Fail(FString::Printf(TEXT("Traversal portal %d is invalid, duplicated, or unsorted."), Index));
		}
	}

	for (int32 Index = 0; Index < Traversal.SeamEndpoints.Num(); ++Index)
	{
		const FLNPNavSeamEndpoint& Endpoint = Traversal.SeamEndpoints[Index];
		FIntPoint Coord;
		const FLNPNavCell* Cell = ResolveLocalNode(Navigation, Endpoint.Node, &Coord);
		const FLNPNavLayer* Layer = FindLayer(Navigation, Endpoint.Node.LocalNavLayerId);
		const uint8 Edge = static_cast<uint8>(Endpoint.Edge);
		if (Cell == nullptr || Layer == nullptr || Endpoint.Node.LocalNavLayerId != 0 || Edge >= 3
			|| Endpoint.SeamStep > Layer->Subdivisions || Endpoint.MinClearanceClass > Cell->ClearanceClass
			|| Coord != LNPCrustAtlas::GetSeamSampleCoord(Layer->Subdivisions, Endpoint.Edge, Endpoint.SeamStep)
			|| (Index > 0 && (!SeamLess(Traversal.SeamEndpoints[Index - 1], Endpoint)
				|| (Traversal.SeamEndpoints[Index - 1].Edge == Endpoint.Edge
					&& Traversal.SeamEndpoints[Index - 1].SeamStep == Endpoint.SeamStep))))
		{
			return Fail(FString::Printf(TEXT("Traversal seam endpoint %d is invalid, duplicated, or unsorted."), Index));
		}
	}
	return true;
}

bool LNPNavData::EncodeTraversal(
	const FLNPNavTraversalData& Traversal, const FLNPNavData& Navigation,
	TArray<uint8>& OutPayload, FString& OutError)
{
	OutPayload.Reset();
	FLNPNavTraversalData Canonical = MakeCanonicalTraversal(Traversal);
	if (!ValidateTraversal(Canonical, Navigation, OutError))
	{
		return false;
	}
	FMemoryWriter Writer(OutPayload, true);
	Writer.SetByteSwapping(!PLATFORM_LITTLE_ENDIAN);
	uint16 Version = TraversalCodecVersion;
	uint16 Reserved = 0;
	uint32 ComponentCount = Canonical.StaticComponents.Num();
	uint32 PortalCount = Canonical.Portals.Num();
	uint32 SeamCount = Canonical.SeamEndpoints.Num();
	Writer << Version << Reserved << ComponentCount << PortalCount << SeamCount;
	for (FLNPNavStaticComponent& Component : Canonical.StaticComponents)
	{
		Writer << Component.LocalStaticComponentId << Reserved << Component.NodeCount;
	}
	for (FLNPNavPortal& Portal : Canonical.Portals)
	{
		SerializeNode(Writer, Portal.A);
		SerializeNode(Writer, Portal.B);
		uint8 Flags = static_cast<uint8>(Portal.Flags);
		Writer << Portal.MinClearanceClass << Flags << Reserved;
	}
	for (FLNPNavSeamEndpoint& Endpoint : Canonical.SeamEndpoints)
	{
		SerializeNode(Writer, Endpoint.Node);
		uint8 Edge = static_cast<uint8>(Endpoint.Edge);
		uint8 ReservedByte = 0;
		Writer << Endpoint.SeamStep << Edge << Endpoint.MinClearanceClass << ReservedByte;
	}
	if (Writer.IsError())
	{
		OutPayload.Reset();
		OutError = TEXT("Failed to encode Traversal payload.");
		return false;
	}
	return true;
}

bool LNPNavData::DecodeTraversal(
	const TConstArrayView<uint8> Payload, const FLNPNavData& Navigation,
	FLNPNavTraversalData& OutTraversal, FString& OutError)
{
	OutTraversal = FLNPNavTraversalData();
	OutError.Reset();
	if (Payload.Num() < 16)
	{
		OutError = TEXT("Traversal payload is truncated before its header.");
		return false;
	}
	FMemoryReaderView Reader(Payload, true);
	Reader.SetByteSwapping(!PLATFORM_LITTLE_ENDIAN);
	uint16 Version = 0;
	uint16 Reserved = 0;
	uint32 ComponentCount = 0;
	uint32 PortalCount = 0;
	uint32 SeamCount = 0;
	Reader << Version << Reserved << ComponentCount << PortalCount << SeamCount;
	if (Version != TraversalCodecVersion || Reserved != 0
		|| ComponentCount > MaxTraversalRecordCount || PortalCount > MaxTraversalRecordCount
		|| SeamCount > MaxTraversalRecordCount)
	{
		OutError = TEXT("Invalid Traversal payload header.");
		return false;
	}
	OutTraversal.StaticComponents.SetNum(ComponentCount);
	for (FLNPNavStaticComponent& Component : OutTraversal.StaticComponents)
	{
		uint16 RecordReserved = 0;
		Reader << Component.LocalStaticComponentId << RecordReserved << Component.NodeCount;
		if (RecordReserved != 0)
		{
			Reader.SetError();
		}
	}
	OutTraversal.Portals.SetNum(PortalCount);
	for (FLNPNavPortal& Portal : OutTraversal.Portals)
	{
		SerializeNode(Reader, Portal.A);
		SerializeNode(Reader, Portal.B);
		uint8 Flags = 0;
		uint16 RecordReserved = 0;
		Reader << Portal.MinClearanceClass << Flags << RecordReserved;
		Portal.Flags = static_cast<ELNPNavPortalFlags>(Flags);
		if (RecordReserved != 0)
		{
			Reader.SetError();
		}
	}
	OutTraversal.SeamEndpoints.SetNum(SeamCount);
	for (FLNPNavSeamEndpoint& Endpoint : OutTraversal.SeamEndpoints)
	{
		SerializeNode(Reader, Endpoint.Node);
		uint8 Edge = 0;
		uint8 RecordReserved = 0;
		Reader << Endpoint.SeamStep << Edge << Endpoint.MinClearanceClass << RecordReserved;
		Endpoint.Edge = static_cast<ELNPCrustSeamEdge>(Edge);
		if (RecordReserved != 0)
		{
			Reader.SetError();
		}
	}
	if (Reader.IsError() || Reader.Tell() != Payload.Num()
		|| !ValidateTraversal(OutTraversal, Navigation, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Traversal payload is truncated, malformed, or has trailing bytes.");
		}
		OutTraversal = FLNPNavTraversalData();
		return false;
	}
	return true;
}
