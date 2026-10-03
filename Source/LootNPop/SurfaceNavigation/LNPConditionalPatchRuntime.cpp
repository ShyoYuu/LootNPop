// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPConditionalPatchRuntime.h"

#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"

namespace
{
	uint64 EdgeKey(int32 From, int32 To)
	{
		return (static_cast<uint64>(static_cast<uint32>(From)) << 32) | static_cast<uint32>(To);
	}

	int32 GraphNode(const FLNPNavSnapshot& Nav, int32 Slot, const FLNPLocalNavNodeRef& Local)
	{
		FLNPNavNodeRef Ref;
		return LNPNavRuntime::MakeRuntimeNodeRef(Nav, Slot, Local, Ref) ? LNPNavGraph::ToGraphNode(Nav, Ref) : INDEX_NONE;
	}

	void RebuildGroups(FLNPNavSnapshot& Nav)
	{
		// 삭제를 역 union하지 않는다. Pod는 제외하고 patch가 바꾼 실제 보행 graph의 연결 성분을 다시 구한다.
		Nav.ReachabilityGroupByNode.Init(MAX_uint32, Nav.Graph.GetNodeCount());
		Nav.ReachabilityGroupCount = 0;
		TArray<int32> Queue;
		for (int32 Node = 0; Node < Nav.Graph.GetNodeCount(); ++Node)
		{
			if (Nav.Graph.BlockedNodes[Node] || Nav.ReachabilityGroupByNode[Node] != MAX_uint32) { continue; }
			const uint32 Group = Nav.ReachabilityGroupCount++;
			Queue.Reset();
			Queue.Add(Node);
			Nav.ReachabilityGroupByNode[Node] = Group;
			for (int32 Index = 0; Index < Queue.Num(); ++Index)
			{
				const int32 From = Queue[Index];
				Nav.Graph.ForEachNeighbor(Nav.Graph.GetSlot(From), From, [&](int32 To)
				{
					if (Nav.ReachabilityGroupByNode[To] == MAX_uint32)
					{
						Nav.ReachabilityGroupByNode[To] = Group;
						Queue.Add(To);
					}
				});
			}
		}
		Nav.ReachabilityGroupByStaticComponent.Init(MAX_uint32, Nav.RuntimeStaticComponentCount);
		TBitArray<> Seen(false, Nav.RuntimeStaticComponentCount);
		for (int32 Node = 0; Node < Nav.Graph.GetNodeCount(); ++Node)
		{
			const uint32 Group = Nav.ReachabilityGroupByNode[Node];
			if (Group == MAX_uint32) { continue; }
			const int32 Slot = Nav.Graph.GetSlot(Node);
			const uint32 Component = LNPNavRuntime::GetRuntimeStaticComponent(Nav, Slot, Nav.Graph.GetNode(Slot, Node).LocalComponent);
			uint32& Stored = Nav.ReachabilityGroupByStaticComponent[Component];
			if (!Seen[Component]) { Stored = Group; Seen[Component] = true; }
			else if (Stored != Group) { Stored = MAX_uint32; }
		}
	}
}

bool LNPConditionalPatchRuntime::Prepare(FLNPSurfaceDataSnapshot& Snapshot, FString& OutError)
{
	TMap<const FLNPConditionalPatchData*, FLNPNavSlotInput> CombinedByAsset;
	TArray<FLNPNavSlotInput> Inputs;
	bool bHasPatch = false;
	for (FLNPSurfaceDataSlotSnapshot& Slot : Snapshot.Slots)
	{
		if (!Slot.ConditionalPatches.IsValid() || Slot.ConditionalPatches->Patches.IsEmpty())
		{
			Inputs.Add({Slot.Support, Slot.Navigation, Slot.Traversal});
			continue;
		}
		bHasPatch = true;
		FLNPNavSlotInput* Combined = CombinedByAsset.Find(Slot.ConditionalPatches.Get());
		if (Combined == nullptr)
		{
			auto Support = MakeShared<FLNPSupportAtlas, ESPMode::ThreadSafe>(*Slot.Support);
			auto Nav = MakeShared<FLNPNavData, ESPMode::ThreadSafe>(*Slot.Navigation);
			for (const FLNPConditionalPatch& Patch : Slot.ConditionalPatches->Patches)
			{
				if (Nav->LocalStaticComponentCount + Patch.Navigation.LocalStaticComponentCount >= MAX_uint16
					|| Support->Layers.Num() + Patch.SupportLayers.Num() > MAX_uint8 + 1)
				{
					OutError = TEXT("Combined patch graph exceeds component or Layer ordinal range.");
					return false;
				}
				Support->Layers.Append(Patch.SupportLayers);
				for (FLNPNavLayer Layer : Patch.Navigation.Layers)
				{
					for (FLNPNavTile& Tile : Layer.Tiles)
					{
						for (FLNPNavCell& Cell : Tile.Cells)
						{
							Cell.LocalStaticComponentId += Nav->LocalStaticComponentCount;
						}
					}
					Nav->Layers.Add(MoveTemp(Layer));
				}
				Nav->LocalStaticComponentCount += Patch.Navigation.LocalStaticComponentCount;
			}
			Combined = &CombinedByAsset.Add(Slot.ConditionalPatches.Get(), {Support, Nav, Slot.Traversal});
		}
		const int32 StaticLayers = Slot.Support->Layers.Num();
		Slot.Support = Combined->Support;
		Slot.Navigation = Combined->Navigation;
		Slot.ActiveSupportLayers.Init(false, Slot.Support->Layers.Num());
		for (int32 Layer = 0; Layer < StaticLayers; ++Layer) { Slot.ActiveSupportLayers[Layer] = true; }
		Inputs.Add(*Combined);
	}
	if (!bHasPatch) { return true; }
	if (!LNPNavRuntime::BuildSnapshot(Inputs, MakeArrayView(ULNPOctantSpawnSubsystem::OctantRotations),
		Snapshot.Generation, Snapshot.Nav, OutError)) { return false; }
	for (int32 Node = 0; Node < Snapshot.Nav.Graph.GetNodeCount(); ++Node)
	{
		const int32 Slot = Snapshot.Nav.Graph.GetSlot(Node);
		const auto& Data = Snapshot.Slots[Slot];
		const auto& Graph = Snapshot.Nav.Graph;
		const int32 Layer = Graph.SlotGraphs[Slot]->Layers[Graph.GetNode(Slot, Node).LayerOrdinal].LocalNavLayerId;
		if (!Data.ActiveSupportLayers.IsEmpty() && !Data.ActiveSupportLayers[Layer]) { Snapshot.Nav.Graph.BlockedNodes[Node] = true; }
	}
	RebuildGroups(Snapshot.Nav);
	return true;
}

bool LNPConditionalPatchRuntime::BuildActiveSnapshot(const FLNPSurfaceDataSnapshot& Base,
	TConstArrayView<FLNPPlacementId> Active, const FLNPSurfaceDataSnapshot& Previous,
	FLNPSurfaceDataSnapshot& OutSnapshot, FString& OutError)
{
	OutError.Reset();
	if (!Base.Nav.Graph.IsValid() || Base.Generation != Previous.Generation)
	{
		OutError = TEXT("Patch activation requires a prepared base and the same snapshot generation.");
		return false;
	}
	TSet<FLNPPlacementId> Unique;
	for (const FLNPPlacementId& Id : Active)
	{
		if (!Id.IsValid() || !Base.Slots.IsValidIndex(Id.Slot) || !Base.Slots[Id.Slot].ConditionalPatches.IsValid()
			|| !Base.Slots[Id.Slot].ConditionalPatches->Patches.ContainsByPredicate(
				[&](const FLNPConditionalPatch& Patch) { return Patch.MarkerId == Id.MarkerId; }))
		{
			OutError = TEXT("Patch activation contains an unknown slot or MarkerId.");
			return false;
		}
		Unique.Add(Id);
	}
	FLNPSurfaceDataSnapshot Next = Base;
	Next.ActivePatches = Unique.Array();
	Next.ActivePatches.Sort([](const FLNPPlacementId& A, const FLNPPlacementId& B)
	{
		return A.Slot != B.Slot ? A.Slot < B.Slot : LNPConditionalPatch::MarkerLess(A.MarkerId, B.MarkerId);
	});
	if (Next.ActivePatches == Previous.ActivePatches) { OutSnapshot = Previous; return true; }
	FLNPNavGraph& Graph = Next.Nav.Graph;
	TSet<uint64> BlockedEdges;
	for (uint64 Key : Graph.BlockedEdges) { BlockedEdges.Add(Key); }
	TArray<int32> BlockedNodes;
	for (const FLNPPlacementId& Id : Next.ActivePatches)
	{
		const FLNPConditionalPatch& Patch = *Next.Slots[Id.Slot].ConditionalPatches->Patches.FindByPredicate(
			[&](const FLNPConditionalPatch& P) { return P.MarkerId == Id.MarkerId; });
		for (int32 Layer = Patch.FirstSupportLayer; Layer < Patch.FirstSupportLayer + Patch.SupportLayers.Num(); ++Layer)
		{
			Next.Slots[Id.Slot].ActiveSupportLayers[Layer] = true;
			const auto& Asset = *Graph.SlotGraphs[Id.Slot];
			const auto& GraphLayer = Asset.Layers[Layer];
			for (int32 Node = GraphLayer.NodeBegin; Node < GraphLayer.NodeEnd; ++Node)
			{
				Graph.BlockedNodes[Graph.SlotNodeBase[Id.Slot] + Node] = false;
			}
		}
		for (const FLNPLocalNavNodeRef& Ref : Patch.BlockedNodes) { BlockedNodes.Add(GraphNode(Next.Nav, Id.Slot, Ref)); }
		for (const FLNPConditionalPatchEdge& Edge : Patch.BlockedEdges)
		{
			const int32 A = GraphNode(Next.Nav, Id.Slot, Edge.A), B = GraphNode(Next.Nav, Id.Slot, Edge.B);
			BlockedEdges.Add(EdgeKey(A, B)); BlockedEdges.Add(EdgeKey(B, A));
		}
		for (const FLNPConditionalPatchEdge& Edge : Patch.WalkEdges)
		{
			const int32 A = GraphNode(Next.Nav, Id.Slot, Edge.A), B = GraphNode(Next.Nav, Id.Slot, Edge.B);
			Graph.ExtraLinks.Add({A, B}); Graph.ExtraLinks.Add({B, A});
		}
	}
	for (int32 Node : BlockedNodes) { Graph.BlockedNodes[Node] = true; }
	Graph.BlockedEdges = BlockedEdges.Array();
	Graph.BlockedEdges.Sort();
	Graph.HasBlockedEdge.Init(false, Graph.GetNodeCount());
	for (uint64 Key : Graph.BlockedEdges) { Graph.HasBlockedEdge[static_cast<int32>(Key >> 32)] = true; }
	Graph.ExtraLinks.Sort([](const FLNPNavGraphLink& A, const FLNPNavGraphLink& B)
	{
		return A.From != B.From ? A.From < B.From : A.To < B.To;
	});
	for (int32 Index = Graph.ExtraLinks.Num() - 1; Index > 0; --Index)
	{
		if (Graph.ExtraLinks[Index].From == Graph.ExtraLinks[Index - 1].From && Graph.ExtraLinks[Index].To == Graph.ExtraLinks[Index - 1].To)
		{
			Graph.ExtraLinks.RemoveAt(Index);
		}
	}
	for (const FLNPNavGraphLink& Link : Graph.ExtraLinks) { Graph.HasExtraLinks[Link.From] = true; }
	Next.PatchTileRevisions = Previous.PatchTileRevisions;
	TSet<uint32> ChangedTiles;
	bool bGraphChanged = false;
	for (int32 Node = 0; Node < Graph.GetNodeCount(); ++Node)
	{
		if (Graph.BlockedNodes[Node] != Previous.Nav.Graph.BlockedNodes[Node])
		{
			ChangedTiles.Add(LNPNavGraph::GetTileKey(Next.Nav, Node));
			bGraphChanged = true;
		}
	}
	// node뿐 아니라 link 양쪽 Tile도 갱신해 edge만 닫힌 경로를 빠뜨리지 않는다.
	TSet<uint64> OldEdges, NewEdges;
	for (uint64 Edge : Previous.Nav.Graph.BlockedEdges) { OldEdges.Add(Edge); }
	for (uint64 Edge : Graph.BlockedEdges) { NewEdges.Add(Edge); }
	auto TouchEdges = [&](const TSet<uint64>& From, const TSet<uint64>& To)
	{
		for (uint64 Edge : From)
		{
			if (To.Contains(Edge)) { continue; }
			ChangedTiles.Add(LNPNavGraph::GetTileKey(Next.Nav, static_cast<int32>(Edge >> 32)));
			ChangedTiles.Add(LNPNavGraph::GetTileKey(Next.Nav, static_cast<int32>(Edge)));
			bGraphChanged = true;
		}
	};
	TouchEdges(OldEdges, NewEdges);
	TouchEdges(NewEdges, OldEdges);
	OldEdges.Reset(); NewEdges.Reset();
	for (const auto& Link : Previous.Nav.Graph.ExtraLinks) { OldEdges.Add(EdgeKey(Link.From, Link.To)); }
	for (const auto& Link : Graph.ExtraLinks) { NewEdges.Add(EdgeKey(Link.From, Link.To)); }
	TouchEdges(OldEdges, NewEdges);
	TouchEdges(NewEdges, OldEdges);
	for (int32 Slot = 0; Slot < Next.Slots.Num(); ++Slot)
	{
		if (!Next.Slots[Slot].ConditionalPatches.IsValid()) { continue; }
		for (const FLNPConditionalPatch& Patch : Next.Slots[Slot].ConditionalPatches->Patches)
		{
			if (Patch.SupportLayers.IsEmpty() || Next.Slots[Slot].ActiveSupportLayers[Patch.FirstSupportLayer]
				== Previous.Slots[Slot].ActiveSupportLayers[Patch.FirstSupportLayer]) { continue; }
			for (const FLNPNavLayer& Layer : Patch.Navigation.Layers)
			{
				for (const FLNPNavTile& Tile : Layer.Tiles) { ChangedTiles.Add((static_cast<uint32>(Next.Nav.SlotLayerBase[Slot] + Layer.LocalNavLayerId) << 16) | Tile.TileId); }
			}
		}
	}
	Next.Nav.ConnectivityGraphVersion = Previous.Nav.ConnectivityGraphVersion;
	if (bGraphChanged)
	{
		RebuildGroups(Next.Nav);
		if (++Next.Nav.ConnectivityGraphVersion == 0) { Next.Nav.ConnectivityGraphVersion = 1; }
	}
	else
	{
		Next.Nav.ReachabilityGroupByNode = Previous.Nav.ReachabilityGroupByNode;
		Next.Nav.ReachabilityGroupByStaticComponent = Previous.Nav.ReachabilityGroupByStaticComponent;
		Next.Nav.ReachabilityGroupCount = Previous.Nav.ReachabilityGroupCount;
	}
	for (uint32 Tile : ChangedTiles)
	{
		uint32& Revision = Next.PatchTileRevisions.FindOrAdd(Tile);
		if (++Revision == 0) { Revision = 1; }
	}
	OutSnapshot = MoveTemp(Next);
	return true;
}
