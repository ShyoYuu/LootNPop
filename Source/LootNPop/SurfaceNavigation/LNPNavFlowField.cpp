// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavFlowField.h"

#include "SurfaceNavigation/LNPNavGraph.h"
#include "SurfaceNavigation/LNPNavOverlay.h"
#include "SurfaceNavigation/LNPNavRuntime.h"

namespace
{
	/** 거리가 작은 쪽, 같으면 index가 작은 쪽을 먼저 확정한다. */
	struct FFlowFieldOpenOrder
	{
		bool operator()(const FLNPNavOpenEntry& A, const FLNPNavOpenEntry& B) const
		{
			return A.G != B.G ? A.G < B.G : A.Node < B.Node;
		}
	};

	/** node의 field 칸. 없으면 bCreate일 때 Tile 블록을 만든다. 블록 배열이 자라므로 포인터를 다음 호출 너머로 들고 있지 않는다. */
	int32 FindBlock(const FLNPNavSnapshot& Nav, FLNPNavFlowField& Field, const int32 Node, const bool bCreate)
	{
		const uint32 TileKey = LNPNavGraph::GetTileKey(Nav, Node);
		if (const int32* Found = Field.BlockByTile.Find(TileKey))
		{
			return *Found;
		}
		if (!bCreate)
		{
			return INDEX_NONE;
		}
		const int32 Index = Field.Blocks.AddUninitialized();
		FLNPNavFlowFieldBlock& Block = Field.Blocks[Index];
		for (int32 Cell = 0; Cell < 256; ++Cell)
		{
			Block.Distance[Cell] = MAX_flt;
			Block.Next[Cell] = INDEX_NONE;
		}
		FMemory::Memzero(Block.Settled, sizeof(Block.Settled));
		Field.BlockByTile.Add(TileKey, Index);
		return Index;
	}

	uint8 GetCell(const FLNPNavGraph& Graph, const int32 Node)
	{
		return Graph.GetNode(Graph.GetSlot(Node), Node).LocalCellIndex;
	}

	bool IsSettledCell(const FLNPNavFlowFieldBlock& Block, const uint8 Cell)
	{
		return (Block.Settled[Cell >> 6] & (uint64(1) << (Cell & 63))) != 0;
	}

	void Complete(FLNPNavFlowField& Field)
	{
		Field.Status = ELNPNavFlowFieldStatus::Complete;
		Field.Open.Empty();
	}
}

ELNPNavFlowFieldStatus LNPNavFlowField::BeginBuild(const FLNPNavSnapshot& Nav, const int32 Goal,
	const FLNPNavFlowFieldParams& Params, FLNPNavFlowField& OutField, const FLNPNavOverlay* Overlay)
{
	OutField = FLNPNavFlowField();
	OutField.Goal = Goal;
	OutField.Params = Params;
	OutField.SnapshotGeneration = Nav.SnapshotGeneration;
	OutField.ConnectivityGraphVersion = Nav.ConnectivityGraphVersion;
	OutField.OverlayRevision = LNPNavOverlay::GetRevision(Overlay);
	const FLNPNavGraph& Graph = Nav.Graph;
	if (Graph.GetSlot(Goal) == INDEX_NONE || Graph.BlockedNodes[Goal] || LNPNavOverlay::IsBlocked(Overlay, Goal))
	{
		return OutField.Status;
	}
	const int32 BlockIndex = FindBlock(Nav, OutField, Goal, true);
	OutField.Blocks[BlockIndex].Distance[GetCell(Graph, Goal)] = 0.0f;
	OutField.Open.HeapPush({0.0f, 0.0f, Goal}, FFlowFieldOpenOrder());
	OutField.Status = ELNPNavFlowFieldStatus::Building;
	return OutField.Status;
}

ELNPNavFlowFieldStatus LNPNavFlowField::StepBuild(const FLNPNavSnapshot& Nav, FLNPNavFlowField& Field, const int32 Budget,
	const FLNPNavOverlay* Overlay)
{
	if (Field.Status != ELNPNavFlowFieldStatus::Building)
	{
		return Field.Status;
	}
	if (Nav.SnapshotGeneration != Field.SnapshotGeneration || Nav.ConnectivityGraphVersion != Field.ConnectivityGraphVersion
		|| LNPNavOverlay::GetRevision(Overlay) != Field.OverlayRevision)
	{
		Field.Status = ELNPNavFlowFieldStatus::Invalid;
		Field.Open.Empty();
		return Field.Status;
	}

	const FLNPNavGraph& Graph = Nav.Graph;
	const double WalkableMinDot = Graph.SlotGraphs[0]->Agent.WalkableMinDot;
	const float MaxCost = static_cast<float>(Field.Params.MaxCost);
	// 남은 open이 모두 반경 밖이면 끝났다. 예산을 다 쓴 직후에도 검사해 다음 호출을 기다리지 않는다.
	auto IsExhausted = [&Field, MaxCost]()
	{
		return Field.Open.IsEmpty() || Field.Settled >= Field.Params.MaxSettled || Field.Open.HeapTop().G > MaxCost;
	};
	int32 Remaining = Budget;
	while (Remaining > 0 && !IsExhausted())
	{
		FLNPNavOpenEntry Entry;
		Field.Open.HeapPop(Entry, FFlowFieldOpenOrder(), EAllowShrinking::No);
		const int32 Slot = Graph.GetSlot(Entry.Node);
		const FLNPNavGraphNode& GraphNode = Graph.GetNode(Slot, Entry.Node);
		const int32 BlockIndex = FindBlock(Nav, Field, Entry.Node, false);
		{
			FLNPNavFlowFieldBlock& Block = Field.Blocks[BlockIndex];
			const uint8 Cell = GraphNode.LocalCellIndex;
			// 더 짧은 거리로 다시 넣은 node의 옛 항목이다.
			if (IsSettledCell(Block, Cell) || Entry.G > Block.Distance[Cell])
			{
				continue;
			}
			Block.Settled[Cell >> 6] |= uint64(1) << (Cell & 63);
		}
		++Field.Settled;
		--Remaining;

		const FVector3d Point = Graph.GetWorldPoint(Slot, Entry.Node);
		const int32 SlotBegin = Graph.SlotNodeBase[Slot];
		const int32 SlotEnd = Graph.SlotNodeBase[Slot + 1];
		// 정방향 이웃을 역방향 edge로 쓴다. edge가 양방향으로 있다는 전제는 Nav.FlowField 자동화가 검증한다.
		Graph.ForEachNeighbor(Slot, Entry.Node, [&](const int32 From)
		{
			const int32 FromSlot = From >= SlotBegin && From < SlotEnd ? Slot : Graph.GetSlot(From);
			const FLNPNavGraphNode& FromNode = Graph.GetNode(FromSlot, From);
			const int32 FromBlockIndex = FindBlock(Nav, Field, From, true);
			FLNPNavFlowFieldBlock& FromBlock = Field.Blocks[FromBlockIndex];
			const uint8 FromCell = FromNode.LocalCellIndex;
			if (IsSettledCell(FromBlock, FromCell))
			{
				return;
			}
			// 개체는 From에서 확정 node 쪽으로 걷는다. 비용도 그 방향으로 잰다.
			const float NewG = Entry.G + static_cast<float>(
				LNPNavGraph::ComputeEdgeCost(Graph.GetWorldPoint(FromSlot, From), Point, WalkableMinDot));
			if (NewG >= FromBlock.Distance[FromCell])
			{
				return;
			}
			FromBlock.Distance[FromCell] = NewG;
			FromBlock.Next[FromCell] = Entry.Node;
			Field.Open.HeapPush({NewG, NewG, From}, FFlowFieldOpenOrder());
		}, Overlay);
	}
	if (IsExhausted())
	{
		Complete(Field);
	}
	return Field.Status;
}

bool LNPNavFlowField::GetSettledDistance(const FLNPNavSnapshot& Nav, const FLNPNavFlowField& Field, const int32 Node, float& OutDistance)
{
	if (Field.Status == ELNPNavFlowFieldStatus::Invalid || Nav.SnapshotGeneration != Field.SnapshotGeneration
		|| Nav.Graph.GetSlot(Node) == INDEX_NONE)
	{
		return false;
	}
	const int32* BlockIndex = Field.BlockByTile.Find(LNPNavGraph::GetTileKey(Nav, Node));
	if (BlockIndex == nullptr)
	{
		return false;
	}
	const FLNPNavFlowFieldBlock& Block = Field.Blocks[*BlockIndex];
	const uint8 Cell = GetCell(Nav.Graph, Node);
	if (!IsSettledCell(Block, Cell))
	{
		return false;
	}
	OutDistance = Block.Distance[Cell];
	return true;
}

bool LNPNavFlowField::ExtractNodePath(const FLNPNavSnapshot& Nav, const FLNPNavFlowField& Field, const int32 Start, TArray<int32>& OutNodes)
{
	OutNodes.Reset();
	float Distance = 0.0f;
	if (!GetSettledDistance(Nav, Field, Start, Distance))
	{
		return false;
	}
	// 확정 node의 다음 node는 그보다 먼저 확정됐으므로 goal까지 확정 node만 지난다.
	for (int32 Node = Start; Node != INDEX_NONE;)
	{
		OutNodes.Add(Node);
		if (OutNodes.Num() > Field.Settled)
		{
			OutNodes.Reset();
			return false;
		}
		const FLNPNavFlowFieldBlock& Block = Field.Blocks[Field.BlockByTile.FindChecked(LNPNavGraph::GetTileKey(Nav, Node))];
		Node = Block.Next[GetCell(Nav.Graph, Node)];
	}
	return OutNodes.Last() == Field.Goal;
}
