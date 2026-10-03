// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavOverlay.h"

#include "SurfaceNavigation/LNPNavGraph.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "LootPod/LNPLootPodCollisionProxy.h"

bool LNPNavOverlay::BuildPodOverlay(const FLNPSurfaceDataSnapshot& Snapshot,
	const TConstArrayView<FLNPNavPodBlocker> Pods, const FLNPNavOverlay* Previous, FLNPNavOverlay& OutOverlay)
{
	return BuildOverlay(Snapshot, Pods, {}, Previous, OutOverlay);
}

bool LNPNavOverlay::BuildOverlay(const FLNPSurfaceDataSnapshot& Snapshot,
	const TConstArrayView<FLNPNavPodBlocker> Pods, const TConstArrayView<FLNPNavMarkerBlocker> Markers,
	const FLNPNavOverlay* Previous, FLNPNavOverlay& OutOverlay)
{
	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	const FLNPNavGraph& Graph = Nav.Graph;
	if (!Graph.IsValid())
	{
		return false;
	}
	// 잘못된 patch 주소를 일부만 적용하지 않는다. 유효한 전체 입력을 확인한 뒤 결과를 만든다.
	for (const FLNPNavMarkerBlocker& Marker : Markers)
	{
		if (!Marker.Id.IsValid() || !Snapshot.Slots.IsValidIndex(Marker.Id.Slot))
		{
			return false;
		}
		for (const FLNPNavNodeRef& Ref : Marker.Nodes)
		{
			const int32 Node = LNPNavGraph::ToGraphNode(Nav, Ref);
			if (Node == INDEX_NONE || Graph.GetSlot(Node) != Marker.Id.Slot)
			{
				return false;
			}
		}
	}
	OutOverlay = Previous ? *Previous : FLNPNavOverlay();
	OutOverlay.BlockedNodes.Init(false, Graph.GetNodeCount());
	TArray<FLNPNavGraphCandidate> Candidates;
	for (const FLNPNavPodBlocker& Pod : Pods)
	{
		const int32 Slot = Pod.Surface.OctantSlot;
		if (!Pod.Surface.IsValid() || Pod.Surface.Generation != Nav.SnapshotGeneration
			|| !Snapshot.Slots.IsValidIndex(Slot) || !Snapshot.Slots[Slot].Navigation.IsValid())
		{
			continue;
		}
		const FLNPNavData& Data = *Snapshot.Slots[Slot].Navigation;
		const double Radius = LNPLootPodCollisionProxy::Radius + Graph.SlotGraphs[Slot]->Agent.Radius;
		for (const FLNPNavLayer& Layer : Data.Layers)
		{
			if (Layer.LocalSupportLayerId != Pod.Surface.LocalLayerId)
			{
				continue;
			}
			LNPNavGraph::CollectNodesNear(Nav, Slot, Layer.LocalNavLayerId, Pod.Location, Radius, Candidates);
			for (const FLNPNavGraphCandidate& Candidate : Candidates)
			{
				OutOverlay.BlockedNodes[Candidate.Node] = true;
			}
		}
	}
	for (const FLNPNavMarkerBlocker& Marker : Markers)
	{
		for (const FLNPNavNodeRef& Ref : Marker.Nodes)
		{
			OutOverlay.BlockedNodes[LNPNavGraph::ToGraphNode(Nav, Ref)] = true;
		}
	}
	// 이음매 node의 두 slot 사본은 같은 월드 위치다. 어느 쪽이든 막히면 양쪽을 막는다.
	for (const FLNPNavSeamLink& Link : Nav.SeamLinks)
	{
		const int32 A = LNPNavGraph::ToGraphNode(Nav, Link.A);
		const int32 B = LNPNavGraph::ToGraphNode(Nav, Link.B);
		if (A != INDEX_NONE && B != INDEX_NONE && (OutOverlay.BlockedNodes[A] || OutOverlay.BlockedNodes[B]))
		{
			OutOverlay.BlockedNodes[A] = true;
			OutOverlay.BlockedNodes[B] = true;
		}
	}
	TSet<uint32> ChangedTiles;
	for (const auto& Entry : Snapshot.PatchTileRevisions)
	{
		const uint32* Seen = Previous ? Previous->PatchTileRevisions.Find(Entry.Key) : nullptr;
		if (!Seen || *Seen != Entry.Value) { ChangedTiles.Add(Entry.Key); }
	}
	OutOverlay.PatchTileRevisions = Snapshot.PatchTileRevisions;
	const bool bSameSize = Previous && Previous->BlockedNodes.Num() == OutOverlay.BlockedNodes.Num();
	for (int32 Node = 0; Node < Graph.GetNodeCount(); ++Node)
	{
		if (OutOverlay.BlockedNodes[Node] != (bSameSize && Previous->BlockedNodes[Node]))
		{
			ChangedTiles.Add(LNPNavGraph::GetTileKey(Nav, Node));
		}
	}
	if (ChangedTiles.IsEmpty())
	{
		return false;
	}
	OutOverlay.Revision = Previous ? Previous->Revision + 1 : 1;
	if (OutOverlay.Revision == 0)
	{
		OutOverlay.Revision = 1;
	}
	for (const uint32 Tile : ChangedTiles)
	{
		uint32& TileRevision = OutOverlay.TileRevisions.FindOrAdd(Tile);
		++TileRevision;
		if (TileRevision == 0)
		{
			TileRevision = 1;
		}
	}
	return true;
}
