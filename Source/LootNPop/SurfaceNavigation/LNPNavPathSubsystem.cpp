// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavPathSubsystem.h"

#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MassEntitySubsystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	FLNPNavPathSchedulerSettings GNavPathDefaults;

	int32 GNavExpansionsPerFrame = GNavPathDefaults.ExpansionsPerFrame;
	FAutoConsoleVariableRef CVarNavExpansionsPerFrame(
		TEXT("LNP.SurfaceNav.NavExpansionsPerFrame"), GNavExpansionsPerFrame,
		TEXT("Server A* node expansion budget per frame across all path requests (about 0.38us per expansion)."));

	int32 GNavMaxExpansionsPerRequest = GNavPathDefaults.MaxExpansionsPerRequest;
	FAutoConsoleVariableRef CVarNavMaxExpansionsPerRequest(
		TEXT("LNP.SurfaceNav.NavMaxExpansionsPerRequest"), GNavMaxExpansionsPerRequest,
		TEXT("Cumulative A* expansions after which a single path request ends as NoPath."));

	int32 GNavScratchCount = GNavPathDefaults.ScratchCount;
	FAutoConsoleVariableRef CVarNavScratchCount(
		TEXT("LNP.SurfaceNav.NavScratchCount"), GNavScratchCount,
		TEXT("Concurrent path requests. Each scratch holds g/parent/stamp arrays for every Nav node (about 6.2MiB on Meadow)."));

	int32 GNavPathCacheSize = GNavPathDefaults.CacheCapacity;
	FAutoConsoleVariableRef CVarNavPathCacheSize(
		TEXT("LNP.SurfaceNav.NavPathCacheSize"), GNavPathCacheSize,
		TEXT("LRU capacity of the (start tile, goal tile) path cache. 0 disables caching."));

	int32 GNavParallelSearch = GNavPathDefaults.bParallel ? 1 : 0;
	FAutoConsoleVariableRef CVarNavParallelSearch(
		TEXT("LNP.SurfaceNav.NavParallelSearch"), GNavParallelSearch,
		TEXT("1 = expand running path requests on worker threads in parallel. Results do not depend on this."));
}

bool ULNPNavPathSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULNPNavPathSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SurfaceData = Collection.InitializeDependency<ULNPSurfaceDataSubsystem>();
	EntitySubsystem = Collection.InitializeDependency<UMassEntitySubsystem>();
}

TStatId ULNPNavPathSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPNavPathSubsystem, STATGROUP_Tickables);
}

void ULNPNavPathSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	TRACE_CPUPROFILER_EVENT_SCOPE(LNPNavPathSubsystem_Tick);
	const UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client || SurfaceData == nullptr || EntitySubsystem == nullptr)
	{
		return;
	}
	const TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot = SurfaceData->TakeSnapshot();
	if (!Snapshot.IsValid() || !Snapshot->Nav.Graph.IsValid())
	{
		return;
	}

	FLNPNavPathSchedulerSettings& Settings = Scheduler.Settings;
	Settings.ExpansionsPerFrame = FMath::Max(1, GNavExpansionsPerFrame);
	Settings.MaxExpansionsPerRequest = FMath::Max(1, GNavMaxExpansionsPerRequest);
	Settings.ScratchCount = FMath::Max(1, GNavScratchCount);
	Settings.CacheCapacity = FMath::Max(0, GNavPathCacheSize);
	Settings.bParallel = GNavParallelSearch != 0;

	const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();
	const double StartSeconds = FPlatformTime::Seconds();
	// Pod runtime overlay는 구현 단위 2에서 게시한다. 그 전에는 revision 0이다.
	Scheduler.Tick(Snapshot->Nav, nullptr, [&EntityManager](const FMassEntityHandle Owner)
	{
		return EntityManager.IsEntityValid(Owner);
	});
	LastTickSeconds = FPlatformTime::Seconds() - StartSeconds;
}
