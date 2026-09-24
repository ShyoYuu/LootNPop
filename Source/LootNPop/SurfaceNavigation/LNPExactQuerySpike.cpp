// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPExactQuerySpike.h"
#include "SurfaceNavigation/LNPCollisionChannels.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "Config/LNPSettings.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "LootNPop.h"

#include "Async/ParallelFor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MassExecutionContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	int32 GQueriesPerFrame = 0;
	FAutoConsoleVariableRef CVarQueriesPerFrame(
		TEXT("LNP.SurfaceNav.ExactSpike.QueriesPerFrame"), GQueriesPerFrame,
		TEXT("Gate 0 spike: LNPWorldExact sync queries per frame issued from a Mass worker processor (0 = off, max 3072)."));

	int32 GMovingBodies = 0;
	FAutoConsoleVariableRef CVarMovingBodies(
		TEXT("LNP.SurfaceNav.ExactSpike.MovingBodies"), GMovingBodies,
		TEXT("Gate 0 spike: number of kinematic bodies moved on the game thread in TG_PrePhysics (0 = none, max 256)."));

	int32 GParallel = 1;
	FAutoConsoleVariableRef CVarParallel(
		TEXT("LNP.SurfaceNav.ExactSpike.Parallel"), GParallel,
		TEXT("Gate 0 spike: 1 = spread queries over worker threads with ParallelFor, 0 = run them on the processor's thread."));

	int32 GLockProbe = 1;
	FAutoConsoleVariableRef CVarLockProbe(
		TEXT("LNP.SurfaceNav.ExactSpike.LockProbe"), GLockProbe,
		TEXT("Gate 0 spike: 1 = time a scene read-lock acquire right before each query to estimate lock wait."));

	int32 GAutoCapture = 0;
	FAutoConsoleVariableRef CVarAutoCapture(
		TEXT("LNP.SurfaceNav.ExactSpike.AutoCapture"), GAutoCapture,
		TEXT("Gate 0 spike: 1 = once the world is ready, warm up 10 s, reset stats, capture 30 s, then Report once. For unattended -game runs."));

	constexpr double AutoCaptureWarmupSeconds = 10.0;
	constexpr double AutoCaptureSeconds = 30.0;

	constexpr int32 MaxMovingBodies = 256;
	constexpr int32 QueryChunkSize = 64;

	/** 질의 선분: 기준 반지름 안쪽 3000cm에서 바깥쪽 1000cm까지. 내부형 구라 안쪽이 위다. */
	constexpr float QueryInnerOffset = 3000.f;
	constexpr float QueryOuterOffset = 1000.f;

	constexpr float SphereQueryRadius = 50.f;
	constexpr float CapsuleQueryRadius = 40.f;
	constexpr float CapsuleQueryHalfHeight = 90.f;

	/** 한 통계 배열의 표본 상한. 3072 q/frame × 60fps × 40s 정도를 담는다. */
	constexpr int32 MaxSamples = 8 * 1024 * 1024;

	const TCHAR* QueryTypeNames[LNPExactQuerySpike::QueryTypeCount] = { TEXT("Line"), TEXT("Sphere"), TEXT("Capsule") };

	uint32 CyclesToNs(const uint64 Cycles)
	{
		return static_cast<uint32>(FMath::Min<double>(FPlatformTime::GetSecondsPerCycle64() * 1e9 * static_cast<double>(Cycles), MAX_uint32));
	}

	/** 정렬 뒤 백분위를 µs로 적는다. */
	FString FormatPercentiles(TArray<uint32> Samples)
	{
		if (Samples.IsEmpty())
		{
			return TEXT("n=0");
		}
		Samples.Sort();
		auto At = [&Samples](const double P)
		{
			const int32 Index = FMath::Clamp(FMath::CeilToInt32(P * Samples.Num()) - 1, 0, Samples.Num() - 1);
			return Samples[Index] / 1000.0;
		};
		return FString::Printf(TEXT("n=%d P50=%.2fus P95=%.2fus P99=%.2fus Max=%.2fus"),
			Samples.Num(), At(0.50), At(0.95), At(0.99), Samples.Last() / 1000.0);
	}

	void AppendCapped(TArray<uint32>& Target, const uint32 Value)
	{
		if (Target.Num() < MaxSamples)
		{
			Target.Add(Value);
		}
	}
}

// --- ALNPExactSpikeMover ---

ALNPExactSpikeMover::ALNPExactSpikeMover()
{
	PrimaryActorTick.bCanEverTick = true;
	// Mass PrePhysics phase와 같은 tick group에서 움직여 worker 읽기 락과 게임 스레드 쓰기 락이 겹치게 한다.
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ALNPExactSpikeMover::Setup(const TArray<FVector>& InDirections)
{
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	ULNPHitIdentitySubsystem* HitIdentity = GetWorld()->GetSubsystem<ULNPHitIdentitySubsystem>();
	const float SphereRadius = GetDefault<ULNPSettings>()->SphereRadius;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LNPExactSpikeMoverSetup), /*bTraceComplex=*/false);
	for (const FVector& Direction : InDirections)
	{
		FHitResult Hit;
		const FVector Start = Direction * (SphereRadius - QueryInnerOffset);
		const FVector End = Direction * (SphereRadius + QueryOuterOffset);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, LNPCollisionChannels::WorldExact, Params))
			continue;

		UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(this);
		Body->SetStaticMesh(CubeMesh);
		Body->SetMobility(EComponentMobility::Movable);
		Body->SetCollisionProfileName(TEXT("LNPDynamicTerrain"));
		Body->SetWorldScale3D(FVector(3.f));
		// 300cm 정육면체의 중심을 지면에서 300cm 위(중심 방향)에 둔다. 진동 ±100cm에도 지면과 닿지 않는다.
		const float BaseRadius = Hit.ImpactPoint.Size() - 300.f;
		Body->SetWorldLocationAndRotation(Direction * BaseRadius, FRotationMatrix::MakeFromZ(-Direction).ToQuat());
		Body->RegisterComponent();

		Bodies.Add(Body);
		Directions.Add(Direction);
		BaseRadii.Add(BaseRadius);

		if (HitIdentity != nullptr)
		{
			HitIdentity->RegisterRuntimeSource(Body);
		}
	}
}

void ALNPExactSpikeMover::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const double Time = GetWorld()->GetTimeSeconds();
	for (int32 Index = 0; Index < Bodies.Num(); ++Index)
	{
		const float Offset = 100.f * FMath::Sin(Time * UE_TWO_PI * 0.5 + Index);
		Bodies[Index]->SetWorldLocation(Directions[Index] * (BaseRadii[Index] + Offset), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ALNPExactSpikeMover::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULNPHitIdentitySubsystem* HitIdentity = GetWorld()->GetSubsystem<ULNPHitIdentitySubsystem>())
	{
		for (UStaticMeshComponent* Body : Bodies)
		{
			HitIdentity->UnregisterRuntimeSource(Body);
		}
	}
	Super::EndPlay(EndPlayReason);
}

// --- ULNPExactQuerySpikeSubsystem ---

bool ULNPExactQuerySpikeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
#endif
}

void ULNPExactQuerySpikeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Results.SetNum(LNPExactQuerySpike::SetSize);
	SphereRadius = GetDefault<ULNPSettings>()->SphereRadius;
}

void ULNPExactQuerySpikeSubsystem::Deinitialize()
{
	// 월드 해체 중이라 Actor를 Destroy하지 않는다. body 등록 해제는 Mover의 EndPlay가 한다.
	Mover = nullptr;
	SpikeBodies = MakeShared<FBodySet, ESPMode::ThreadSafe>();
	Super::Deinitialize();
}

FVector ULNPExactQuerySpikeSubsystem::GetDirection(const int32 Index)
{
	// Fibonacci sphere. 머신·빌드와 무관하게 같은 방향을 만든다.
	const double Golden = UE_PI * (3.0 - FMath::Sqrt(5.0));
	const double Z = 1.0 - (2.0 * Index + 1.0) / LNPExactQuerySpike::SetSize;
	const double RadiusXY = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
	const double Theta = Golden * Index;
	return FVector(FMath::Cos(Theta) * RadiusXY, FMath::Sin(Theta) * RadiusXY, Z);
}

int32 ULNPExactQuerySpikeSubsystem::AdvanceCursor(const int32 Count)
{
	const int32 Start = Cursor;
	Cursor = (Cursor + Count) % LNPExactQuerySpike::SetSize;
	return Start;
}

bool ULNPExactQuerySpikeSubsystem::IsSpikeBody(const TWeakObjectPtr<UPrimitiveComponent>& Component) const
{
	return SpikeBodies->Contains(Component);
}

void ULNPExactQuerySpikeSubsystem::RebuildMovers(const int32 Count)
{
	if (Mover != nullptr)
	{
		Mover->Destroy();
		Mover = nullptr;
	}

	FBodySet NewBodies;
	if (Count > 0)
	{
		// 질의 방향 중 균등 간격으로 골라 그 질의가 body에 맞게 한다.
		TArray<FVector> Directions;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Directions.Add(GetDirection(Index * LNPExactQuerySpike::SetSize / Count));
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		Mover = GetWorld()->SpawnActor<ALNPExactSpikeMover>(SpawnParams);
		Mover->Setup(Directions);
		for (UStaticMeshComponent* Body : Mover->GetBodies())
		{
			NewBodies.Add(Body);
		}
	}

	MoverCount = Count;
	SpikeBodies = MakeShared<FBodySet, ESPMode::ThreadSafe>(MoveTemp(NewBodies));

	UE_LOG(LogLootNPop, Display, TEXT("[ExactSpike] %s: %d moving bodies (%d requested)."),
		*GetWorld()->GetName(), SpikeBodies->Num(), Count);
}

void ULNPExactQuerySpikeSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	// tickable 구간은 Mass phase 밖이다(ULNPHitIdentitySubsystem 주석). worker가 읽는 값은 여기서만 바꾼다.
	const ULNPOctantSpawnSubsystem* OctantSubsystem = GetWorld()->GetSubsystem<ULNPOctantSpawnSubsystem>();
	bReady = OctantSubsystem != nullptr && OctantSubsystem->bGenerationComplete;

	const int32 DesiredMovers = bReady ? FMath::Clamp(GMovingBodies, 0, MaxMovingBodies) : 0;
	if (DesiredMovers != MoverCount)
	{
		RebuildMovers(DesiredMovers);
	}

	// 무인 측정: 준비 시각부터 warm-up 뒤 Reset, capture 뒤 Report를 한 번씩 한다.
	if (bReady && GAutoCapture != 0 && GQueriesPerFrame > 0 && AutoCaptureStage < 2)
	{
		const double Now = GetWorld()->GetRealTimeSeconds();
		if (AutoCaptureReadyTime < 0.0)
		{
			AutoCaptureReadyTime = Now;
		}
		if (AutoCaptureStage == 0 && Now - AutoCaptureReadyTime >= AutoCaptureWarmupSeconds)
		{
			ResetStats();
			AutoCaptureStage = 1;
		}
		else if (AutoCaptureStage == 1 && Now - AutoCaptureReadyTime >= AutoCaptureWarmupSeconds + AutoCaptureSeconds)
		{
			Report();
			AutoCaptureStage = 2;
		}
	}
}

TStatId ULNPExactQuerySpikeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPExactQuerySpikeSubsystem, STATGROUP_Tickables);
}

void ULNPExactQuerySpikeSubsystem::RecordFrame(FLNPExactSpikeFrame&& Frame)
{
	FScopeLock Lock(&StatsLock);

	uint64 FrameQuery = 0;
	uint64 FrameLock = 0;
	for (int32 Index = 0; Index < Frame.QueryNs.Num(); ++Index)
	{
		const int32 Type = (Frame.Start + Index) % LNPExactQuerySpike::QueryTypeCount;
		++QueryCount[Type];
		AppendCapped(QueryNs[Type], Frame.QueryNs[Index]);
		FrameQuery += Frame.QueryNs[Index];
	}
	for (const uint32 Ns : Frame.LockNs)
	{
		AppendCapped(LockNs, Ns);
		FrameLock += Ns;
	}
	for (int32 Type = 0; Type < LNPExactQuerySpike::QueryTypeCount; ++Type)
	{
		HitCount[Type] += Frame.Hits[Type];
	}

	AppendCapped(FrameQueryNs, static_cast<uint32>(FMath::Min<uint64>(FrameQuery, MAX_uint32)));
	AppendCapped(FrameLockNs, static_cast<uint32>(FMath::Min<uint64>(FrameLock, MAX_uint32)));
	AppendCapped(FrameWallNs, static_cast<uint32>(FMath::Min<uint64>(Frame.WallNs, MAX_uint32)));

	UnknownHits += Frame.UnknownHits;
	DynamicHits += Frame.DynamicHits;
	ClassificationErrors += Frame.ClassificationErrors;
	++(Frame.bOnGameThread ? GameThreadFrames : WorkerFrames);
}

void ULNPExactQuerySpikeSubsystem::ResetStats()
{
	FScopeLock Lock(&StatsLock);
	for (int32 Type = 0; Type < LNPExactQuerySpike::QueryTypeCount; ++Type)
	{
		QueryNs[Type].Reset();
		QueryCount[Type] = 0;
		HitCount[Type] = 0;
	}
	LockNs.Reset();
	FrameQueryNs.Reset();
	FrameLockNs.Reset();
	FrameWallNs.Reset();
	UnknownHits = DynamicHits = ClassificationErrors = WorkerFrames = GameThreadFrames = 0;
}

void ULNPExactQuerySpikeSubsystem::Report() const
{
	FScopeLock Lock(&StatsLock);

	const UWorld* World = GetWorld();
	UE_LOG(LogLootNPop, Display, TEXT("[ExactSpike] ===== %s NetMode=%d QueriesPerFrame=%d MovingBodies=%d Parallel=%d LockProbe=%d ====="),
		*World->GetName(), static_cast<int32>(World->GetNetMode()), GQueriesPerFrame, SpikeBodies->Num(), GParallel, GLockProbe);
	UE_LOG(LogLootNPop, Display, TEXT("[ExactSpike] Frames: worker=%llu gamethread=%llu"), WorkerFrames, GameThreadFrames);

	for (int32 Type = 0; Type < LNPExactQuerySpike::QueryTypeCount; ++Type)
	{
		UE_LOG(LogLootNPop, Display, TEXT("[ExactSpike] %-7s count=%llu hits=%llu per-query %s"),
			QueryTypeNames[Type], QueryCount[Type], HitCount[Type], *FormatPercentiles(QueryNs[Type]));
	}
	UE_LOG(LogLootNPop, Display, TEXT("[ExactSpike] LockWait per-query %s"), *FormatPercentiles(LockNs));
	UE_LOG(LogLootNPop, Display, TEXT("[ExactSpike] Frame query sum   %s"), *FormatPercentiles(FrameQueryNs));
	UE_LOG(LogLootNPop, Display, TEXT("[ExactSpike] Frame lock sum    %s"), *FormatPercentiles(FrameLockNs));
	UE_LOG(LogLootNPop, Display, TEXT("[ExactSpike] Frame wall        %s"), *FormatPercentiles(FrameWallNs));
	UE_LOG(LogLootNPop, Display, TEXT("[ExactSpike] UnknownHits=%llu DynamicHits=%llu ClassificationErrors=%llu"),
		UnknownHits, DynamicHits, ClassificationErrors);

	// 결과 표 해시: 동적 body hit는 시간에 따라 달라지므로 제외한다.
	uint32 Hash[LNPExactQuerySpike::QueryTypeCount] = {};
	int32 Filled[LNPExactQuerySpike::QueryTypeCount] = {};
	int32 KindCounts[5] = {};
	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		const FLNPExactSpikeResult& Result = Results[Index];
		++KindCounts[static_cast<uint8>(Result.Kind)];
		if (Result.Kind == FLNPExactSpikeResult::EKind::Unset || Result.Kind == FLNPExactSpikeResult::EKind::Dynamic)
			continue;

		const int32 Type = Index % LNPExactQuerySpike::QueryTypeCount;
		++Filled[Type];
		uint32 EntryHash = HashCombineFast(GetTypeHash(Index), GetTypeHash(static_cast<uint8>(Result.Kind)));
		EntryHash = HashCombineFast(EntryHash, GetTypeHash(Result.Slot));
		EntryHash = HashCombineFast(EntryHash, GetTypeHash(Result.Lifetime));
		EntryHash = HashCombineFast(EntryHash, GetTypeHash(Result.DistanceCm));
		EntryHash = HashCombineFast(EntryHash, GetTypeHash(Result.FaceIndex));
		EntryHash = HashCombineFast(EntryHash, GetTypeHash(Result.InstanceIndex));
		// 순서 무관 합산. 채워지는 순서가 달라도 같은 표면 같은 값이다.
		Hash[Type] += EntryHash;
	}
	UE_LOG(LogLootNPop, Display, TEXT("[ExactSpike] Table unset=%d miss=%d known=%d dynamic=%d unknown=%d"),
		KindCounts[0], KindCounts[1], KindCounts[2], KindCounts[3], KindCounts[4]);
	for (int32 Type = 0; Type < LNPExactQuerySpike::QueryTypeCount; ++Type)
	{
		UE_LOG(LogLootNPop, Display, TEXT("[ExactSpike] Table %-7s filled=%d hash=%08X"), QueryTypeNames[Type], Filled[Type], Hash[Type]);
	}

	// 머신 간 해시가 다를 때 칸 단위로 비교하기 위한 덤프.
	FString Csv = TEXT("Index,Type,Kind,Slot,Lifetime,DistanceCm,FaceIndex,InstanceIndex\n");
	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		const FLNPExactSpikeResult& Result = Results[Index];
		Csv += FString::Printf(TEXT("%d,%d,%d,%d,%d,%d,%d,%d\n"), Index, Index % LNPExactQuerySpike::QueryTypeCount,
			static_cast<int32>(Result.Kind), Result.Slot, Result.Lifetime, Result.DistanceCm, Result.FaceIndex, Result.InstanceIndex);
	}
	const FString CsvPath = FPaths::ProjectSavedDir() / FString::Printf(TEXT("ExactSpike/Table_NetMode%d.csv"), static_cast<int32>(World->GetNetMode()));
	FFileHelper::SaveStringToFile(Csv, *CsvPath);
	UE_LOG(LogLootNPop, Display, TEXT("[ExactSpike] Table written to %s"), *CsvPath);
}

namespace
{
	void ForEachSpikeSubsystem(TFunctionRef<void(ULNPExactQuerySpikeSubsystem&)> Func)
	{
		if (GEngine == nullptr)
			return;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World())
			{
				if (ULNPExactQuerySpikeSubsystem* Spike = World->GetSubsystem<ULNPExactQuerySpikeSubsystem>())
				{
					Func(*Spike);
				}
			}
		}
	}

	FAutoConsoleCommand GLNPExactSpikeReport(
		TEXT("LNP.SurfaceNav.ExactSpike.Report"),
		TEXT("Gate 0 spike: log query/lock percentiles, hit classification counters and the result table hash for every game world."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			ForEachSpikeSubsystem([](ULNPExactQuerySpikeSubsystem& Spike) { Spike.Report(); });
		}));

	FAutoConsoleCommand GLNPExactSpikeReset(
		TEXT("LNP.SurfaceNav.ExactSpike.Reset"),
		TEXT("Gate 0 spike: clear timing samples and counters (keeps the result table) for every game world. Use after warm-up."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			ForEachSpikeSubsystem([](ULNPExactQuerySpikeSubsystem& Spike) { Spike.ResetStats(); });
		}));
}

// --- ULNPExactQuerySpikeProcessor ---

ULNPExactQuerySpikeProcessor::ULNPExactQuerySpikeProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	// 적 이동이 exact를 부를 위치와 같은 phase. 게임 스레드 강제 없음 — worker 실행이 검증 대상이다.
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void ULNPExactQuerySpikeProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<ULNPHitIdentitySubsystem>(EMassFragmentAccess::ReadOnly);
}

void ULNPExactQuerySpikeProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
#if !UE_BUILD_SHIPPING
	const int32 QueryCount = FMath::Clamp(GQueriesPerFrame, 0, LNPExactQuerySpike::SetSize);
	if (QueryCount == 0)
		return;

	UWorld* World = EntityManager.GetWorld();
	ULNPExactQuerySpikeSubsystem* Spike = World ? World->GetSubsystem<ULNPExactQuerySpikeSubsystem>() : nullptr;
	const ULNPHitIdentitySubsystem* HitIdentity = World ? World->GetSubsystem<ULNPHitIdentitySubsystem>() : nullptr;
	FPhysScene* PhysScene = World ? World->GetPhysicsScene() : nullptr;
	if (Spike == nullptr || HitIdentity == nullptr || PhysScene == nullptr || !Spike->IsReady())
		return;

	TRACE_CPUPROFILER_EVENT_SCOPE(LNPExactQuerySpike);
	const uint64 WallStart = FPlatformTime::Cycles64();

	const TSharedRef<const FLNPHitIdentitySnapshot, ESPMode::ThreadSafe> Snapshot = HitIdentity->GetSnapshot();
	Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver();
	const bool bLockProbe = GLockProbe != 0 && Solver != nullptr;
	const float SphereRadius = Spike->GetSphereRadius();

	FLNPExactSpikeFrame Frame;
	Frame.Start = Spike->AdvanceCursor(QueryCount);
	Frame.bOnGameThread = IsInGameThread();
	Frame.QueryNs.SetNumZeroed(QueryCount);
	if (bLockProbe)
	{
		Frame.LockNs.SetNumZeroed(QueryCount);
	}

	struct FChunkCounters
	{
		uint32 Hits[LNPExactQuerySpike::QueryTypeCount] = {};
		uint32 Unknown = 0;
		uint32 Dynamic = 0;
		uint32 ClassificationErrors = 0;
	};
	const int32 ChunkCount = FMath::DivideAndRoundUp(QueryCount, QueryChunkSize);
	TArray<FChunkCounters> Counters;
	Counters.SetNum(ChunkCount);

	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereQueryRadius);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleQueryRadius, CapsuleQueryHalfHeight);

	ParallelFor(ChunkCount, [&](const int32 ChunkIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LNPExactQuerySpike_Chunk);
		FChunkCounters& Chunk = Counters[ChunkIndex];

		FCollisionQueryParams Params(SCENE_QUERY_STAT(LNPExactSpike), /*bTraceComplex=*/false);
		Params.bReturnFaceIndex = true;

		const int32 Begin = ChunkIndex * QueryChunkSize;
		const int32 End = FMath::Min(Begin + QueryChunkSize, QueryCount);
		for (int32 Local = Begin; Local < End; ++Local)
		{
			const int32 Index = (Frame.Start + Local) % LNPExactQuerySpike::SetSize;
			const int32 Type = Index % LNPExactQuerySpike::QueryTypeCount;
			const FVector Direction = ULNPExactQuerySpikeSubsystem::GetDirection(Index);
			const FVector Start = Direction * (SphereRadius - QueryInnerOffset);
			const FVector Finish = Direction * (SphereRadius + QueryOuterOffset);

			if (bLockProbe)
			{
				const uint64 LockStart = FPlatformTime::Cycles64();
				Solver->GetExternalDataLock_External().ReadLock();
				Solver->GetExternalDataLock_External().ReadUnlock();
				Frame.LockNs[Local] = CyclesToNs(FPlatformTime::Cycles64() - LockStart);
			}

			// wrapper 시간: 동기 query + registry 해석.
			const uint64 QueryStart = FPlatformTime::Cycles64();
			FHitResult Hit;
			bool bHit = false;
			switch (Type)
			{
			case 0:
				bHit = World->LineTraceSingleByChannel(Hit, Start, Finish, LNPCollisionChannels::WorldExact, Params);
				break;
			case 1:
				bHit = World->SweepSingleByChannel(Hit, Start, Finish, FQuat::Identity, LNPCollisionChannels::WorldExact, SphereShape, Params);
				break;
			default:
				bHit = World->SweepSingleByChannel(Hit, Start, Finish, FRotationMatrix::MakeFromZ(Direction).ToQuat(),
					LNPCollisionChannels::WorldExact, CapsuleShape, Params);
				break;
			}
			const FLNPExactHitIdentity Identity = bHit ? ULNPHitIdentitySubsystem::ResolveHit(*Snapshot, Hit) : FLNPExactHitIdentity();
			Frame.QueryNs[Local] = CyclesToNs(FPlatformTime::Cycles64() - QueryStart);

			FLNPExactSpikeResult& Result = Spike->GetResultSlot(Index);
			Result = FLNPExactSpikeResult();
			if (!bHit)
			{
				Result.Kind = FLNPExactSpikeResult::EKind::Miss;
				continue;
			}

			++Chunk.Hits[Type];
			// 정답: 스파이크 body면 Dynamic, 아니면 Dynamic이 아니어야 한다(이 맵의 다른 source는 모두 Static).
			const bool bExpectDynamic = Spike->IsSpikeBody(Hit.Component);
			const bool bResolvedDynamic = Identity.Lifetime == ELNPExactSourceLifetime::Dynamic;
			if (!Identity.IsKnown())
			{
				++Chunk.Unknown;
				Result.Kind = FLNPExactSpikeResult::EKind::Unknown;
			}
			else if (bResolvedDynamic)
			{
				++Chunk.Dynamic;
				Result.Kind = FLNPExactSpikeResult::EKind::Dynamic;
			}
			else
			{
				Result.Kind = FLNPExactSpikeResult::EKind::Known;
				Result.Slot = Identity.Slot;
				Result.Lifetime = static_cast<uint8>(Identity.Lifetime);
				Result.DistanceCm = FMath::RoundToInt32(Hit.Distance);
				Result.FaceIndex = Identity.FaceIndex;
				Result.InstanceIndex = Identity.InstanceIndex;
			}
			// 미해석은 UnknownHits로 따로 센다. 분류 오류는 해석됐지만 동적 여부가 틀린 경우다.
			if (Identity.IsKnown() && bExpectDynamic != bResolvedDynamic)
			{
				++Chunk.ClassificationErrors;
			}
		}
	}, GParallel != 0 ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);

	for (const FChunkCounters& Chunk : Counters)
	{
		for (int32 Type = 0; Type < LNPExactQuerySpike::QueryTypeCount; ++Type)
		{
			Frame.Hits[Type] += Chunk.Hits[Type];
		}
		Frame.UnknownHits += Chunk.Unknown;
		Frame.DynamicHits += Chunk.Dynamic;
		Frame.ClassificationErrors += Chunk.ClassificationErrors;
	}

	Frame.WallNs = CyclesToNs(FPlatformTime::Cycles64() - WallStart);
	Spike->RecordFrame(MoveTemp(Frame));
#endif // !UE_BUILD_SHIPPING
}
