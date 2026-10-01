// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPNavPathSubsystem.h"

#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "LootNPop.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MassEntitySubsystem.h"
#include "Misc/FileHelper.h"
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

uint32 ULNPNavPathSubsystem::Submit(const FLNPNavPathRequest& Request)
{
	const uint32 Serial = Scheduler.Submit(Request);
	if (bCaptureRequests)
	{
		const auto Snapshot = SurfaceData->TakeSnapshot();
		RequestCapture += FString::Printf(TEXT("%.6f,%d,%d,%u,%d,%.6f,%.6f,%.6f,%u,%u,%llu,%.6f,%.6f,%.6f,%u,%u,%llu,%.3f,%.3f,%u,%u\n"),
			GetWorld()->GetTimeSeconds() - RequestCaptureStart, Request.Owner.Index, Request.Owner.SerialNumber, Serial,
			static_cast<int32>(Request.Priority), Request.StartPosition.X, Request.StartPosition.Y, Request.StartPosition.Z,
			Request.StartSurface.OctantSlot, Request.StartSurface.LocalLayerId, Request.StartSurface.Generation,
			Request.GoalPosition.X, Request.GoalPosition.Y, Request.GoalPosition.Z,
			Request.GoalSurface.OctantSlot, Request.GoalSurface.LocalLayerId, Request.GoalSurface.Generation,
			Request.SnapRadius, Request.ApproachRadius, Snapshot.IsValid() ? Snapshot->Nav.ConnectivityGraphVersion : 0,
			Overlay.IsValid() ? Overlay->Revision : 0);
	}
	return Serial;
}

void ULNPNavPathSubsystem::BeginRequestCapture()
{
	RequestCaptureStart = GetWorld()->GetTimeSeconds();
	RequestCapture = TEXT("time,owner,ownerSerial,requestSerial,priority,startX,startY,startZ,startSlot,startLayer,startGeneration,goalX,goalY,goalZ,goalSlot,goalLayer,goalGeneration,snapRadius,approachRadius,graphVersion,overlayRevision\n");
	bCaptureRequests = true;
}

bool ULNPNavPathSubsystem::EndRequestCapture(const FString& Filename)
{
	bCaptureRequests = false;
	const bool bSaved = FFileHelper::SaveStringToFile(RequestCapture, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	RequestCapture.Empty();
	return bSaved;
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

void ULNPNavPathSubsystem::AddPodBlocker(const int32 PodID, const FVector& Location,
	const FLNPSurfaceHandle& Surface)
{
	if (PodID > 0 && Surface.IsValid())
	{
		PodBlockers.Add(PodID, {PodID, FVector3d(Location), Surface});
	}
}

void ULNPNavPathSubsystem::RemovePodBlocker(const int32 PodID)
{
	if (PodBlockers.Remove(PodID) > 0)
	{
		CommitPodBlockers();
	}
}

void ULNPNavPathSubsystem::CommitPodBlockers()
{
	if (SurfaceData == nullptr)
	{
		return;
	}
	const TSharedPtr<const FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe> Snapshot = SurfaceData->TakeSnapshot();
	if (!Snapshot.IsValid() || !Snapshot->Nav.Graph.IsValid())
	{
		return;
	}
	if (OverlayGeneration != Snapshot->Generation)
	{
		Overlay.Reset();
		OverlayGeneration = Snapshot->Generation;
	}
	TArray<FLNPNavPodBlocker> Pods;
	PodBlockers.GenerateValueArray(Pods);
	FLNPNavOverlay Next;
	if (LNPNavOverlay::BuildPodOverlay(*Snapshot, Pods, Overlay.Get(), Next))
	{
		Overlay = MakeShared<FLNPNavOverlay, ESPMode::ThreadSafe>(MoveTemp(Next));
		const int32 Requeued = Scheduler.RequeueInvalidatedPaths(Snapshot->Nav, Overlay.Get());
		UE_LOG(LogLootNPop, Log, TEXT("[NavOverlay] Published Pod blockers: pods=%d revision=%u blockedNodes=%d"),
			Pods.Num(), Overlay->Revision, Overlay->BlockedNodes.CountSetBits());
		if (Requeued > 0)
		{
			UE_LOG(LogLootNPop, Log, TEXT("[NavOverlay] Requeued %d invalidated paths."), Requeued);
		}
	}
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
	if (OverlayGeneration != Snapshot->Generation)
	{
		CommitPodBlockers();
	}
	const TSharedPtr<const FLNPNavOverlay, ESPMode::ThreadSafe> CurrentOverlay = Overlay;
	Scheduler.Tick(Snapshot->Nav, CurrentOverlay.Get(), [&EntityManager](const FMassEntityHandle Owner)
	{
		return EntityManager.IsEntityValid(Owner);
	});
	LastTickSeconds = FPlatformTime::Seconds() - StartSeconds;
}
