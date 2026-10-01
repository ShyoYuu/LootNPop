// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavQuery.h"

#include "Algo/BinarySearch.h"

namespace
{
	/** Tile 표는 (TileY, TileX) 순이다. 인접 좌표를 연속으로 찾을 때 마지막 Tile을 재사용한다. */
	const FLNPNavCell* FindCell(const FLNPNavLayer& Layer, const int32 I, const int32 J, const FLNPNavTile*& InOutTile)
	{
		uint16 TileX = 0;
		uint16 TileY = 0;
		uint8 LocalCellIndex = 0;
		if (!LNPNavData::IsValidGridCoord(Layer.Subdivisions, I, J)
			|| !LNPNavData::MakeTileAddress(I, J, TileX, TileY, LocalCellIndex))
		{
			return nullptr;
		}
		if (InOutTile == nullptr || InOutTile->TileX != TileX || InOutTile->TileY != TileY)
		{
			InOutTile = Layer.Tiles.FindByPredicate([TileX, TileY](const FLNPNavTile& Tile)
			{
				return Tile.TileX == TileX && Tile.TileY == TileY;
			});
			if (InOutTile == nullptr)
			{
				return nullptr;
			}
		}
		const int32 CellIndex = Algo::BinarySearchBy(
			InOutTile->Cells, LocalCellIndex, [](const FLNPNavCell& Cell) { return Cell.LocalCellIndex; });
		return CellIndex != INDEX_NONE ? &InOutTile->Cells[CellIndex] : nullptr;
	}

	/** node 방향에서 대응 Support Layer를 보간한 slot local 지면점. 베이커가 같은 조회로 node를 만들었으므로 실패하지 않는다. */
	bool QueryNodeSupport(
		const FLNPSupportAtlasLayer& SupportLayer, const int32 Subdivisions, const int32 I, const int32 J,
		FVector3d& OutLocalPoint, FVector3f& OutLocalNormal)
	{
		const FVector3d Direction = LNPSupportAtlas::GetSampleDirection(Subdivisions, I, J);
		FLNPSupportLayerQuery Query;
		if (!LNPSupportAtlas::QueryLayer(SupportLayer, Direction, Query))
		{
			return false;
		}
		OutLocalPoint = Direction * Query.Radius;
		OutLocalNormal = Query.Normal;
		return true;
	}

	const FLNPSupportAtlasLayer* FindSupportLayer(const FLNPSurfaceDataSlotSnapshot& Slot, const FLNPNavLayer& Layer)
	{
		return Slot.Support.IsValid() && Slot.Support->Layers.IsValidIndex(Layer.LocalSupportLayerId)
			? &Slot.Support->Layers[Layer.LocalSupportLayerId]
			: nullptr;
	}

	/** stale·범위 밖 ref를 걸러 slot과 decoded cell을 돌려준다. */
	const FLNPNavCell* ResolveNode(
		const FLNPSurfaceDataSnapshot& Snapshot, const FLNPNavNodeRef& Node,
		int32& OutSlot, const FLNPNavLayer*& OutLayer, FIntPoint& OutCoord)
	{
		FLNPLocalNavNodeRef Local;
		if (Snapshot.Nav.SnapshotGeneration != Snapshot.Generation
			|| !LNPNavRuntime::ResolveRuntimeNodeRef(Snapshot.Nav, Node, OutSlot, Local)
			|| !Snapshot.Slots.IsValidIndex(OutSlot) || !Snapshot.Slots[OutSlot].Navigation.IsValid())
		{
			return nullptr;
		}
		const FLNPNavData& Navigation = *Snapshot.Slots[OutSlot].Navigation;
		OutLayer = LNPNavData::FindLayer(Navigation, Local.LocalNavLayerId);
		return OutLayer != nullptr ? LNPNavData::ResolveLocalNode(Navigation, Local, &OutCoord) : nullptr;
	}
}

bool LNPNavQuery::ProjectToNode(
	const FLNPSurfaceDataSnapshot& Snapshot,
	const FVector3d& WorldPosition,
	const FLNPSurfaceHandle& Surface,
	const double MaxDistance,
	FLNPNavProjection& OutProjection)
{
	OutProjection = FLNPNavProjection();
	if (!Snapshot.Nav.IsValid() || Snapshot.Nav.SnapshotGeneration != Snapshot.Generation
		|| !Surface.IsValid() || Surface.Generation != Snapshot.Generation
		|| !Snapshot.Slots.IsValidIndex(Surface.OctantSlot) || !(MaxDistance > 0.0))
	{
		return false;
	}
	const int32 Slot = Surface.OctantSlot;
	const FLNPSurfaceDataSlotSnapshot& SlotSnapshot = Snapshot.Slots[Slot];
	if (!SlotSnapshot.Navigation.IsValid())
	{
		return false;
	}
	// Nav Layer는 Support Layer와 1:1이며 LocalNavLayerId가 LocalLayerId와 같다.
	const FLNPNavLayer* Layer = LNPNavData::FindLayer(*SlotSnapshot.Navigation, Surface.LocalLayerId);
	const FLNPSupportAtlasLayer* SupportLayer = Layer ? FindSupportLayer(SlotSnapshot, *Layer) : nullptr;
	if (SupportLayer == nullptr || !(SupportLayer->BaseRadius > 0.0))
	{
		return false;
	}

	const FVector3d Local = SlotSnapshot.WorldToSlotRotation.RotateVector(WorldPosition);
	const int32 N = Layer->Subdivisions;
	FIntPoint Min, Max;
	LNPNavData::GetGridSearchBounds(N, Local, MaxDistance, Min, Max);
	const int32 MinJ = Min.Y;
	const int32 MaxJ = Max.Y;
	const int32 MinI = Min.X;
	const int32 MaxI = Max.X;

	double BestDistanceSquared = FMath::Square(MaxDistance);
	bool bFound = false;
	const FLNPNavTile* CachedTile = nullptr;
	for (int32 J = MinJ; J <= MaxJ; ++J)
	{
		for (int32 I = MinI; I <= FMath::Min(MaxI, N - J); ++I)
		{
			if (FindCell(*Layer, I, J, CachedTile) == nullptr)
			{
				continue;
			}
			FVector3d LocalPoint;
			FVector3f LocalNormal;
			if (!QueryNodeSupport(*SupportLayer, N, I, J, LocalPoint, LocalNormal))
			{
				continue;
			}
			const FVector3d Point = SlotSnapshot.SlotRotation.RotateVector(LocalPoint);
			const double DistanceSquared = FVector3d::DistSquared(Point, WorldPosition);
			// 같은 거리면 먼저 본 (J, I) 좌표를 유지해 결과가 결정론적이다.
			if (bFound ? DistanceSquared >= BestDistanceSquared : DistanceSquared > BestDistanceSquared)
			{
				continue;
			}
			FLNPLocalNavNodeRef LocalNode;
			FLNPNavNodeRef Node;
			if (!LNPNavData::MakeLocalNodeRef(*SlotSnapshot.Navigation, Layer->LocalNavLayerId, I, J, LocalNode)
				|| !LNPNavRuntime::MakeRuntimeNodeRef(Snapshot.Nav, Slot, LocalNode, Node)
				|| LNPNavRuntime::IsBlockedSeamNode(Snapshot.Nav, Node))
			{
				continue;
			}
			BestDistanceSquared = DistanceSquared;
			bFound = true;
			OutProjection.Node = Node;
			OutProjection.Point = Point;
			OutProjection.Normal = FVector3f(SlotSnapshot.SlotRotation.RotateVector(FVector3d(LocalNormal)).GetSafeNormal());
		}
	}
	if (!bFound)
	{
		OutProjection = FLNPNavProjection();
		return false;
	}
	OutProjection.Distance = FMath::Sqrt(BestDistanceSquared);
	return true;
}

bool LNPNavQuery::GetNodeSupport(
	const FLNPSurfaceDataSnapshot& Snapshot, const FLNPNavNodeRef& Node, FVector3d& OutPoint, FVector3f& OutNormal)
{
	int32 Slot = INDEX_NONE;
	const FLNPNavLayer* Layer = nullptr;
	FIntPoint Coord;
	if (ResolveNode(Snapshot, Node, Slot, Layer, Coord) == nullptr)
	{
		return false;
	}
	const FLNPSurfaceDataSlotSnapshot& SlotSnapshot = Snapshot.Slots[Slot];
	const FLNPSupportAtlasLayer* SupportLayer = FindSupportLayer(SlotSnapshot, *Layer);
	FVector3d LocalPoint;
	FVector3f LocalNormal;
	if (SupportLayer == nullptr
		|| !QueryNodeSupport(*SupportLayer, Layer->Subdivisions, Coord.X, Coord.Y, LocalPoint, LocalNormal))
	{
		return false;
	}
	OutPoint = SlotSnapshot.SlotRotation.RotateVector(LocalPoint);
	OutNormal = FVector3f(SlotSnapshot.SlotRotation.RotateVector(FVector3d(LocalNormal)).GetSafeNormal());
	return true;
}

bool LNPNavQuery::GetStaticComponent(
	const FLNPSurfaceDataSnapshot& Snapshot, const FLNPNavNodeRef& Node, uint32& OutComponent)
{
	OutComponent = MAX_uint32;
	int32 Slot = INDEX_NONE;
	const FLNPNavLayer* Layer = nullptr;
	FIntPoint Coord;
	const FLNPNavCell* Cell = ResolveNode(Snapshot, Node, Slot, Layer, Coord);
	if (Cell == nullptr || LNPNavRuntime::IsBlockedSeamNode(Snapshot.Nav, Node))
	{
		return false;
	}
	OutComponent = LNPNavRuntime::GetRuntimeStaticComponent(Snapshot.Nav, Slot, Cell->LocalStaticComponentId);
	return OutComponent != MAX_uint32;
}

bool LNPNavQuery::GetReachabilityGroup(
	const FLNPSurfaceDataSnapshot& Snapshot, const FLNPNavNodeRef& Node, FLNPNavGroupRef& OutGroup)
{
	OutGroup = FLNPNavGroupRef();
	uint32 Component = MAX_uint32;
	if (!GetStaticComponent(Snapshot, Node, Component)
		|| !Snapshot.Nav.ReachabilityGroupByStaticComponent.IsValidIndex(Component))
	{
		return false;
	}
	OutGroup.Group = Snapshot.Nav.ReachabilityGroupByStaticComponent[Component];
	OutGroup.ConnectivityGraphVersion = Snapshot.Nav.ConnectivityGraphVersion;
	OutGroup.SnapshotGeneration = Snapshot.Nav.SnapshotGeneration;
	return true;
}

ELNPNavReachability LNPNavQuery::TestReachability(
	const FLNPNavSnapshot& Nav, const FLNPNavGroupRef& From, const FLNPNavGroupRef& To)
{
	auto IsCurrent = [&Nav](const FLNPNavGroupRef& Group)
	{
		return Group.IsValid() && Group.SnapshotGeneration == Nav.SnapshotGeneration
			&& Group.ConnectivityGraphVersion == Nav.ConnectivityGraphVersion && Group.Group < Nav.ReachabilityGroupCount;
	};
	if (!Nav.IsValid() || !IsCurrent(From) || !IsCurrent(To))
	{
		return ELNPNavReachability::Stale;
	}
	return From.Group == To.Group ? ELNPNavReachability::Reachable : ELNPNavReachability::Unreachable;
}
