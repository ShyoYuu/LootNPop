// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavRuntime.h"

#include "SurfaceNavigation/LNPCrustAtlas.h"

#include "Algo/BinarySearch.h"

namespace
{
	constexpr int32 NavSlotCount = 8;
	/** Support rasterizer가 이웃 샘플 사이에 허용하는 법선 각도(25°)와 같은 seam 양쪽 법선 허용치. */
	const double MinSeamNormalDot = FMath::Cos(FMath::DegreesToRadians(25.0));

	int32 FindRoot(TArray<uint32>& Parents, int32 Index)
	{
		while (Parents[Index] != static_cast<uint32>(Index))
		{
			Parents[Index] = Parents[Parents[Index]];
			Index = static_cast<int32>(Parents[Index]);
		}
		return Index;
	}

	void Union(TArray<uint32>& Parents, const int32 A, const int32 B)
	{
		const int32 RootA = FindRoot(Parents, A);
		const int32 RootB = FindRoot(Parents, B);
		if (RootA != RootB)
		{
			// 작은 index를 root로 둬 결과가 union 순서와 무관하게 결정론적이다.
			Parents[FMath::Max(RootA, RootB)] = static_cast<uint32>(FMath::Min(RootA, RootB));
		}
	}

	bool SameAgent(const FLNPNavAgentProfile& A, const FLNPNavAgentProfile& B)
	{
		return A.Radius == B.Radius && A.HalfHeight == B.HalfHeight && A.MaxStepUp == B.MaxStepUp
			&& A.MaxStepDown == B.MaxStepDown && A.WalkableMinDot == B.WalkableMinDot;
	}

	/** 인접한 두 격자 좌표 사이의 6방향 index. 인접하지 않으면 INDEX_NONE이다. */
	int32 FindNeighborDirection(const int32 Subdivisions, const FIntPoint From, const FIntPoint To)
	{
		for (uint8 Direction = 0; Direction < 6; ++Direction)
		{
			FIntPoint Coord;
			if (LNPNavData::TryGetNeighborCoord(
				Subdivisions, From.X, From.Y, static_cast<ELNPNavNeighbor>(Direction), Coord) && Coord == To)
			{
				return Direction;
			}
		}
		return INDEX_NONE;
	}

	/** [edge * (N + 1) + step] → SeamEndpoints index. 없으면 INDEX_NONE. */
	TArray<int32> BuildSeamLookup(const FLNPNavTraversalData& Traversal, const int32 Subdivisions)
	{
		TArray<int32> Lookup;
		Lookup.Init(INDEX_NONE, 3 * (Subdivisions + 1));
		for (int32 Index = 0; Index < Traversal.SeamEndpoints.Num(); ++Index)
		{
			const FLNPNavSeamEndpoint& Endpoint = Traversal.SeamEndpoints[Index];
			Lookup[static_cast<int32>(Endpoint.Edge) * (Subdivisions + 1) + Endpoint.SeamStep] = Index;
		}
		return Lookup;
	}
}

bool LNPNavRuntime::BuildSnapshot(
	const TConstArrayView<FLNPNavSlotInput> Slots,
	const TConstArrayView<FRotator> SlotRotations,
	const uint64 SnapshotGeneration,
	FLNPNavSnapshot& OutSnapshot,
	FString& OutError)
{
	OutSnapshot = FLNPNavSnapshot();
	OutError.Reset();
	auto Fail = [&OutSnapshot, &OutError](FString Error)
	{
		OutSnapshot = FLNPNavSnapshot();
		OutError = MoveTemp(Error);
		return false;
	};

	if (Slots.Num() != NavSlotCount || SlotRotations.Num() != NavSlotCount || SnapshotGeneration == 0)
	{
		return Fail(TEXT("Nav snapshot requires eight slots, eight rotations, and a non-zero generation."));
	}

	FLNPNavSnapshot Snapshot;
	Snapshot.SnapshotGeneration = SnapshotGeneration;
	Snapshot.SlotLayerBase.Reserve(NavSlotCount + 1);
	Snapshot.SlotComponentBase.Reserve(NavSlotCount + 1);
	uint32 LayerBase = 0;
	uint32 ComponentBase = 0;
	for (int32 Slot = 0; Slot < NavSlotCount; ++Slot)
	{
		const FLNPNavSlotInput& Input = Slots[Slot];
		if (!Input.Support.IsValid() || !Input.Navigation.IsValid() || !Input.Traversal.IsValid())
		{
			return Fail(FString::Printf(TEXT("Nav slot %d has no decoded Support, Navigation, or Traversal."), Slot));
		}
		const FLNPNavData& Navigation = *Input.Navigation;
		if (Navigation.Layers.Num() != Input.Support->Layers.Num() || Navigation.Layers.IsEmpty()
			|| Navigation.Layers[0].LocalNavLayerId != 0)
		{
			return Fail(FString::Printf(TEXT("Nav slot %d has %d Nav Layers for %d Support Layers."),
				Slot, Navigation.Layers.Num(), Input.Support->Layers.Num()));
		}
		const FLNPNavData& First = *Slots[0].Navigation;
		if (!SameAgent(Navigation.Agent, First.Agent)
			|| Navigation.Layers[0].Subdivisions != First.Layers[0].Subdivisions)
		{
			return Fail(FString::Printf(TEXT("Nav slot %d uses a different agent profile or crust Nav resolution."), Slot));
		}
		Snapshot.SlotLayerBase.Add(static_cast<uint16>(LayerBase));
		Snapshot.SlotComponentBase.Add(ComponentBase);
		LayerBase += Navigation.Layers.Num();
		ComponentBase += Navigation.LocalStaticComponentCount;
		if (LayerBase >= MAX_uint16)
		{
			return Fail(TEXT("Nav snapshot exceeds the uint16 runtime Nav Layer range."));
		}
	}
	Snapshot.SlotLayerBase.Add(static_cast<uint16>(LayerBase));
	Snapshot.SlotComponentBase.Add(ComponentBase);

	TArray<uint32> Parents;
	Parents.SetNumUninitialized(ComponentBase);
	for (uint32 Index = 0; Index < ComponentBase; ++Index)
	{
		Parents[Index] = Index;
	}

	for (int32 Slot = 0; Slot < NavSlotCount; ++Slot)
	{
		const FLNPNavData& Navigation = *Slots[Slot].Navigation;
		for (const FLNPNavPortal& Portal : Slots[Slot].Traversal->Portals)
		{
			const FLNPNavCell* A = LNPNavData::ResolveLocalNode(Navigation, Portal.A);
			const FLNPNavCell* B = LNPNavData::ResolveLocalNode(Navigation, Portal.B);
			if (A == nullptr || B == nullptr)
			{
				return Fail(FString::Printf(TEXT("Nav slot %d portal endpoint does not resolve."), Slot));
			}
			Union(Parents,
				Snapshot.SlotComponentBase[Slot] + A->LocalStaticComponentId,
				Snapshot.SlotComponentBase[Slot] + B->LocalStaticComponentId);
		}
	}

	TArray<FLNPCrustSeamPair> Pairs;
	if (!LNPCrustAtlas::ComputeSeamPairs(SlotRotations, Pairs, OutError))
	{
		return Fail(MoveTemp(OutError));
	}

	const int32 N = Slots[0].Navigation->Layers[0].Subdivisions;
	TMap<const FLNPNavTraversalData*, TArray<int32>> SeamLookups;
	for (const FLNPNavSlotInput& Input : Slots)
	{
		if (!SeamLookups.Contains(Input.Traversal.Get()))
		{
			SeamLookups.Add(Input.Traversal.Get(), BuildSeamLookup(*Input.Traversal, N));
		}
	}

	// 각 slot 베이크는 이웃 slot의 지오메트리를 보지 못하므로 seam 줄 node·seam 방향 edge의 clearance는
	// 사본마다 다를 수 있다. 실제 월드의 충돌은 양쪽 지오메트리의 합집합이므로 양쪽 사본이 모두 통과한 것만 유효하다(D-060).
	for (const FLNPCrustSeamPair& Pair : Pairs)
	{
		const FLNPNavSlotInput& InputA = Slots[Pair.A.Slot];
		const FLNPNavSlotInput& InputB = Slots[Pair.B.Slot];
		const TArray<int32>& LookupA = SeamLookups.FindChecked(InputA.Traversal.Get());
		const TArray<int32>& LookupB = SeamLookups.FindChecked(InputB.Traversal.Get());
		const FQuat4d RotationA(SlotRotations[Pair.A.Slot].Quaternion());
		const FQuat4d RotationB(SlotRotations[Pair.B.Slot].Quaternion());
		const int32 EdgeA = static_cast<int32>(Pair.A.Edge);
		const int32 EdgeB = static_cast<int32>(Pair.B.Edge);
		auto PairName = [&Pair]()
		{
			return FString::Printf(TEXT("%d:%d-%d:%d"),
				Pair.A.Slot, static_cast<int32>(Pair.A.Edge), Pair.B.Slot, static_cast<int32>(Pair.B.Edge));
		};
		auto RuntimeRef = [&Snapshot](const int32 Slot, const FLNPNavSlotInput& Input, const int32 EndpointIndex)
		{
			FLNPNavNodeRef Node;
			MakeRuntimeNodeRef(Snapshot, Slot, Input.Traversal->SeamEndpoints[EndpointIndex].Node, Node);
			return Node;
		};

		for (int32 Step = 0; Step <= N; ++Step)
		{
			const int32 StepB = Pair.bReversed ? N - Step : Step;
			const int32 IndexA = LookupA[EdgeA * (N + 1) + Step];
			const int32 IndexB = LookupB[EdgeB * (N + 1) + StepB];
			if (IndexA == INDEX_NONE && IndexB == INDEX_NONE)
			{
				continue;
			}
			if (IndexA == INDEX_NONE || IndexB == INDEX_NONE)
			{
				Snapshot.BlockedSeamNodes.Add(IndexA != INDEX_NONE
					? RuntimeRef(Pair.A.Slot, InputA, IndexA)
					: RuntimeRef(Pair.B.Slot, InputB, IndexB));
				continue;
			}

			const FLNPNavSeamEndpoint& EndpointA = InputA.Traversal->SeamEndpoints[IndexA];
			const FLNPNavSeamEndpoint& EndpointB = InputB.Traversal->SeamEndpoints[IndexB];
			FIntPoint CoordA;
			FIntPoint CoordB;
			const FLNPNavCell* CellA = LNPNavData::ResolveLocalNode(*InputA.Navigation, EndpointA.Node, &CoordA);
			const FLNPNavCell* CellB = LNPNavData::ResolveLocalNode(*InputB.Navigation, EndpointB.Node, &CoordB);
			if (CellA == nullptr || CellB == nullptr || CellA->ClearanceClass != CellB->ClearanceClass)
			{
				return Fail(FString::Printf(TEXT("Nav seam pair %s has unresolved or clearance-mismatched nodes at step %d/%d."),
					*PairName(), Step, N));
			}

			const FVector3d LocalDirectionA = LNPSupportAtlas::GetSampleDirection(N, CoordA.X, CoordA.Y);
			const FVector3d LocalDirectionB = LNPSupportAtlas::GetSampleDirection(N, CoordB.X, CoordB.Y);
			FLNPSupportLayerQuery QueryA;
			FLNPSupportLayerQuery QueryB;
			if (!LNPSupportAtlas::QueryLayer(InputA.Support->Layers[0], LocalDirectionA, QueryA)
				|| !LNPSupportAtlas::QueryLayer(InputB.Support->Layers[0], LocalDirectionB, QueryB))
			{
				return Fail(FString::Printf(TEXT("Nav seam pair %s has no crust Support at step %d/%d."),
					*PairName(), Step, N));
			}
			const double DirectionDot = FVector3d::DotProduct(
				RotationA.RotateVector(LocalDirectionA), RotationB.RotateVector(LocalDirectionB));
			const double RadiusDelta = FMath::Abs(QueryA.Radius - QueryB.Radius);
			const double NormalDot = FVector3d::DotProduct(
				RotationA.RotateVector(FVector3d(QueryA.Normal)).GetSafeNormal(),
				RotationB.RotateVector(FVector3d(QueryB.Normal)).GetSafeNormal());
			Snapshot.MaxSeamRadiusDelta = FMath::Max(Snapshot.MaxSeamRadiusDelta, RadiusDelta);
			Snapshot.MinSeamNormalDot = FMath::Min(Snapshot.MinSeamNormalDot, NormalDot);
			if (DirectionDot < 1.0 - 1.e-9 || RadiusDelta > MaxSeamRadiusDelta || NormalDot < MinSeamNormalDot)
			{
				return Fail(FString::Printf(
					TEXT("Nav seam pair %s differs at step %d/%d (directionDot=%.12f radiusDelta=%.3f normalDot=%.4f)."),
					*PairName(), Step, N, DirectionDot, RadiusDelta, NormalDot));
			}

			// seam 방향 edge는 양쪽 사본이 같은 월드 선분이다. 한쪽만 열려 있으면 열린 사본을 막힘으로 기록한다.
			// 다음 step이 한쪽에만 있으면 그 node가 막히므로 edge도 함께 무효다.
			const int32 NextStepB = Pair.bReversed ? StepB - 1 : StepB + 1;
			const int32 NextIndexA = Step < N ? LookupA[EdgeA * (N + 1) + Step + 1] : INDEX_NONE;
			const int32 NextIndexB = Step < N ? LookupB[EdgeB * (N + 1) + NextStepB] : INDEX_NONE;
			if (NextIndexA != INDEX_NONE && NextIndexB != INDEX_NONE)
			{
				const int32 DirectionA = FindNeighborDirection(N, CoordA,
					LNPCrustAtlas::GetSeamSampleCoord(N, Pair.A.Edge, Step + 1));
				const int32 DirectionB = FindNeighborDirection(N, CoordB,
					LNPCrustAtlas::GetSeamSampleCoord(N, Pair.B.Edge, NextStepB));
				if (DirectionA == INDEX_NONE || DirectionB == INDEX_NONE)
				{
					return Fail(FString::Printf(TEXT("Nav seam pair %s has non-adjacent seam samples at step %d."),
						*PairName(), Step));
				}
				const bool bEdgeA = (CellA->EdgeMask & (1u << DirectionA)) != 0;
				const bool bEdgeB = (CellB->EdgeMask & (1u << DirectionB)) != 0;
				if (bEdgeA != bEdgeB)
				{
					Snapshot.BlockedSeamEdges.Add(bEdgeA
						? FLNPNavEdgeRef{RuntimeRef(Pair.A.Slot, InputA, IndexA), RuntimeRef(Pair.A.Slot, InputA, NextIndexA)}
						: FLNPNavEdgeRef{RuntimeRef(Pair.B.Slot, InputB, IndexB), RuntimeRef(Pair.B.Slot, InputB, NextIndexB)});
				}
			}

			FLNPNavSeamLink& Link = Snapshot.SeamLinks.AddDefaulted_GetRef();
			if (!MakeRuntimeNodeRef(Snapshot, Pair.A.Slot, EndpointA.Node, Link.A)
				|| !MakeRuntimeNodeRef(Snapshot, Pair.B.Slot, EndpointB.Node, Link.B))
			{
				return Fail(FString::Printf(TEXT("Nav seam pair %s endpoint has no runtime node ref."), *PairName()));
			}
			Union(Parents,
				Snapshot.SlotComponentBase[Pair.A.Slot] + CellA->LocalStaticComponentId,
				Snapshot.SlotComponentBase[Pair.B.Slot] + CellB->LocalStaticComponentId);
		}
	}

	Snapshot.RuntimeStaticComponentByLocal.SetNumUninitialized(ComponentBase);
	TArray<uint32> RuntimeByRoot;
	RuntimeByRoot.Init(MAX_uint32, ComponentBase);
	for (uint32 Index = 0; Index < ComponentBase; ++Index)
	{
		const int32 Root = FindRoot(Parents, static_cast<int32>(Index));
		if (RuntimeByRoot[Root] == MAX_uint32)
		{
			RuntimeByRoot[Root] = Snapshot.RuntimeStaticComponentCount++;
		}
		Snapshot.RuntimeStaticComponentByLocal[Index] = RuntimeByRoot[Root];
	}

	Snapshot.BlockedSeamNodeKeys.Reserve(Snapshot.BlockedSeamNodes.Num());
	for (const FLNPNavNodeRef& Node : Snapshot.BlockedSeamNodes)
	{
		Snapshot.BlockedSeamNodeKeys.Add(MakeNodeKey(Node));
	}
	Snapshot.BlockedSeamNodeKeys.Sort();

	// 7a에는 active 동적 Traversal Link가 없으므로 ReachabilityGroup은 StaticNavComponent와 1:1이다.
	// Phase 8이 active link union과 version 증가를 이 자리에 넣는다.
	Snapshot.ReachabilityGroupByStaticComponent.SetNumUninitialized(Snapshot.RuntimeStaticComponentCount);
	for (uint32 Component = 0; Component < Snapshot.RuntimeStaticComponentCount; ++Component)
	{
		Snapshot.ReachabilityGroupByStaticComponent[Component] = Component;
	}
	Snapshot.ReachabilityGroupCount = Snapshot.RuntimeStaticComponentCount;
	Snapshot.ConnectivityGraphVersion = 1;

	// 조밀 graph는 decode 공유와 같은 단위(asset)로 한 번만 만든다.
	TMap<const FLNPNavData*, TSharedPtr<const FLNPNavAssetGraph, ESPMode::ThreadSafe>> AssetGraphs;
	TArray<TSharedPtr<const FLNPNavAssetGraph, ESPMode::ThreadSafe>> SlotGraphs;
	for (int32 Slot = 0; Slot < NavSlotCount; ++Slot)
	{
		const FLNPNavSlotInput& Input = Slots[Slot];
		TSharedPtr<const FLNPNavAssetGraph, ESPMode::ThreadSafe>& AssetGraph = AssetGraphs.FindOrAdd(Input.Navigation.Get());
		if (!AssetGraph.IsValid())
		{
			TSharedRef<FLNPNavAssetGraph, ESPMode::ThreadSafe> Built = MakeShared<FLNPNavAssetGraph, ESPMode::ThreadSafe>();
			if (!LNPNavGraph::BuildAssetGraph(*Input.Support, *Input.Navigation, *Input.Traversal, *Built, OutError))
			{
				return Fail(FString::Printf(TEXT("Nav slot %d graph: %s"), Slot, *OutError));
			}
			AssetGraph = Built;
		}
		SlotGraphs.Add(AssetGraph);
	}
	FLNPNavGraph Graph;
	if (!LNPNavGraph::BuildRuntimeGraph(SlotGraphs, SlotRotations, Snapshot, Graph, OutError))
	{
		return Fail(MoveTemp(OutError));
	}
	Snapshot.Graph = MoveTemp(Graph);

	OutSnapshot = MoveTemp(Snapshot);
	return true;
}

bool LNPNavRuntime::MakeRuntimeNodeRef(
	const FLNPNavSnapshot& Snapshot, const int32 Slot, const FLNPLocalNavNodeRef& Local, FLNPNavNodeRef& OutNode)
{
	OutNode = FLNPNavNodeRef();
	if (Snapshot.SnapshotGeneration == 0 || Slot < 0 || Slot >= Snapshot.SlotLayerBase.Num() - 1 || !Local.IsValid()
		|| Local.LocalNavLayerId >= Snapshot.SlotLayerBase[Slot + 1] - Snapshot.SlotLayerBase[Slot])
	{
		return false;
	}
	OutNode.RuntimeNavLayerId = static_cast<uint16>(Snapshot.SlotLayerBase[Slot] + Local.LocalNavLayerId);
	OutNode.TileId = Local.TileId;
	OutNode.LocalCellIndex = Local.LocalCellIndex;
	OutNode.SnapshotGeneration = Snapshot.SnapshotGeneration;
	return true;
}

bool LNPNavRuntime::ResolveRuntimeNodeRef(
	const FLNPNavSnapshot& Snapshot, const FLNPNavNodeRef& Node, int32& OutSlot, FLNPLocalNavNodeRef& OutLocal)
{
	OutSlot = INDEX_NONE;
	OutLocal = FLNPLocalNavNodeRef();
	if (!Snapshot.IsValid() || !LNPNavData::IsCurrentNodeRef(Node, Snapshot.SnapshotGeneration))
	{
		return false;
	}
	for (int32 Slot = 0; Slot < NavSlotCount; ++Slot)
	{
		if (Node.RuntimeNavLayerId < Snapshot.SlotLayerBase[Slot + 1])
		{
			OutSlot = Slot;
			OutLocal.LocalNavLayerId = static_cast<uint16>(Node.RuntimeNavLayerId - Snapshot.SlotLayerBase[Slot]);
			OutLocal.TileId = Node.TileId;
			OutLocal.LocalCellIndex = Node.LocalCellIndex;
			return true;
		}
	}
	return false;
}

uint32 LNPNavRuntime::GetRuntimeStaticComponent(
	const FLNPNavSnapshot& Snapshot, const int32 Slot, const uint16 LocalStaticComponentId)
{
	if (!Snapshot.IsValid() || Slot < 0 || Slot >= NavSlotCount
		|| LocalStaticComponentId >= Snapshot.SlotComponentBase[Slot + 1] - Snapshot.SlotComponentBase[Slot])
	{
		return MAX_uint32;
	}
	return Snapshot.RuntimeStaticComponentByLocal[Snapshot.SlotComponentBase[Slot] + LocalStaticComponentId];
}

bool LNPNavRuntime::IsBlockedSeamNode(const FLNPNavSnapshot& Snapshot, const FLNPNavNodeRef& Node)
{
	return LNPNavData::IsCurrentNodeRef(Node, Snapshot.SnapshotGeneration)
		&& Algo::BinarySearch(Snapshot.BlockedSeamNodeKeys, MakeNodeKey(Node)) != INDEX_NONE;
}

uint64 LNPNavRuntime::GetAllocatedBytes(const FLNPNavData& Navigation)
{
	uint64 Bytes = sizeof(FLNPNavData) + Navigation.Layers.GetAllocatedSize();
	for (const FLNPNavLayer& Layer : Navigation.Layers)
	{
		Bytes += Layer.Tiles.GetAllocatedSize();
		for (const FLNPNavTile& Tile : Layer.Tiles)
		{
			Bytes += Tile.Cells.GetAllocatedSize();
		}
	}
	return Bytes;
}

uint64 LNPNavRuntime::GetAllocatedBytes(const FLNPNavTraversalData& Traversal)
{
	return sizeof(FLNPNavTraversalData)
		+ Traversal.StaticComponents.GetAllocatedSize()
		+ Traversal.Portals.GetAllocatedSize()
		+ Traversal.SeamEndpoints.GetAllocatedSize();
}

uint64 LNPNavRuntime::GetAllocatedBytes(const FLNPNavSnapshot& Snapshot)
{
	return Snapshot.SlotLayerBase.GetAllocatedSize()
		+ Snapshot.SlotComponentBase.GetAllocatedSize()
		+ Snapshot.RuntimeStaticComponentByLocal.GetAllocatedSize()
		+ Snapshot.SeamLinks.GetAllocatedSize()
		+ Snapshot.BlockedSeamNodes.GetAllocatedSize()
		+ Snapshot.BlockedSeamEdges.GetAllocatedSize()
		+ Snapshot.BlockedSeamNodeKeys.GetAllocatedSize()
		+ Snapshot.ReachabilityGroupByStaticComponent.GetAllocatedSize()
		+ Snapshot.Graph.GetAllocatedBytes();
}
