// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavBaking.h"

namespace
{
	struct FBakeNode
	{
		int32 I = 0;
		int32 J = 0;
		FVector3d Position = FVector3d::ZeroVector;
		FVector3f Normal = FVector3f::ZeroVector;
		uint8 EdgeMask = 0;
		uint8 ClearanceClass = 0;
		uint16 Component = MAX_uint16;
		bool bBoundary = false;
	};

	struct FBakeLayer
	{
		uint16 LayerId = MAX_uint16;
		int32 Subdivisions = 0;
		TArray<FBakeNode> Nodes;
		TMap<uint64, int32> NodeByCoord;
	};

	struct FPortalCandidate
	{
		FLNPLocalNavNodeRef A;
		FLNPLocalNavNodeRef B;
		double DistanceSquared = TNumericLimits<double>::Max();
		uint8 MinClearanceClass = 0;
	};

	struct FSpatialNode
	{
		int32 LayerIndex = INDEX_NONE;
		int32 NodeIndex = INDEX_NONE;
	};

	uint64 BakeCoordKey(const int32 I, const int32 J)
	{
		return (static_cast<uint64>(static_cast<uint32>(I)) << 32) | static_cast<uint32>(J);
	}

	bool BakeNodeLess(const FLNPLocalNavNodeRef& A, const FLNPLocalNavNodeRef& B)
	{
		return A.LocalNavLayerId != B.LocalNavLayerId ? A.LocalNavLayerId < B.LocalNavLayerId
			: A.TileId != B.TileId ? A.TileId < B.TileId
			: A.LocalCellIndex < B.LocalCellIndex;
	}

	bool BakePortalLess(const FLNPNavPortal& A, const FLNPNavPortal& B)
	{
		return BakeNodeLess(A.A, B.A) || (!BakeNodeLess(B.A, A.A)
			&& (BakeNodeLess(A.B, B.B) || (!BakeNodeLess(B.B, A.B)
				&& A.MinClearanceClass < B.MinClearanceClass)));
	}

	/**
	 * 두 node의 높이 차가 불연속 step 한도나 walkable 경사로 설명되는지 보는 사전 필터다. 사이 지형의 step과 경사는
	 * edge clearance callback이 exact Support 경로로 다시 검사한다. 끝점 높이 차만 step 한도와 비교하면 200cm 격자에서
	 * 약 12.7°를 넘는 경사가 모두 끊긴다.
	 */
	bool CanStep(const FBakeNode& A, const FBakeNode& B, const FLNPNavAgentProfile& Agent)
	{
		const double TanMaxSlope = FMath::Tan(FMath::Acos(FMath::Clamp(static_cast<double>(Agent.WalkableMinDot), 0.0, 1.0)));
		auto CanRise = [&Agent, TanMaxSlope](const FVector3d& Delta, const FVector3d& Up)
		{
			const double Rise = FVector3d::DotProduct(Delta, Up);
			const double SlopeRise = FMath::Sqrt(FMath::Max(0.0, Delta.SizeSquared() - Rise * Rise)) * TanMaxSlope;
			return Rise <= FMath::Max(static_cast<double>(Agent.MaxStepUp), SlopeRise)
				&& Rise >= -FMath::Max(static_cast<double>(Agent.MaxStepDown), SlopeRise);
		};
		const FVector3d Delta = B.Position - A.Position;
		return CanRise(Delta, -A.Position.GetSafeNormal()) && CanRise(-Delta, -B.Position.GetSafeNormal());
	}

	void BuildTiles(const FBakeLayer& Baked, FLNPNavLayer& OutLayer)
	{
		TMap<uint32, int32> TileByCoord;
		for (const FBakeNode& Node : Baked.Nodes)
		{
			uint16 TileX = 0;
			uint16 TileY = 0;
			uint8 LocalCellIndex = 0;
			check(LNPNavData::MakeTileAddress(Node.I, Node.J, TileX, TileY, LocalCellIndex));
			const uint32 TileKey = (static_cast<uint32>(TileY) << 16) | TileX;
			int32* Existing = TileByCoord.Find(TileKey);
			if (Existing == nullptr)
			{
				const int32 NewIndex = OutLayer.Tiles.AddDefaulted();
				OutLayer.Tiles[NewIndex].TileX = TileX;
				OutLayer.Tiles[NewIndex].TileY = TileY;
				TileByCoord.Add(TileKey, NewIndex);
				Existing = TileByCoord.Find(TileKey);
			}
			FLNPNavCell& Cell = OutLayer.Tiles[*Existing].Cells.AddDefaulted_GetRef();
			Cell.LocalCellIndex = LocalCellIndex;
			Cell.EdgeMask = Node.EdgeMask;
			Cell.ClearanceClass = Node.ClearanceClass;
			Cell.Flags = ELNPNavCellFlags::Walkable
				| (Node.bBoundary ? ELNPNavCellFlags::NearStaticBlocker : ELNPNavCellFlags::None);
			Cell.LocalStaticComponentId = Node.Component;
		}

		OutLayer.Tiles.Sort([](const FLNPNavTile& A, const FLNPNavTile& B)
		{
			return A.TileY != B.TileY ? A.TileY < B.TileY : A.TileX < B.TileX;
		});
		for (int32 TileIndex = 0; TileIndex < OutLayer.Tiles.Num(); ++TileIndex)
		{
			FLNPNavTile& Tile = OutLayer.Tiles[TileIndex];
			Tile.TileId = static_cast<uint16>(TileIndex);
			Tile.Cells.Sort([](const FLNPNavCell& A, const FLNPNavCell& B)
			{
				return A.LocalCellIndex < B.LocalCellIndex;
			});
		}
	}

	FIntVector SpatialCell(const FVector3d& Position, const double CellSize)
	{
		return FIntVector(
			FMath::FloorToInt32(Position.X / CellSize),
			FMath::FloorToInt32(Position.Y / CellSize),
			FMath::FloorToInt32(Position.Z / CellSize));
	}
}

bool LNPNavBaking::Build(
	const FLNPSupportAtlas& Support,
	const FLNPNavBakeSettings& Settings,
	FLNPNavNodeClearance HasNodeClearance,
	FLNPNavEdgeClearance HasEdgeClearance,
	FLNPNavData& OutNavigation,
	FLNPNavTraversalData& OutTraversal,
	FLNPNavBakeReport& OutReport,
	FString& OutError)
{
	OutNavigation = FLNPNavData();
	OutTraversal = FLNPNavTraversalData();
	OutReport = FLNPNavBakeReport();
	OutError.Reset();
	auto Fail = [&](FString Error)
	{
		OutNavigation = FLNPNavData();
		OutTraversal = FLNPNavTraversalData();
		OutReport = FLNPNavBakeReport();
		OutError = MoveTemp(Error);
		return false;
	};

	if (Support.Layers.IsEmpty() || Support.Layers.Num() >= MAX_uint16
		|| !(Settings.CrustSpacing > 0.0) || !(Settings.LayerSpacing > 0.0)
		|| !(Settings.PortalSearchDistance > 0.0) || !(Settings.ClearanceClassStep > 0.0)
		|| !(Settings.Agent.Radius > 0.0f) || Settings.Agent.HalfHeight < Settings.Agent.Radius
		|| Settings.Agent.MaxStepUp < 0.0f || Settings.Agent.MaxStepDown < 0.0f
		|| Settings.Agent.WalkableMinDot < 0.0f || Settings.Agent.WalkableMinDot > 1.0f)
	{
		return Fail(TEXT("Invalid Nav bake input or settings."));
	}

	OutNavigation.Agent = Settings.Agent;
	TArray<FBakeLayer> BakedLayers;
	BakedLayers.SetNum(Support.Layers.Num());
	OutNavigation.Layers.SetNum(Support.Layers.Num());
	uint32 NextComponent = 0;
	const uint8 ClearanceClass = static_cast<uint8>(FMath::Clamp(
		FMath::CeilToInt(Settings.Agent.Radius / Settings.ClearanceClassStep), 1, 255));

	for (int32 LayerIndex = 0; LayerIndex < Support.Layers.Num(); ++LayerIndex)
	{
		const FLNPSupportAtlasLayer& SupportLayer = Support.Layers[LayerIndex];
		FBakeLayer& Baked = BakedLayers[LayerIndex];
		Baked.LayerId = static_cast<uint16>(LayerIndex);
		const double Spacing = LayerIndex == 0 ? Settings.CrustSpacing : Settings.LayerSpacing;
		Baked.Subdivisions = LNPSupportAtlas::ComputeSubdivisionsForSpacing(SupportLayer.BaseRadius, Spacing);
		if (Baked.Subdivisions < 1 || (LayerIndex == 0 && Baked.Subdivisions >= MAX_uint16))
		{
			return Fail(FString::Printf(TEXT("Nav Layer %d has invalid subdivisions."), LayerIndex));
		}

		for (int32 J = 0; J <= Baked.Subdivisions; ++J)
		{
			for (int32 I = 0; I + J <= Baked.Subdivisions; ++I)
			{
				const FVector3d Direction = LNPSupportAtlas::GetSampleDirection(Baked.Subdivisions, I, J);
				FLNPSupportLayerQuery Query;
				if (!LNPSupportAtlas::QueryLayer(SupportLayer, Direction, Query)
					|| FVector3d::DotProduct(FVector3d(Query.Normal), -Direction) < Settings.Agent.WalkableMinDot)
				{
					continue;
				}
				const FVector3d Position = Direction * Query.Radius;
				if (!HasNodeClearance(Baked.LayerId, Position, Query.Normal))
				{
					const bool bCrustSeam = LayerIndex == 0 && (I == 0 || J == 0 || I + J == Baked.Subdivisions);
					OutReport.SeamClearanceRejectCount += bCrustSeam ? 1 : 0;
					continue;
				}
				const int32 NodeIndex = Baked.Nodes.AddDefaulted();
				FBakeNode& Node = Baked.Nodes[NodeIndex];
				Node.I = I;
				Node.J = J;
				Node.Position = Position;
				Node.Normal = Query.Normal;
				Node.ClearanceClass = ClearanceClass;
				Baked.NodeByCoord.Add(BakeCoordKey(I, J), NodeIndex);
			}
		}

		for (int32 NodeIndex = 0; NodeIndex < Baked.Nodes.Num(); ++NodeIndex)
		{
			FBakeNode& Node = Baked.Nodes[NodeIndex];
			for (uint8 Direction = 0; Direction < 6; ++Direction)
			{
				FIntPoint NeighborCoord;
				if (!LNPNavData::TryGetNeighborCoord(
					Baked.Subdivisions, Node.I, Node.J, static_cast<ELNPNavNeighbor>(Direction), NeighborCoord))
				{
					continue;
				}
				const int32* NeighborIndex = Baked.NodeByCoord.Find(BakeCoordKey(NeighborCoord.X, NeighborCoord.Y));
				if (NeighborIndex == nullptr)
				{
					Node.bBoundary = true;
					continue;
				}
				if (NodeIndex >= *NeighborIndex)
				{
					continue;
				}
				FBakeNode& Neighbor = Baked.Nodes[*NeighborIndex];
				if (!CanStep(Node, Neighbor, Settings.Agent)
					|| !HasEdgeClearance(Baked.LayerId, Node.Position, Node.Normal,
						Baked.LayerId, Neighbor.Position, Neighbor.Normal))
				{
					Node.bBoundary = true;
					Neighbor.bBoundary = true;
					continue;
				}
				const ELNPNavNeighbor NeighborDirection = static_cast<ELNPNavNeighbor>(Direction);
				Node.EdgeMask |= LNPNavData::GetNeighborBit(NeighborDirection);
				Neighbor.EdgeMask |= LNPNavData::GetNeighborBit(LNPNavData::GetOppositeNeighbor(NeighborDirection));
			}
		}

		TArray<int32> Queue;
		for (int32 Start = 0; Start < Baked.Nodes.Num(); ++Start)
		{
			if (Baked.Nodes[Start].Component != MAX_uint16)
			{
				continue;
			}
			if (NextComponent >= MAX_uint16)
			{
				return Fail(TEXT("Nav bake exceeded the uint16 StaticComponent range."));
			}
			const uint16 Component = static_cast<uint16>(NextComponent++);
			Queue.Reset();
			Queue.Add(Start);
			Baked.Nodes[Start].Component = Component;
			for (int32 ReadIndex = 0; ReadIndex < Queue.Num(); ++ReadIndex)
			{
				const int32 CurrentIndex = Queue[ReadIndex];
				const FBakeNode& Current = Baked.Nodes[CurrentIndex];
				for (uint8 Direction = 0; Direction < 6; ++Direction)
				{
					if ((Current.EdgeMask & (1u << Direction)) == 0)
					{
						continue;
					}
					FIntPoint NeighborCoord;
					LNPNavData::TryGetNeighborCoord(
						Baked.Subdivisions, Current.I, Current.J,
						static_cast<ELNPNavNeighbor>(Direction), NeighborCoord);
					const int32 NeighborIndex = Baked.NodeByCoord.FindChecked(BakeCoordKey(NeighborCoord.X, NeighborCoord.Y));
					if (Baked.Nodes[NeighborIndex].Component == MAX_uint16)
					{
						Baked.Nodes[NeighborIndex].Component = Component;
						Queue.Add(NeighborIndex);
					}
				}
			}
		}

		FLNPNavLayer& NavLayer = OutNavigation.Layers[LayerIndex];
		NavLayer.LocalNavLayerId = Baked.LayerId;
		NavLayer.LocalSupportLayerId = Baked.LayerId;
		NavLayer.Subdivisions = Baked.Subdivisions;
		BuildTiles(Baked, NavLayer);
	}
	if (NextComponent >= MAX_uint16)
	{
		return Fail(TEXT("Nav bake exceeded the representable StaticComponent count."));
	}
	OutNavigation.LocalStaticComponentCount = static_cast<uint16>(NextComponent);

	FString ValidationError;
	if (!LNPNavData::ValidateNavigation(OutNavigation, ValidationError))
	{
		return Fail(FString::Printf(TEXT("Baked Navigation is invalid: %s"), *ValidationError));
	}

	TArray<uint32> ComponentNodeCounts;
	ComponentNodeCounts.SetNumZeroed(OutNavigation.LocalStaticComponentCount);
	for (const FBakeLayer& Layer : BakedLayers)
	{
		for (const FBakeNode& Node : Layer.Nodes)
		{
			++ComponentNodeCounts[Node.Component];
		}
	}
	for (int32 Component = 0; Component < ComponentNodeCounts.Num(); ++Component)
	{
		OutTraversal.StaticComponents.Add({static_cast<uint16>(Component), ComponentNodeCounts[Component]});
	}

	TMap<FIntVector, TArray<FSpatialNode>> Spatial;
	TMap<uint32, FPortalCandidate> BestPortalByComponentPair;
	const double PortalDistanceSquared = FMath::Square(Settings.PortalSearchDistance);
	for (int32 LayerIndex = 0; LayerIndex < BakedLayers.Num(); ++LayerIndex)
	{
		const FBakeLayer& Layer = BakedLayers[LayerIndex];
		for (int32 NodeIndex = 0; NodeIndex < Layer.Nodes.Num(); ++NodeIndex)
		{
			const FBakeNode& Node = Layer.Nodes[NodeIndex];
			const FIntVector Cell = SpatialCell(Node.Position, Settings.PortalSearchDistance);
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 DZ = -1; DZ <= 1; ++DZ)
					{
						const TArray<FSpatialNode>* Candidates = Spatial.Find(Cell + FIntVector(DX, DY, DZ));
						if (Candidates == nullptr)
						{
							continue;
						}
						for (const FSpatialNode& OtherRef : *Candidates)
						{
							const FBakeLayer& OtherLayer = BakedLayers[OtherRef.LayerIndex];
							const FBakeNode& Other = OtherLayer.Nodes[OtherRef.NodeIndex];
							if (!Node.bBoundary && !Other.bBoundary)
							{
								continue;
							}
							const uint16 MinComponent = FMath::Min(Node.Component, Other.Component);
							const uint16 MaxComponent = FMath::Max(Node.Component, Other.Component);
							const uint32 PairKey = (static_cast<uint32>(MinComponent) << 16) | MaxComponent;
							const double DistanceSquared = FVector3d::DistSquared(Node.Position, Other.Position);
							if (DistanceSquared > PortalDistanceSquared)
							{
								continue;
							}
							++OutReport.PortalDistanceCandidateCount;
							if (!CanStep(Node, Other, Settings.Agent))
							{
								continue;
							}
							++OutReport.PortalStepCandidateCount;
							if (!HasEdgeClearance(Layer.LayerId, Node.Position, Node.Normal,
								OtherLayer.LayerId, Other.Position, Other.Normal))
							{
								continue;
							}
							++OutReport.PortalClearanceCandidateCount;
							FLNPLocalNavNodeRef NodeRef;
							FLNPLocalNavNodeRef OtherNodeRef;
							if (!LNPNavData::MakeLocalNodeRef(OutNavigation, Layer.LayerId, Node.I, Node.J, NodeRef)
								|| !LNPNavData::MakeLocalNodeRef(OutNavigation, OtherLayer.LayerId, Other.I, Other.J, OtherNodeRef))
							{
								return Fail(TEXT("Failed to resolve a baked portal endpoint."));
							}
							if (BakeNodeLess(NodeRef, OtherNodeRef))
							{
								Swap(NodeRef, OtherNodeRef);
							}
							FPortalCandidate& Best = BestPortalByComponentPair.FindOrAdd(PairKey);
							const bool bBetter = DistanceSquared < Best.DistanceSquared
								|| (DistanceSquared == Best.DistanceSquared
									&& (BakeNodeLess(OtherNodeRef, Best.A)
										|| (!BakeNodeLess(Best.A, OtherNodeRef) && BakeNodeLess(NodeRef, Best.B))));
							if (bBetter)
							{
								Best.A = OtherNodeRef;
								Best.B = NodeRef;
								Best.DistanceSquared = DistanceSquared;
								Best.MinClearanceClass = FMath::Min(Node.ClearanceClass, Other.ClearanceClass);
							}
						}
					}
				}
			}
		}
		for (int32 NodeIndex = 0; NodeIndex < Layer.Nodes.Num(); ++NodeIndex)
		{
			Spatial.FindOrAdd(SpatialCell(Layer.Nodes[NodeIndex].Position, Settings.PortalSearchDistance))
				.Add({LayerIndex, NodeIndex});
		}
	}
	for (const TPair<uint32, FPortalCandidate>& Entry : BestPortalByComponentPair)
	{
		const FPortalCandidate& Candidate = Entry.Value;
		FLNPNavPortal& Portal = OutTraversal.Portals.AddDefaulted_GetRef();
		Portal.A = Candidate.A;
		Portal.B = Candidate.B;
		Portal.MinClearanceClass = Candidate.MinClearanceClass;
		Portal.Flags = ELNPNavPortalFlags::Bidirectional;
	}
	OutTraversal.Portals.Sort(BakePortalLess);

	const FBakeLayer& Crust = BakedLayers[0];
	for (int32 Edge = 0; Edge < 3; ++Edge)
	{
		for (int32 Step = 0; Step <= Crust.Subdivisions; ++Step)
		{
			const FIntPoint Coord = LNPCrustAtlas::GetSeamSampleCoord(
				Crust.Subdivisions, static_cast<ELNPCrustSeamEdge>(Edge), Step);
			const int32* NodeIndex = Crust.NodeByCoord.Find(BakeCoordKey(Coord.X, Coord.Y));
			if (NodeIndex == nullptr)
			{
				continue;
			}
			FLNPNavSeamEndpoint& Endpoint = OutTraversal.SeamEndpoints.AddDefaulted_GetRef();
			if (!LNPNavData::MakeLocalNodeRef(OutNavigation, 0, Coord.X, Coord.Y, Endpoint.Node))
			{
				return Fail(TEXT("Failed to resolve a baked seam endpoint."));
			}
			Endpoint.Edge = static_cast<ELNPCrustSeamEdge>(Edge);
			Endpoint.SeamStep = static_cast<uint16>(Step);
			Endpoint.MinClearanceClass = Crust.Nodes[*NodeIndex].ClearanceClass;
		}
	}

	if (!LNPNavData::ValidateTraversal(OutTraversal, OutNavigation, ValidationError))
	{
		return Fail(FString::Printf(TEXT("Baked Traversal is invalid: %s"), *ValidationError));
	}

	OutReport.LayerCount = OutNavigation.Layers.Num();
	OutReport.StaticComponentCount = OutNavigation.LocalStaticComponentCount;
	OutReport.PortalCount = OutTraversal.Portals.Num();
	OutReport.SeamEndpointCount = OutTraversal.SeamEndpoints.Num();
	for (const FLNPNavLayer& Layer : OutNavigation.Layers)
	{
		OutReport.TileCount += Layer.Tiles.Num();
		for (const FLNPNavTile& Tile : Layer.Tiles)
		{
			OutReport.CellCount += Tile.Cells.Num();
		}
	}
	return true;
}
