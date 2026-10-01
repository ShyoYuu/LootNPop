// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavPathScheduler.h"

void FLNPNavPathScheduler::VisitResults(TFunctionRef<void(FMassEntityHandle, const FLNPNavPathResult&)> Visitor) const
{
	for (const TPair<FMassEntityHandle, FRecord>& Entry : Records)
	{
		Visitor(Entry.Key, Entry.Value.Result);
	}
}

#include "SurfaceNavigation/LNPNavOverlay.h"
#include "SurfaceNavigation/LNPNavRuntime.h"

#include "Async/ParallelFor.h"

bool FLNPNavPath::IsCurrent(const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay) const
{
	if (SnapshotGeneration != Nav.SnapshotGeneration || ConnectivityGraphVersion != Nav.ConnectivityGraphVersion)
	{
		return false;
	}
	for (int32 Index = 0; Index < TraversedTiles.Num(); ++Index)
	{
		if (LNPNavOverlay::GetTileRevision(Overlay, TraversedTiles[Index]) != TileRevisions[Index])
		{
			return false;
		}
	}
	return true;
}

uint32 FLNPNavPathScheduler::Submit(const FLNPNavPathRequest& Request)
{
	Cancel(Request.Owner);
	FRecord& Record = Records.Add(Request.Owner);
	Record.Request = Request;
	Record.Result.Serial = NextSerial++;
	Record.Result.Status = ELNPNavPathStatus::Queued;
	Queues[static_cast<int32>(Request.Priority)].PushLast({Request.Owner, Record.Result.Serial});
	++QueuedCount;
	++Stats.Submitted;
	return Record.Result.Serial;
}

void FLNPNavPathScheduler::Cancel(const FMassEntityHandle Owner)
{
	FRecord* Record = Records.Find(Owner);
	if (Record == nullptr)
	{
		return;
	}
	if (Record->Result.Status == ELNPNavPathStatus::Queued)
	{
		--QueuedCount;
		++Stats.Finished[static_cast<int32>(ELNPNavPathStatus::Cancelled)];
	}
	else if (Record->Result.Status == ELNPNavPathStatus::Running)
	{
		const int32 RunningIndex = Running.IndexOfByPredicate([Owner](const FRunningSearch& Search) { return Search.Owner == Owner; });
		if (RunningIndex != INDEX_NONE)
		{
			ReleaseRunning(RunningIndex);
		}
		++Stats.Finished[static_cast<int32>(ELNPNavPathStatus::Cancelled)];
	}
	// 큐에 남은 옛 항목은 serial 불일치로 건너뛴다.
	Records.Remove(Owner);
}

int32 FLNPNavPathScheduler::RequeueInvalidatedPaths(const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay)
{
	int32 Requeued = 0;
	for (auto& Pair : Records)
	{
		FRecord& Record = Pair.Value;
		if (!Record.Result.Path.IsValid() || Record.Result.Status == ELNPNavPathStatus::Queued
			|| Record.Result.Status == ELNPNavPathStatus::Running || Record.Result.Path->IsCurrent(Nav, Overlay))
		{
			continue;
		}
		Record.Result.Path.Reset();
		Record.Result.Status = ELNPNavPathStatus::Queued;
		Record.Result.bFromCache = false;
		Record.Result.Expansions = 0;
		Record.Request.Priority = ELNPNavPathPriority::Replan;
		Queues[static_cast<int32>(ELNPNavPathPriority::Replan)].PushLast({Pair.Key, Record.Result.Serial});
		++QueuedCount;
		++Requeued;
	}
	return Requeued;
}

ELNPNavPathStatus FLNPNavPathScheduler::GetResult(
	const FMassEntityHandle Owner, const uint32 Serial, FLNPNavPathResult& OutResult) const
{
	OutResult = FLNPNavPathResult();
	const FRecord* Record = Records.Find(Owner);
	if (Record == nullptr)
	{
		return ELNPNavPathStatus::None;
	}
	if (Record->Result.Serial != Serial)
	{
		OutResult.Serial = Serial;
		OutResult.Status = ELNPNavPathStatus::Cancelled;
		return OutResult.Status;
	}
	OutResult = Record->Result;
	return OutResult.Status;
}

uint64 FLNPNavPathScheduler::GetScratchBytes() const
{
	uint64 Bytes = 0;
	for (const FLNPNavSearchScratch& Scratch : Scratches)
	{
		Bytes += Scratch.GetAllocatedBytes();
	}
	return Bytes;
}

void FLNPNavPathScheduler::Reset()
{
	Records.Reset();
	for (TDeque<FQueueEntry>& Queue : Queues)
	{
		Queue.Reset();
	}
	QueuedCount = 0;
	Running.Reset();
	ScratchInUse.Init(false, Scratches.Num());
	Cache.Reset();
	Stats = FLNPNavPathSchedulerStats();
}

bool FLNPNavPathScheduler::PopNextQueued(FQueueEntry& OutEntry)
{
	for (TDeque<FQueueEntry>& Queue : Queues)
	{
		while (Queue.TryPopFirst(OutEntry))
		{
			const FRecord* Record = Records.Find(OutEntry.Owner);
			if (Record != nullptr && Record->Result.Serial == OutEntry.Serial && Record->Result.Status == ELNPNavPathStatus::Queued)
			{
				return true;
			}
		}
	}
	return false;
}

int32 FLNPNavPathScheduler::AcquireScratch()
{
	const int32 Limit = FMath::Max(1, Settings.ScratchCount);
	for (int32 Index = 0; Index < FMath::Min(Limit, Scratches.Num()); ++Index)
	{
		if (!ScratchInUse[Index])
		{
			ScratchInUse[Index] = true;
			return Index;
		}
	}
	if (Scratches.Num() < Limit)
	{
		// 전역 node 수 크기 배열은 첫 BeginSearch가 잡는다.
		Scratches.AddDefaulted();
		ScratchInUse.Add(true);
		return Scratches.Num() - 1;
	}
	return INDEX_NONE;
}

void FLNPNavPathScheduler::ReleaseRunning(const int32 RunningIndex)
{
	ScratchInUse[Running[RunningIndex].ScratchIndex] = false;
	Running.RemoveAt(RunningIndex, EAllowShrinking::No);
}

void FLNPNavPathScheduler::Finish(FRecord& Record, const ELNPNavPathStatus Status)
{
	Record.Result.Status = Status;
	++Stats.Finished[static_cast<int32>(Status)];
}

void FLNPNavPathScheduler::Tick(
	const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay, const TFunctionRef<bool(FMassEntityHandle)> IsOwnerValid)
{
	++TickIndex;
	++Stats.Ticks;
	Stats.LastTickExpansions = 0;
	Stats.LastTickStarted = 0;

	// 1) 사라진 owner의 요청은 결과를 버린다.
	for (auto It = Records.CreateIterator(); It; ++It)
	{
		if (IsOwnerValid(It.Key()))
		{
			continue;
		}
		const ELNPNavPathStatus Status = It.Value().Result.Status;
		if (Status == ELNPNavPathStatus::Queued || Status == ELNPNavPathStatus::Running)
		{
			QueuedCount -= Status == ELNPNavPathStatus::Queued ? 1 : 0;
			const FMassEntityHandle Owner = It.Key();
			const int32 RunningIndex = Running.IndexOfByPredicate([Owner](const FRunningSearch& Search) { return Search.Owner == Owner; });
			if (RunningIndex != INDEX_NONE)
			{
				ReleaseRunning(RunningIndex);
			}
			++Stats.Finished[static_cast<int32>(ELNPNavPathStatus::Cancelled)];
		}
		It.RemoveCurrent();
	}

	// 2) 프레임을 넘긴 요청은 시작 때의 snapshot·overlay와 같아야 이어 간다. 다르면 결과를 섞지 않고 Stale로 끝낸다.
	const uint32 OverlayRevision = LNPNavOverlay::GetRevision(Overlay);
	for (int32 Index = Running.Num() - 1; Index >= 0; --Index)
	{
		const FRunningSearch& Search = Running[Index];
		if (Search.Search.SnapshotGeneration != Nav.SnapshotGeneration
			|| Search.Search.ConnectivityGraphVersion != Nav.ConnectivityGraphVersion
			|| Search.OverlayRevision != OverlayRevision)
		{
			Finish(Records.FindChecked(Search.Owner), ELNPNavPathStatus::Stale);
			ReleaseRunning(Index);
		}
	}

	// 3) 예산이 남는 동안 시작 → 병렬 확장 → 종료 처리를 반복한다. 매 라운드는 요청을 끝내거나 요청마다 1개 이상 확장한다.
	int32 Budget = Settings.ExpansionsPerFrame;
	while (Budget > 0)
	{
		FQueueEntry Entry;
		while (Budget > 0 && Running.Num() < FMath::Max(1, Settings.ScratchCount) && PopNextQueued(Entry))
		{
			--QueuedCount;
			Budget -= Settings.ResolveCostInExpansions;
			++Stats.LastTickStarted;
			StartRequest(Nav, Overlay, Records.FindChecked(Entry.Owner));
		}
		if (Running.IsEmpty() || Budget <= 0)
		{
			break;
		}
		Stats.MaxConcurrentRunning = FMath::Max(Stats.MaxConcurrentRunning, Running.Num());

		const int32 Share = FMath::Max(1, Budget / Running.Num());
		for (FRunningSearch& Search : Running)
		{
			Search.ExpansionsBefore = Search.Search.Expansions;
		}
		auto Step = [this, &Nav, Overlay, Share](const int32 Index)
		{
			FRunningSearch& Search = Running[Index];
			LNPNavPathfinding::StepSearch(Nav, Scratches[Search.ScratchIndex], Search.Search, Share, Overlay);
		};
		if (Settings.bParallel && Running.Num() > 1)
		{
			ParallelFor(Running.Num(), Step);
		}
		else
		{
			for (int32 Index = 0; Index < Running.Num(); ++Index)
			{
				Step(Index);
			}
		}

		for (int32 Index = Running.Num() - 1; Index >= 0; --Index)
		{
			FRunningSearch& Search = Running[Index];
			const int32 Used = Search.Search.Expansions - Search.ExpansionsBefore;
			Budget -= Used;
			Stats.LastTickExpansions += Used;
			Stats.Expansions += Used;
			if (Search.Search.Status != ELNPNavSearchStatus::Running)
			{
				FinishSearch(Nav, Overlay, Search);
				ReleaseRunning(Index);
			}
		}
	}

	for (const FRunningSearch& Search : Running)
	{
		++Records.FindChecked(Search.Owner).Result.RunningTicks;
	}
}

void FLNPNavPathScheduler::StartRequest(const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay, FRecord& Record)
{
	const FLNPNavPathRequest& Request = Record.Request;
	FLNPNavEndpointQuery Query;
	Query.StartPosition = Request.StartPosition;
	Query.StartSurface = &Request.StartSurface;
	Query.GoalPosition = Request.GoalPosition;
	Query.GoalSurface = &Request.GoalSurface;
	Query.SnapRadius = Request.SnapRadius;
	Query.ApproachRadius = Request.ApproachRadius;
	const FLNPNavEndpoints Endpoints = LNPNavGraph::ResolveEndpoints(Nav, Query, Overlay);

	int32 Target = INDEX_NONE;
	ELNPNavPathStatus EndpointStatus = ELNPNavPathStatus::Succeeded;
	FCacheKey Key;
	switch (Endpoints.Status)
	{
	case ELNPNavEndpointStatus::Stale:
		Finish(Record, ELNPNavPathStatus::Stale);
		return;
	case ELNPNavEndpointStatus::Reachable:
		Key = {LNPNavGraph::GetTileKey(Nav, Endpoints.StartNode), LNPNavGraph::GetTileKey(Nav, Endpoints.GoalNode),
			Nav.SnapshotGeneration, Nav.ConnectivityGraphVersion};
		if (FLNPNavPathPtr Cached = FindCachedPath(Nav, Overlay, Key, Endpoints.StartNode, Endpoints.GoalNode))
		{
			Record.Result.Path = MoveTemp(Cached);
			Record.Result.bFromCache = true;
			Finish(Record, ELNPNavPathStatus::Succeeded);
			return;
		}
		Target = Endpoints.GoalNode;
		break;
	case ELNPNavEndpointStatus::Unreachable:
	case ELNPNavEndpointStatus::NoNode:
		// D-063: 도달 불가여도 접근점이 있으면 접근점까지 경로를 찾는다. 이 경로는 cache에 넣지 않는다.
		EndpointStatus = Endpoints.Status == ELNPNavEndpointStatus::Unreachable
			? ELNPNavPathStatus::Unreachable : ELNPNavPathStatus::NoNode;
		if (Endpoints.StartNode == INDEX_NONE || Endpoints.ApproachNode == INDEX_NONE)
		{
			Finish(Record, EndpointStatus);
			return;
		}
		Target = Endpoints.ApproachNode;
		break;
	}

	const int32 ScratchIndex = AcquireScratch();
	check(ScratchIndex != INDEX_NONE);
	FRunningSearch& Search = Running.AddDefaulted_GetRef();
	Search.Owner = Request.Owner;
	Search.Serial = Record.Result.Serial;
	Search.ScratchIndex = ScratchIndex;
	Search.EndpointStatus = EndpointStatus;
	Search.OverlayRevision = LNPNavOverlay::GetRevision(Overlay);
	Search.StartTile = Key.StartTile;
	Search.GoalTile = Key.GoalTile;
	FLNPNavSearchParams Params;
	Params.MaxExpansions = Settings.MaxExpansionsPerRequest;
	Record.Result.Status = ELNPNavPathStatus::Running;
	LNPNavPathfinding::BeginSearch(Nav, Endpoints.StartNode, Target, Params, Scratches[ScratchIndex], Search.Search, Overlay);
	if (Search.Search.Status != ELNPNavSearchStatus::Running)
	{
		FinishSearch(Nav, Overlay, Search);
		ReleaseRunning(Running.Num() - 1);
	}
}

void FLNPNavPathScheduler::FinishSearch(const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay, FRunningSearch& Search)
{
	FRecord& Record = Records.FindChecked(Search.Owner);
	Record.Result.Expansions = Search.Search.Expansions;
	const bool bApproach = Search.EndpointStatus != ELNPNavPathStatus::Succeeded;
	switch (Search.Search.Status)
	{
	case ELNPNavSearchStatus::Found:
	{
		TArray<int32> Nodes;
		if (LNPNavPathfinding::ExtractNodePath(Scratches[Search.ScratchIndex], Search.Search, Nodes))
		{
			Record.Result.Path = BuildPath(Nav, Overlay, Nodes, Search.Search.PathCost);
			if (!bApproach)
			{
				AddToCache({Search.StartTile, Search.GoalTile, Nav.SnapshotGeneration, Nav.ConnectivityGraphVersion},
					Record.Result.Path);
			}
			Finish(Record, Search.EndpointStatus);
		}
		else
		{
			Finish(Record, bApproach ? Search.EndpointStatus : ELNPNavPathStatus::NoPath);
		}
		break;
	}
	case ELNPNavSearchStatus::NoPath:
		// 접근점까지 길이 없어도 목표 도달성 판정은 그대로다.
		Finish(Record, bApproach ? Search.EndpointStatus : ELNPNavPathStatus::NoPath);
		break;
	case ELNPNavSearchStatus::Unreachable:
		// ResolveEndpoints가 group을 맞췄으므로 오지 않지만, 오면 도달 불가다.
		Finish(Record, ELNPNavPathStatus::Unreachable);
		break;
	default:
		// Invalid: 탐색 중 snapshot이 달라졌거나 입력 node가 무효다. 다시 요청하게 한다.
		Finish(Record, ELNPNavPathStatus::Stale);
		break;
	}
}

FLNPNavPathPtr FLNPNavPathScheduler::BuildPath(
	const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay, const TConstArrayView<int32> Nodes, const double Cost) const
{
	TSharedRef<FLNPNavPath, ESPMode::ThreadSafe> Path = MakeShared<FLNPNavPath, ESPMode::ThreadSafe>();
	Path->SnapshotGeneration = Nav.SnapshotGeneration;
	Path->ConnectivityGraphVersion = Nav.ConnectivityGraphVersion;
	Path->Cost = Cost;
	TArray<int32> Waypoints;
	LNPNavPathfinding::SimplifyPath(Nav, Nodes, Waypoints, Overlay);
	const FLNPNavGraph& Graph = Nav.Graph;
	Path->Waypoints.Reserve(Waypoints.Num());
	for (const int32 Node : Waypoints)
	{
		Path->Waypoints.Add({Graph.GetWorldPoint(Graph.GetSlot(Node), Node), Node});
	}

	// 구간 cell의 Tile을 모은다. seam link·portal 전이 구간은 직선 검사 대상이 아니므로 양 끝 node만 센다.
	TArray<uint32> Tiles;
	TArray<int32> SegmentNodes;
	for (int32 Index = 0; Index < Waypoints.Num(); ++Index)
	{
		Tiles.Add(LNPNavGraph::GetTileKey(Nav, Waypoints[Index]));
		if (Index > 0 && LNPNavGraph::CollectDirectWalkNodes(Nav, Waypoints[Index - 1], Waypoints[Index], SegmentNodes, Overlay))
		{
			for (const int32 Node : SegmentNodes)
			{
				Tiles.Add(LNPNavGraph::GetTileKey(Nav, Node));
			}
		}
	}
	Tiles.Sort();
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		if (Index == 0 || Tiles[Index] != Tiles[Index - 1])
		{
			Path->TraversedTiles.Add(Tiles[Index]);
			Path->TileRevisions.Add(LNPNavOverlay::GetTileRevision(Overlay, Tiles[Index]));
		}
	}
	return Path;
}

FLNPNavPathPtr FLNPNavPathScheduler::FindCachedPath(
	const FLNPNavSnapshot& Nav, const FLNPNavOverlay* Overlay, const FCacheKey& Key, const int32 StartNode, const int32 GoalNode)
{
	FCacheEntry* Entry = Cache.Find(Key);
	if (Entry == nullptr)
	{
		++Stats.CacheMisses;
		return nullptr;
	}
	const FLNPNavPath& Path = *Entry->Path;
	// 요청자는 자기 시작 node에서 첫 waypoint로, 마지막 waypoint에서 자기 목표 node로 곧게 걸을 수 있어야 경로를 나눠 쓴다.
	if (!Path.IsCurrent(Nav, Overlay)
		|| !LNPNavGraph::IsDirectWalkable(Nav, StartNode, Path.Waypoints[0].Node, Overlay)
		|| !LNPNavGraph::IsDirectWalkable(Nav, Path.Waypoints.Last().Node, GoalNode, Overlay))
	{
		++Stats.CacheRejects;
		++Stats.CacheMisses;
		return nullptr;
	}
	Entry->LastUsedTick = TickIndex;
	++Stats.CacheHits;
	return Entry->Path;
}

void FLNPNavPathScheduler::AddToCache(const FCacheKey& Key, const FLNPNavPathPtr& Path)
{
	if (Settings.CacheCapacity <= 0)
	{
		return;
	}
	if (!Cache.Contains(Key))
	{
		while (Cache.Num() >= Settings.CacheCapacity)
		{
			// LRU. 용량이 수백 개라 선형 탐색으로 충분하다.
			const FCacheKey* Oldest = nullptr;
			uint64 OldestTick = MAX_uint64;
			for (const TPair<FCacheKey, FCacheEntry>& Pair : Cache)
			{
				if (Pair.Value.LastUsedTick < OldestTick)
				{
					Oldest = &Pair.Key;
					OldestTick = Pair.Value.LastUsedTick;
				}
			}
			Cache.Remove(FCacheKey(*Oldest));
		}
	}
	Cache.Add(Key, {Path, TickIndex});
}
