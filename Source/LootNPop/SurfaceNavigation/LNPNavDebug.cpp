// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavQuery.h"
#include "SurfaceNavigation/LNPNavPathSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "LootNPop.h"

#if !UE_BUILD_SHIPPING
namespace
{
	enum class ENavDrawMode : uint8
	{
		Component,
		Tile,
		Group,
	};

	const TCHAR* LexToString(const ENavDrawMode Mode)
	{
		switch (Mode)
		{
		case ENavDrawMode::Tile: return TEXT("tile");
		case ENavDrawMode::Group: return TEXT("group");
		default: return TEXT("component");
		}
	}

	/** 보고서와 그림이 같은 ID에 같은 색을 쓰도록 ID 해시로 색상만 바꾼다. */
	FColor MakeIdColor(const uint32 Id)
	{
		const uint32 Hash = HashCombineFast(Id, 0x9E3779B9u);
		return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hash & 0xFF), 190, 255).ToFColor(true);
	}

	FString DescribeColor(const FColor Color)
	{
		return FString::Printf(TEXT("#%02X%02X%02X"), Color.R, Color.G, Color.B);
	}

	TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> TakeReadySnapshot(UWorld* World, const TCHAR* Tag)
	{
		const ULNPSurfaceDataSubsystem* SurfaceData = World ? World->GetSubsystem<ULNPSurfaceDataSubsystem>() : nullptr;
		TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot =
			SurfaceData ? SurfaceData->TakeSnapshot() : nullptr;
		if (!Snapshot.IsValid() || !Snapshot->Nav.IsValid())
		{
			UE_LOG(LogLootNPop, Warning, TEXT("[%s] %s -> NOT_READY"), Tag, *GetNameSafe(World));
			return nullptr;
		}
		return Snapshot;
	}

	/** 로컬 플레이어 폰의 발 위치. 구 내부 중력은 바깥쪽이므로 발은 중심 반대 방향으로 반높이만큼 떨어져 있다. */
	bool GetLocalFeet(UWorld* World, FVector3d& OutFeet)
	{
		const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			return false;
		}
		const FVector3d Location = Pawn->GetActorLocation();
		OutFeet = Location + Location.GetSafeNormal() * Pawn->GetSimpleCollisionHalfHeight();
		return true;
	}

	/** 발 위치에서 Support handle을 찾아 같은 Layer의 node로 투영한다. */
	bool ProjectFeet(const FLNPSurfaceDataSnapshot& Snapshot, const FVector3d& Feet, FLNPNavProjection& OutProjection)
	{
		FLNPSurfaceQuery Query;
		Query.WorldPosition = Feet;
		Query.MaxStepUp = 100.0;
		Query.MaxDrop = 300.0;
		FLNPSurfaceQueryResult Result;
		return LNPSurfaceDataLoading::QuerySupport(Snapshot, Query, Result) == ELNPSurfaceQueryStatus::HighConfidence
			&& LNPNavQuery::ProjectToNode(
				Snapshot, Feet, Result.Surface, LNPNavQuery::DefaultProjectionRadius, OutProjection);
	}

	/** runtime StaticNavComponent별 node 수. asset-local descriptor를 runtime ID로 합산한다. */
	TArray<uint64> ComputeComponentSizes(const FLNPSurfaceDataSnapshot& Snapshot)
	{
		TArray<uint64> Sizes;
		Sizes.SetNumZeroed(Snapshot.Nav.RuntimeStaticComponentCount);
		for (int32 Slot = 0; Slot < Snapshot.Slots.Num(); ++Slot)
		{
			for (const FLNPNavStaticComponent& Component : Snapshot.Slots[Slot].Traversal->StaticComponents)
			{
				const uint32 Runtime = LNPNavRuntime::GetRuntimeStaticComponent(
					Snapshot.Nav, Slot, Component.LocalStaticComponentId);
				if (Sizes.IsValidIndex(Runtime))
				{
					Sizes[Runtime] += Component.NodeCount;
				}
			}
		}
		return Sizes;
	}

	void LogNodeLine(const TCHAR* Tag, const FLNPSurfaceDataSnapshot& Snapshot, const FLNPNavProjection& Projection,
		const TArray<uint64>& ComponentSizes)
	{
		int32 Slot = INDEX_NONE;
		FLNPLocalNavNodeRef Local;
		LNPNavRuntime::ResolveRuntimeNodeRef(Snapshot.Nav, Projection.Node, Slot, Local);
		uint32 Component = MAX_uint32;
		FLNPNavGroupRef Group;
		LNPNavQuery::GetStaticComponent(Snapshot, Projection.Node, Component);
		LNPNavQuery::GetReachabilityGroup(Snapshot, Projection.Node, Group);
		const uint64 Size = ComponentSizes.IsValidIndex(Component) ? ComponentSizes[Component] : 0;
		UE_LOG(LogLootNPop, Display,
			TEXT("[%s] localNode slot=%d layer=%u(runtime %u) tile=%u cell=%u distance=%.1fcm component=%u(%s nodes=%llu) group=%u(%s) version=%u"),
			Tag, Slot, Local.LocalNavLayerId, Projection.Node.RuntimeNavLayerId, Local.TileId, Local.LocalCellIndex,
			Projection.Distance, Component, *DescribeColor(MakeIdColor(Component)), Size,
			Group.Group, *DescribeColor(MakeIdColor(Group.Group)), Group.ConnectivityGraphVersion);
	}

	/**
	 * LNP.SurfaceNav.NavReport [TopComponents]
	 * 게시된 Nav snapshot의 slot별 cell·Tile·component·portal·seam 수, 막힘, ReachabilityGroup, component 크기 분포와
	 * 로컬 폰 위치의 node를 기록한다. DrawNav와 같은 색 규칙을 쓴다.
	 */
	FAutoConsoleCommandWithWorldAndArgs GLNPNavReport(
		TEXT("LNP.SurfaceNav.NavReport"),
		TEXT("Log published Nav counts per slot, seam/blocked/portal totals, ReachabilityGroups, StaticNavComponent size ")
		TEXT("distribution, and the local pawn's projected node. Args: [TopComponents] (default 10)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			constexpr TCHAR Tag[] = TEXT("NavReport");
			const TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot = TakeReadySnapshot(World, Tag);
			if (!Snapshot.IsValid())
			{
				return;
			}
			const int32 TopCount = Args.Num() > 0 ? FMath::Max(0, FCString::Atoi(*Args[0])) : 10;
			const FLNPNavSnapshot& Nav = Snapshot->Nav;
			// 7b 탐색 분기 수 입력. seam link·portal은 제외한 같은 Layer grid edge 기준 차수다.
			int64 GridDegrees[7] = {};
			UE_LOG(LogLootNPop, Display,
				TEXT("[%s] %s NetMode=%d generation=%llu connectivityVersion=%u runtimeLayers=%u runtimeComponents=%u groups=%u seamLinks=%d blockedSeamNodes=%d blockedSeamEdges=%d"),
				Tag, *GetNameSafe(World), static_cast<int32>(World->GetNetMode()), Nav.SnapshotGeneration,
				Nav.ConnectivityGraphVersion, Nav.SlotLayerBase.Last(), Nav.RuntimeStaticComponentCount,
				Nav.ReachabilityGroupCount, Nav.SeamLinks.Num(), Nav.BlockedSeamNodes.Num(), Nav.BlockedSeamEdges.Num());

			for (int32 Slot = 0; Slot < Snapshot->Slots.Num(); ++Slot)
			{
				const FLNPSurfaceDataSlotSnapshot& SlotSnapshot = Snapshot->Slots[Slot];
				int32 Tiles = 0;
				int32 Cells = 0;
				int32 EdgeEnds = 0;
				int32 IsolatedCells = 0;
				for (const FLNPNavLayer& Layer : SlotSnapshot.Navigation->Layers)
				{
					Tiles += Layer.Tiles.Num();
					for (const FLNPNavTile& Tile : Layer.Tiles)
					{
						Cells += Tile.Cells.Num();
						for (const FLNPNavCell& Cell : Tile.Cells)
						{
							const int32 Degree = FMath::CountBits(Cell.EdgeMask);
							EdgeEnds += Degree;
							IsolatedCells += Degree == 0 ? 1 : 0;
							++GridDegrees[FMath::Min(Degree, 6)];
						}
					}
				}
				TSet<uint32> RuntimeComponents;
				for (uint32 Local = 0; Local < SlotSnapshot.Navigation->LocalStaticComponentCount; ++Local)
				{
					RuntimeComponents.Add(LNPNavRuntime::GetRuntimeStaticComponent(Nav, Slot, static_cast<uint16>(Local)));
				}
				UE_LOG(LogLootNPop, Display,
					TEXT("[%s] slot=%d asset=%s layers=%d(runtime %u..%u) tiles=%d cells=%d edges=%d isolatedCells=%d localComponents=%u runtimeComponents=%d portals=%d seamEndpoints=%d"),
					Tag, Slot, *SlotSnapshot.SurfaceDataAsset.GetAssetName(), SlotSnapshot.Navigation->Layers.Num(),
					Nav.SlotLayerBase[Slot], Nav.SlotLayerBase[Slot + 1] - 1, Tiles, Cells, EdgeEnds / 2, IsolatedCells,
					SlotSnapshot.Navigation->LocalStaticComponentCount, RuntimeComponents.Num(),
					SlotSnapshot.Traversal->Portals.Num(), SlotSnapshot.Traversal->SeamEndpoints.Num());
			}

			UE_LOG(LogLootNPop, Display,
				TEXT("[%s] gridDegree 0:%lld 1:%lld 2:%lld 3:%lld 4:%lld 5:%lld 6:%lld (all slots, excludes seam links and portals)"),
				Tag, GridDegrees[0], GridDegrees[1], GridDegrees[2], GridDegrees[3], GridDegrees[4], GridDegrees[5],
				GridDegrees[6]);

			const TArray<uint64> Sizes = ComputeComponentSizes(*Snapshot);
			int32 Buckets[5] = {};
			for (const uint64 Size : Sizes)
			{
				++Buckets[Size <= 1 ? 0 : Size <= 10 ? 1 : Size <= 100 ? 2 : Size <= 1000 ? 3 : 4];
			}
			UE_LOG(LogLootNPop, Display,
				TEXT("[%s] componentSizes nodes<=1:%d 2-10:%d 11-100:%d 101-1000:%d >1000:%d"),
				Tag, Buckets[0], Buckets[1], Buckets[2], Buckets[3], Buckets[4]);
			TArray<uint32> Order;
			Order.Reserve(Sizes.Num());
			for (uint32 Component = 0; Component < static_cast<uint32>(Sizes.Num()); ++Component)
			{
				Order.Add(Component);
			}
			Order.StableSort([&Sizes](const uint32 A, const uint32 B) { return Sizes[A] > Sizes[B]; });
			for (int32 Rank = 0; Rank < FMath::Min(TopCount, Order.Num()); ++Rank)
			{
				const uint32 Component = Order[Rank];
				const uint32 Group = Nav.ReachabilityGroupByStaticComponent[Component];
				UE_LOG(LogLootNPop, Display, TEXT("[%s] top%d component=%u(%s) nodes=%llu group=%u(%s)"),
					Tag, Rank + 1, Component, *DescribeColor(MakeIdColor(Component)), Sizes[Component],
					Group, *DescribeColor(MakeIdColor(Group)));
			}

			FVector3d Feet;
			FLNPNavProjection Projection;
			if (!GetLocalFeet(World, Feet))
			{
				UE_LOG(LogLootNPop, Display, TEXT("[%s] localNode none (no local pawn)"), Tag);
			}
			else if (!ProjectFeet(*Snapshot, Feet, Projection))
			{
				FLNPSurfaceQuery Query;
				Query.WorldPosition = Feet;
				Query.MaxStepUp = 100.0;
				Query.MaxDrop = 300.0;
				FLNPSurfaceQueryResult Result;
				const ELNPSurfaceQueryStatus Status = LNPSurfaceDataLoading::QuerySupport(*Snapshot, Query, Result);
				UE_LOG(LogLootNPop, Display,
					TEXT("[%s] localNode none within %.0fcm of feet %s (radius %.1f) supportStatus=%d slot=%u layer=%u"),
					Tag, LNPNavQuery::DefaultProjectionRadius, *Feet.ToString(), Feet.Length(), static_cast<int32>(Status),
					Result.Surface.OctantSlot, Result.Surface.LocalLayerId);
			}
			else
			{
				LogNodeLine(Tag, *Snapshot, Projection, Sizes);
			}
			if (const ULNPNavPathSubsystem* Paths = World->GetSubsystem<ULNPNavPathSubsystem>())
			{
				int32 StatusCounts[static_cast<int32>(ELNPNavPathStatus::Cancelled) + 1] = {};
				Paths->GetScheduler().VisitResults([&](FMassEntityHandle, const FLNPNavPathResult& Result)
				{
					++StatusCounts[static_cast<int32>(Result.Status)];
				});
				const FLNPNavPathScheduler& Scheduler = Paths->GetScheduler();
				const FLNPNavPathSchedulerStats& Stats = Scheduler.GetStats();
				UE_LOG(LogLootNPop, Display,
					TEXT("[%s] paths queued=%d running=%d success=%d noPath=%d unreachable=%d noNode=%d stale=%d cancelled=%d submitted=%llu expansions=%llu lastTickExpansions=%d lastTickMs=%.3f cacheHits=%llu cacheMisses=%llu followerFrames=%llu followingPathFrames=%llu waypointsAdvanced=%llu"),
					Tag, Scheduler.GetQueuedCount(), Scheduler.GetRunningCount(),
					StatusCounts[static_cast<int32>(ELNPNavPathStatus::Succeeded)],
					StatusCounts[static_cast<int32>(ELNPNavPathStatus::NoPath)],
					StatusCounts[static_cast<int32>(ELNPNavPathStatus::Unreachable)],
					StatusCounts[static_cast<int32>(ELNPNavPathStatus::NoNode)],
					StatusCounts[static_cast<int32>(ELNPNavPathStatus::Stale)],
					StatusCounts[static_cast<int32>(ELNPNavPathStatus::Cancelled)],
					Stats.Submitted, Stats.Expansions, Stats.LastTickExpansions,
					Paths->GetLastTickSeconds() * 1000.0, Stats.CacheHits, Stats.CacheMisses,
					Paths->GetFollowerFrames(), Paths->GetFollowingPathFrames(), Paths->GetFollowerWaypointsAdvanced());
			}
		}));

	/**
	 * LNP.SurfaceNav.DrawNav [component|tile|group] [Radius] [Seconds]
	 * 로컬 폰 주변 반경 안의 Nav node·edge를 모드별 ID 색으로 그린다. 막힌 이음매 node·edge는 빨강,
	 * seam link는 청록 세로선, Layer portal은 자홍 굵은 선이다. 폰이 서 있는 node와 ID를 라벨로 표시한다.
	 */
	FAutoConsoleCommandWithWorldAndArgs GLNPDrawNav(
		TEXT("LNP.SurfaceNav.DrawNav"),
		TEXT("Draw Nav nodes/edges around the local pawn colored by StaticNavComponent, Tile, or ReachabilityGroup. ")
		TEXT("Red = blocked seam node/edge, cyan = seam link, magenta = Layer portal. ")
		TEXT("Args: [component|tile|group] (default component) [Radius] (default 3000) [Seconds] (default 15)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			constexpr TCHAR Tag[] = TEXT("DrawNav");
			const TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot = TakeReadySnapshot(World, Tag);
			FVector3d Center;
			if (!Snapshot.IsValid() || !GetLocalFeet(World, Center))
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[%s] Needs a published snapshot and a local pawn."), Tag);
				return;
			}
			ENavDrawMode Mode = ENavDrawMode::Component;
			if (Args.Num() > 0)
			{
				Mode = Args[0] == TEXT("tile") ? ENavDrawMode::Tile
					: Args[0] == TEXT("group") ? ENavDrawMode::Group
					: ENavDrawMode::Component;
			}
			const double Radius = Args.Num() > 1 ? FMath::Max(100.0, FCString::Atod(*Args[1])) : 3000.0;
			const float Seconds = Args.Num() > 2 ? FMath::Max(0.1f, FCString::Atof(*Args[2])) : 15.0f;
			const FLNPNavSnapshot& Nav = Snapshot->Nav;
			const ULNPNavPathSubsystem* NavPaths = World->GetSubsystem<ULNPNavPathSubsystem>();
			const TSharedPtr<const FLNPNavOverlay, ESPMode::ThreadSafe> Overlay = NavPaths ? NavPaths->TakeOverlay() : nullptr;
			// 지면과 겹쳐 가려지지 않도록 중심 방향(위)으로 살짝 띄운다.
			auto Lift = [](const FVector3d& Point) { return Point - Point.GetSafeNormal() * 8.0; };
			auto NodeColor = [&](const FLNPNavNodeRef& Node, const uint16 TileId)
			{
				uint32 Component = MAX_uint32;
				LNPNavQuery::GetStaticComponent(*Snapshot, Node, Component);
				switch (Mode)
				{
				case ENavDrawMode::Tile:
					return MakeIdColor(HashCombineFast(static_cast<uint32>(Node.RuntimeNavLayerId), TileId));
				case ENavDrawMode::Group:
					return MakeIdColor(Nav.ReachabilityGroupByStaticComponent.IsValidIndex(Component)
						? Nav.ReachabilityGroupByStaticComponent[Component] : MAX_uint32);
				default:
					return MakeIdColor(Component);
				}
			};

			int32 DrawnNodes = 0;
			int32 DrawnEdges = 0;
			int32 DrawnBlocked = 0;
			for (int32 Slot = 0; Slot < Snapshot->Slots.Num(); ++Slot)
			{
				const FLNPSurfaceDataSlotSnapshot& SlotSnapshot = Snapshot->Slots[Slot];
				const FLNPNavData& Navigation = *SlotSnapshot.Navigation;
				for (const FLNPNavLayer& Layer : Navigation.Layers)
				{
					const double LayerRadius = SlotSnapshot.Support->Layers[Layer.LocalSupportLayerId].BaseRadius;
					for (const FLNPNavTile& Tile : Layer.Tiles)
					{
						for (const FLNPNavCell& Cell : Tile.Cells)
						{
							const FIntPoint Coord = LNPNavData::DecodeTileAddress(Tile.TileX, Tile.TileY, Cell.LocalCellIndex);
							// Support 조회 전에 기준 반지름 구면 위 방향으로 먼 node를 먼저 거른다.
							const FVector3d Rough = SlotSnapshot.SlotRotation.RotateVector(
								LNPSupportAtlas::GetSampleDirection(Layer.Subdivisions, Coord.X, Coord.Y)) * LayerRadius;
							if (FVector3d::DistSquared(Rough, Center) > FMath::Square(Radius * 1.5 + 500.0))
							{
								continue;
							}
							FLNPNavNodeRef Node;
							FVector3d Point;
							FVector3f Normal;
							if (!LNPNavRuntime::MakeRuntimeNodeRef(Nav, Slot,
									{Layer.LocalNavLayerId, Tile.TileId, Cell.LocalCellIndex}, Node)
								|| !LNPNavQuery::GetNodeSupport(*Snapshot, Node, Point, Normal)
								|| FVector3d::DistSquared(Point, Center) > FMath::Square(Radius))
							{
								continue;
							}
						const bool bBlocked = LNPNavRuntime::IsBlockedSeamNode(Nav, Node);
						const bool bPodBlocked = LNPNavOverlay::IsBlocked(Overlay.Get(), LNPNavGraph::ToGraphNode(Nav, Node));
						const FColor Color = bPodBlocked ? FColor::Orange : bBlocked ? FColor::Red : NodeColor(Node, Tile.TileId);
						DrawDebugPoint(World, Lift(Point), bBlocked || bPodBlocked ? 16.0f : 9.0f, Color, false, Seconds);
							++DrawnNodes;
						DrawnBlocked += bBlocked || bPodBlocked ? 1 : 0;
							// 대칭 edge를 한 번만 그리도록 양의 방향(0, 2, 4)만 본다.
							for (const ELNPNavNeighbor Direction :
								{ELNPNavNeighbor::IPositive, ELNPNavNeighbor::JPositive, ELNPNavNeighbor::IPositiveJNegative})
							{
								FIntPoint NeighborCoord;
								FLNPLocalNavNodeRef NeighborLocal;
								FLNPNavNodeRef Neighbor;
								FVector3d NeighborPoint;
								FVector3f NeighborNormal;
								if ((Cell.EdgeMask & LNPNavData::GetNeighborBit(Direction)) == 0
									|| !LNPNavData::TryGetNeighborCoord(Layer.Subdivisions, Coord.X, Coord.Y, Direction, NeighborCoord)
									|| !LNPNavData::MakeLocalNodeRef(Navigation, Layer.LocalNavLayerId,
										NeighborCoord.X, NeighborCoord.Y, NeighborLocal)
									|| !LNPNavRuntime::MakeRuntimeNodeRef(Nav, Slot, NeighborLocal, Neighbor)
									|| !LNPNavQuery::GetNodeSupport(*Snapshot, Neighbor, NeighborPoint, NeighborNormal))
								{
									continue;
								}
								DrawDebugLine(World, Lift(Point), Lift(NeighborPoint), Color, false, Seconds, 0, 1.5f);
								++DrawnEdges;
							}
						}
					}
				}
			}

			auto DrawIfNear = [&](const FLNPNavNodeRef& A, const FLNPNavNodeRef& B, const FColor Color, const float Thickness)
			{
				FVector3d PointA;
				FVector3d PointB;
				FVector3f Normal;
				if (LNPNavQuery::GetNodeSupport(*Snapshot, A, PointA, Normal)
					&& LNPNavQuery::GetNodeSupport(*Snapshot, B, PointB, Normal)
					&& FVector3d::DistSquared(PointA, Center) <= FMath::Square(Radius))
				{
					DrawDebugLine(World, Lift(PointA), Lift(PointB), Color, false, Seconds, 0, Thickness);
					return true;
				}
				return false;
			};
			int32 DrawnSeamLinks = 0;
			for (const FLNPNavSeamLink& Link : Nav.SeamLinks)
			{
				FVector3d Point;
				FVector3f Normal;
				if (LNPNavQuery::GetNodeSupport(*Snapshot, Link.A, Point, Normal)
					&& FVector3d::DistSquared(Point, Center) <= FMath::Square(Radius))
				{
					DrawDebugLine(World, Point, Point - Point.GetSafeNormal() * 60.0, FColor::Cyan, false, Seconds, 0, 2.0f);
					++DrawnSeamLinks;
				}
			}
			for (const FLNPNavEdgeRef& Edge : Nav.BlockedSeamEdges)
			{
				DrawIfNear(Edge.From, Edge.To, FColor::Red, 4.0f);
			}
			int32 DrawnPortals = 0;
			for (int32 Slot = 0; Slot < Snapshot->Slots.Num(); ++Slot)
			{
				for (const FLNPNavPortal& Portal : Snapshot->Slots[Slot].Traversal->Portals)
				{
					FLNPNavNodeRef A;
					FLNPNavNodeRef B;
					if (LNPNavRuntime::MakeRuntimeNodeRef(Nav, Slot, Portal.A, A)
						&& LNPNavRuntime::MakeRuntimeNodeRef(Nav, Slot, Portal.B, B))
					{
						DrawnPortals += DrawIfNear(A, B, FColor::Magenta, 6.0f) ? 1 : 0;
					}
				}
			}

			FLNPNavProjection Projection;
			if (ProjectFeet(*Snapshot, Center, Projection))
			{
				uint32 Component = MAX_uint32;
				FLNPNavGroupRef Group;
				LNPNavQuery::GetStaticComponent(*Snapshot, Projection.Node, Component);
				LNPNavQuery::GetReachabilityGroup(*Snapshot, Projection.Node, Group);
				DrawDebugString(World, Lift(Projection.Point) - Projection.Point.GetSafeNormal() * 100.0,
					FString::Printf(TEXT("C%u G%u v%u"), Component, Group.Group, Group.ConnectivityGraphVersion),
					nullptr, FColor::White, Seconds);
				DrawDebugPoint(World, Lift(Projection.Point), 20.0f, FColor::White, false, Seconds);
				const TArray<uint64> Sizes = ComputeComponentSizes(*Snapshot);
				LogNodeLine(Tag, *Snapshot, Projection, Sizes);
			}
			int32 DrawnPaths = 0;
			if (NavPaths)
			{
				NavPaths->GetScheduler().VisitResults([&](FMassEntityHandle, const FLNPNavPathResult& Result)
				{
					if (!Result.Path.IsValid())
					{
						return;
					}
					const TArray<FLNPNavPathWaypoint>& Waypoints = Result.Path->Waypoints;
					for (int32 Index = 1; Index < Waypoints.Num(); ++Index)
					{
						if (FVector3d::DistSquared(Waypoints[Index].Location, Center) <= FMath::Square(Radius))
						{
							DrawDebugLine(World, Lift(Waypoints[Index - 1].Location), Lift(Waypoints[Index].Location),
								FColor::Yellow, false, Seconds, 0, 3.0f);
							++DrawnPaths;
						}
					}
				});
			}
			UE_LOG(LogLootNPop, Display,
				TEXT("[%s] mode=%s radius=%.0f nodes=%d edges=%d blockedNodes=%d seamLinks=%d portals=%d pathSegments=%d"),
				Tag, LexToString(Mode), Radius, DrawnNodes, DrawnEdges, DrawnBlocked, DrawnSeamLinks, DrawnPortals, DrawnPaths);
		}));
}
#endif
