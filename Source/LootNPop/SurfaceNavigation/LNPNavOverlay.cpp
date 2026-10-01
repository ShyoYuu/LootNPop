// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavOverlay.h"

#include "SurfaceNavigation/LNPNavGraph.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "LootPod/LNPLootPodCollisionProxy.h"

bool LNPNavOverlay::BuildPodOverlay(const FLNPSurfaceDataSnapshot& Snapshot,
	const TConstArrayView<FLNPNavPodBlocker> Pods, const FLNPNavOverlay* Previous, FLNPNavOverlay& OutOverlay)
{
	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	const FLNPNavGraph& Graph = Nav.Graph;
	if (!Graph.IsValid())
	{
		return false;
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
