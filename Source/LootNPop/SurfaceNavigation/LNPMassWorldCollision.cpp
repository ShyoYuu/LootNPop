// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "SurfaceNavigation/LNPCollisionChannels.h"
#include "LootNPop.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	int32 GWorldCollisionDebugDraw = 0;
	FAutoConsoleVariableRef CVarWorldCollisionDebugDraw(
		TEXT("LNP.SurfaceNav.WorldCollision.DebugDraw"), GWorldCollisionDebugDraw,
		TEXT("1 = queue every MassWorldCollision query for debug drawing on the game thread (green = known hit, red = unknown hit, grey = miss)."));

	int32 GWorldCollisionLockProbe = 0;
	FAutoConsoleVariableRef CVarWorldCollisionLockProbe(
		TEXT("LNP.SurfaceNav.WorldCollision.LockProbe"), GWorldCollisionLockProbe,
		TEXT("1 = time a scene read-lock acquire right before each MassWorldCollision query to estimate lock wait. Adds one lock per query."));

	/** 큐가 게임 스레드 draw보다 빨리 쌓이는 경우의 프레임당 draw 상한. */
	constexpr int32 MaxDebugSegmentsPerTick = 4096;

	const TCHAR* QueryClassNames[] =
	{
		TEXT("ProjectileMandatory"),
		TEXT("AirborneMandatory"),
		TEXT("GroundRiskFallback"),
		TEXT("DynamicSupportContact"),
		TEXT("PeriodicGroundValidation"),
		TEXT("DebugValidation"),
	};
	static_assert(UE_ARRAY_COUNT(QueryClassNames) == static_cast<int32>(ELNPWorldQueryClass::Count));

	uint64 WorldCollisionCyclesToNs(const uint64 Cycles)
	{
		return static_cast<uint64>(FPlatformTime::GetSecondsPerCycle64() * 1e9 * static_cast<double>(Cycles));
	}

	void AtomicMax(std::atomic<uint64>& Target, const uint64 Value)
	{
		uint64 Current = Target.load(std::memory_order_relaxed);
		while (Value > Current && !Target.compare_exchange_weak(Current, Value, std::memory_order_relaxed))
		{
		}
	}
}

bool ULNPMassWorldCollisionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULNPMassWorldCollisionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	HitIdentity = Collection.InitializeDependency<ULNPHitIdentitySubsystem>();
}

TStatId ULNPMassWorldCollisionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPMassWorldCollisionSubsystem, STATGROUP_Tickables);
}

bool ULNPMassWorldCollisionSubsystem::RaycastWorld(const FVector& Start, const FVector& End, const FLNPWorldQueryParams& Params,
	FLNPWorldHit& OutHit) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LNPWorldCollision_Raycast);
	return RunQuery(Start, End, FQuat::Identity, FCollisionShape(), Params, OutHit);
}

bool ULNPMassWorldCollisionSubsystem::SweepSphereWorld(const FVector& Start, const FVector& End, const float Radius,
	const FLNPWorldQueryParams& Params, FLNPWorldHit& OutHit) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LNPWorldCollision_SweepSphere);
	return RunQuery(Start, End, FQuat::Identity, FCollisionShape::MakeSphere(Radius), Params, OutHit);
}

bool ULNPMassWorldCollisionSubsystem::SweepCapsuleWorld(const FVector& Start, const FVector& End, const FQuat& Rotation,
	const float Radius, const float HalfHeight, const FLNPWorldQueryParams& Params, FLNPWorldHit& OutHit) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LNPWorldCollision_SweepCapsule);
	return RunQuery(Start, End, Rotation, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params, OutHit);
}

bool ULNPMassWorldCollisionSubsystem::ProbeSupport(const FLNPSupportProbeQuery& Query, const FLNPWorldQueryParams& Params,
	FLNPSupportProbeResult& OutResult) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LNPWorldCollision_ProbeSupport);
	OutResult = FLNPSupportProbeResult();

	const FVector Up = Query.Up.GetSafeNormal();
	const FVector Start = Query.Position + Up * Query.MaxStepUp;
	const FVector End = Query.Position - Up * Query.MaxDrop;
	if (!RunQuery(Start, End, FQuat::Identity, FCollisionShape::MakeSphere(Query.Radius), Params, OutResult.Hit))
		return false;

	// 내부형 구·동굴 천장·동적 패널 모두 호출자가 준 Up 기준으로만 판정한다. 월드 Z를 쓰지 않는다.
	OutResult.bWalkableNormal = FVector::DotProduct(OutResult.Hit.ImpactNormal, Up) >= Query.WalkableMinDot;
	OutResult.bSupported = OutResult.bWalkableNormal
		&& !OutResult.Hit.bStartPenetrating
		&& (OutResult.Hit.Identity.Roles & ELNPExactSourceRole::Support) != 0;
	return OutResult.bSupported;
}

bool ULNPMassWorldCollisionSubsystem::RunQuery(const FVector& Start, const FVector& End, const FQuat& Rotation,
	const FCollisionShape& Shape, const FLNPWorldQueryParams& Params, FLNPWorldHit& OutHit) const
{
	OutHit = FLNPWorldHit();

	UWorld* World = GetWorld();
	if (World == nullptr || HitIdentity == nullptr)
		return false;

	FClassStats& ClassStats = Stats[static_cast<int32>(Params.Class)];

	if (GWorldCollisionLockProbe != 0)
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				const uint64 LockStart = FPlatformTime::Cycles64();
				Solver->GetExternalDataLock_External().ReadLock();
				Solver->GetExternalDataLock_External().ReadUnlock();
				ClassStats.LockNs.fetch_add(WorldCollisionCyclesToNs(FPlatformTime::Cycles64() - LockStart), std::memory_order_relaxed);
			}
		}
	}

	const uint64 QueryStart = FPlatformTime::Cycles64();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LNPWorldCollision), /*bTraceComplex=*/false);
	QueryParams.bReturnFaceIndex = true;
	for (const uint32 ActorId : Params.IgnoredActorIds)
	{
		QueryParams.AddIgnoredActor(ActorId);
	}

	FHitResult Hit;
	const bool bHit = Shape.IsLine()
		? World->LineTraceSingleByChannel(Hit, Start, End, LNPCollisionChannels::WorldExact, QueryParams)
		: World->SweepSingleByChannel(Hit, Start, End, Rotation, LNPCollisionChannels::WorldExact, Shape, QueryParams);

	if (bHit)
	{
		OutHit.bBlockingHit = true;
		OutHit.bStartPenetrating = Hit.bStartPenetrating;
		OutHit.Time = Hit.Time;
		OutHit.Distance = Hit.Distance;
		OutHit.Location = Hit.Location;
		OutHit.ImpactPoint = Hit.ImpactPoint;
		OutHit.ImpactNormal = Hit.ImpactNormal;
		// snapshot 정적 함수로 해석하고 미해석은 이 서브시스템 counter로만 센다.
		OutHit.Identity = ULNPHitIdentitySubsystem::ResolveHit(*HitIdentity->GetSnapshot(), Hit);
		if (!OutHit.Identity.IsKnown())
		{
			UnknownHits.fetch_add(1, std::memory_order_relaxed);
		}
	}
	else
	{
		OutHit.Location = End;
	}

	const uint64 QueryNs = WorldCollisionCyclesToNs(FPlatformTime::Cycles64() - QueryStart);
	ClassStats.Count.fetch_add(1, std::memory_order_relaxed);
	ClassStats.QueryNs.fetch_add(QueryNs, std::memory_order_relaxed);
	AtomicMax(ClassStats.MaxQueryNs, QueryNs);
	if (bHit)
	{
		ClassStats.Hits.fetch_add(1, std::memory_order_relaxed);
	}

	if (GWorldCollisionDebugDraw != 0)
	{
		DebugQueue.Enqueue(FDebugSegment{ Start, End, OutHit.ImpactPoint, bHit, OutHit.Identity.IsKnown() });
	}
	return bHit;
}

void ULNPMassWorldCollisionSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	FDebugSegment Segment;
	for (int32 Drawn = 0; Drawn < MaxDebugSegmentsPerTick && DebugQueue.Dequeue(Segment); ++Drawn)
	{
		if (!Segment.bHit)
		{
			DrawDebugLine(World, Segment.Start, Segment.End, FColor(128, 128, 128), false, 0.f);
			continue;
		}
		const FColor Color = Segment.bKnown ? FColor::Green : FColor::Red;
		DrawDebugLine(World, Segment.Start, Segment.ImpactPoint, Color, false, 0.f);
		DrawDebugPoint(World, Segment.ImpactPoint, 6.f, Color, false, 0.f);
	}
	// 상한을 넘은 나머지는 버린다. 다음 프레임에 옛 선을 그리지 않는다.
	DebugQueue.Empty();
}

void ULNPMassWorldCollisionSubsystem::ResetStats()
{
	for (FClassStats& ClassStats : Stats)
	{
		ClassStats.Count = 0;
		ClassStats.Hits = 0;
		ClassStats.QueryNs = 0;
		ClassStats.MaxQueryNs = 0;
		ClassStats.LockNs = 0;
	}
	UnknownHits = 0;
}

void ULNPMassWorldCollisionSubsystem::Report() const
{
	const UWorld* World = GetWorld();
	UE_LOG(LogLootNPop, Display, TEXT("[WorldCollision] ===== %s NetMode=%d LockProbe=%d ====="),
		*World->GetName(), static_cast<int32>(World->GetNetMode()), GWorldCollisionLockProbe);

	uint64 GroupCount[2] = {};
	uint64 GroupNs[2] = {};
	for (int32 Index = 0; Index < static_cast<int32>(ELNPWorldQueryClass::Count); ++Index)
	{
		const FClassStats& ClassStats = Stats[Index];
		const uint64 Count = ClassStats.Count.load(std::memory_order_relaxed);
		const uint64 TotalNs = ClassStats.QueryNs.load(std::memory_order_relaxed);
		const int32 Group = LNPWorldQuery::IsMandatory(static_cast<ELNPWorldQueryClass>(Index)) ? 0 : 1;
		GroupCount[Group] += Count;
		GroupNs[Group] += TotalNs;
		if (Count == 0)
			continue;

		UE_LOG(LogLootNPop, Display, TEXT("[WorldCollision] %-24s count=%llu hits=%llu avg=%.2fus max=%.2fus lock=%.3fms"),
			QueryClassNames[Index], Count, ClassStats.Hits.load(std::memory_order_relaxed),
			TotalNs / 1000.0 / Count, ClassStats.MaxQueryNs.load(std::memory_order_relaxed) / 1000.0,
			ClassStats.LockNs.load(std::memory_order_relaxed) / 1e6);
	}
	UE_LOG(LogLootNPop, Display, TEXT("[WorldCollision] Mandatory count=%llu total=%.3fms | Optional count=%llu total=%.3fms | UnknownHits=%llu"),
		GroupCount[0], GroupNs[0] / 1e6, GroupCount[1], GroupNs[1] / 1e6, UnknownHits.load(std::memory_order_relaxed));
}

namespace
{
	void ForEachWorldCollision(TFunctionRef<void(ULNPMassWorldCollisionSubsystem&)> Func)
	{
		if (GEngine == nullptr)
			return;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World())
			{
				if (ULNPMassWorldCollisionSubsystem* Subsystem = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>())
				{
					Func(*Subsystem);
				}
			}
		}
	}

	FAutoConsoleCommand GLNPWorldCollisionReport(
		TEXT("LNP.SurfaceNav.WorldCollision.Report"),
		TEXT("Log MassWorldCollision per-class query counts, average/max time, lock probe totals and unknown hits for every game world."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			ForEachWorldCollision([](ULNPMassWorldCollisionSubsystem& Subsystem) { Subsystem.Report(); });
		}));

	FAutoConsoleCommand GLNPWorldCollisionReset(
		TEXT("LNP.SurfaceNav.WorldCollision.Reset"),
		TEXT("Clear MassWorldCollision counters for every game world."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			ForEachWorldCollision([](ULNPMassWorldCollisionSubsystem& Subsystem) { Subsystem.ResetStats(); });
		}));
}
