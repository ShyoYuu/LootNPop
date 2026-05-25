// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPMassSpawnSubsystem.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "DataAsset/LNPMassSpawnConfig.h"
#include "Config/LNPSettings.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "LootNPop.h"

#include "Async/Async.h"

#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"

void ULNPMassSpawnSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RandomStream.GenerateNewSeed();
}

void ULNPMassSpawnSubsystem::BeginSpawning()
{
	UE_LOG(LogLootNPop, Log, TEXT("LNPMassSpawnSubsystem: World ready. Loading config from LNPSettings."));

	if (const ULNPSettings* Settings = GetDefault<ULNPSettings>())
	{
		if (ULNPMassSpawnConfig* Config = Settings->MassSpawnConfig.LoadSynchronous())
		{
			EnqueueSpawnProject(Config, Settings->SphereRadius);
		}
		else
		{
			UE_LOG(LogLootNPop, Warning, TEXT("LNPMassSpawnSubsystem: MassSpawnConfig is null in LNPSettings!"));
		}
	}
}

void ULNPMassSpawnSubsystem::Tick(float DeltaTime)
{
	if (SpawnBuildFuture.IsValid())
	{
		if (!SpawnBuildFuture.IsReady())
			return;

		SpawnBuildFuture = TFuture<void>{};
		AssembleSpawnQueueFromAsyncResult();
	}

	ProcessQueue();
}

TStatId ULNPMassSpawnSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPMassSpawnSubsystem, STATGROUP_Tickables);
}

void ULNPMassSpawnSubsystem::EnqueueSpawnProject(ULNPMassSpawnConfig* InConfig, const float SphereRadius)
{
	if (!InConfig)
		return;
	if (!GetWorld())
		return;

	if (SpawnBuildFuture.IsValid() && !SpawnBuildFuture.IsReady())
	{
		UE_LOG(LogLootNPop, Warning, TEXT("LNPMassSpawnSubsystem: EnqueueSpawnProject called while async build is in progress. Ignoring."));
		return;
	}

	ActiveConfig = InConfig;
	SpawnQueue.Empty();
	AsyncBuildResult.Empty();
	CapturedAssets.Empty();
	SpawnQueueHead = 0;

	// Pre-capture UObject refs (game thread only) and assign integer indices.
	// Layout per pod set: [pod_config, enemy_config_0, enemy_config_1, ...]
	struct FPodSetBuildParams
	{
		int32 PodSetCount;
		int32 PodAssetIndex;
		struct FEnemyEntry { int32 Count; int32 AssetIndex; };
		TArray<FEnemyEntry> Enemies;
	};

	TArray<FPodSetBuildParams> SetParams;
	for (const FLNPLootPodSpawnEntry& PodSet : InConfig->LootPodSpawnSets)
	{
		FPodSetBuildParams P;
		P.PodSetCount = PodSet.PodSetCount;
		P.PodAssetIndex = CapturedAssets.Num();
		CapturedAssets.Add(PodSet.LootPodEntityConfig);

		for (const FLNPEnemySpawnEntry& Enemy : PodSet.AssociatedEnemies)
		{
			P.Enemies.Add({ Enemy.Count, CapturedAssets.Num() });
			CapturedAssets.Add(Enemy.EnemyEntityConfig);
		}
		SetParams.Add(MoveTemp(P));
	}

	// Take a read-only snapshot of the surface cache — no UObject access needed in the task
	ULNPSurfaceCacheSubsystem* SurfaceCache = GetWorld()->GetSubsystem<ULNPSurfaceCacheSubsystem>();
	FLNPSurfaceCacheSnapshot CacheSnap = SurfaceCache->TakeSnapshot();

	const float MinDist          = InConfig->MinDistanceBetweenPods;
	const float EnemyRadius      = InConfig->EnemySpawnRadiusAroundPod;
	const int32 MaxRetry         = InConfig->MaxRetryCount;
	const FRandomStream Rand     = RandomStream;

	UE_LOG(LogLootNPop, Log, TEXT("LNPMassSpawnSubsystem: Launching async queue build."));

	SpawnBuildFuture = Async(EAsyncExecution::TaskGraph,
		[this,
		 Sets    = MoveTemp(SetParams),
		 Cache   = MoveTemp(CacheSnap),
		 Rand,
		 SR      = SphereRadius,
		 MinDist,
		 EnemyRadius,
		 MaxRetry]() mutable
		{
			TArray<FVector> OccupiedPods;
			TArray<FLNPAsyncSpawnEntry> Results;

			for (const auto& Set : Sets)
			{
				for (int32 i = 0; i < Set.PodSetCount; ++i)
				{
					// Find pod surface point using cache lookup + min-distance check
					FVector PodLocation;
					bool bFoundPod = false;
					FVector BaseDir = FVector::DownVector;

					for (int32 Retry = 0; Retry < MaxRetry; ++Retry)
					{
						FVector SearchDir = (Retry == 0) ? BaseDir : (BaseDir + Rand.GetUnitVector() * 0.05f).GetSafeNormal();
						FVector Candidate;
						if (!Cache.GetPoint(SearchDir, Candidate))
							continue;

						if (MinDist > 0.0f)
						{
							bool bTooClose = false;
							for (const FVector& Occ : OccupiedPods)
							{
								if (FVector::DistSquared(Candidate, Occ) < FMath::Square(MinDist))
								{
									bTooClose = true;
									break;
								}
							}
							if (bTooClose)
								continue;
						}

						PodLocation = Candidate;
						bFoundPod = true;
						break;
					}

					if (!bFoundPod)
						continue;

					OccupiedPods.Add(PodLocation);
					TSharedPtr<FLNPSpawnLink> SpawnLink = MakeShared<FLNPSpawnLink>();
					SpawnLink->PodLocation = PodLocation;

					// Pod entry
					FLNPAsyncSpawnEntry PodEntry;
					PodEntry.RequestType = ELNPSpawnRequestType::LootPod;
					PodEntry.AssetIndex  = Set.PodAssetIndex;
					PodEntry.SpawnLink   = SpawnLink;
					FVector PodUp = -PodLocation.GetSafeNormal();
					PodEntry.Transforms.Add(FTransform(UKismetMathLibrary::MakeRotFromZ(PodUp), PodLocation));
					Results.Add(MoveTemp(PodEntry));

					// Enemy entries
					const FVector PodNormal = PodLocation.GetSafeNormal();
					for (const auto& EnemySet : Set.Enemies)
					{
						FLNPAsyncSpawnEntry EnemyEntry;
						EnemyEntry.RequestType = ELNPSpawnRequestType::Enemy;
						EnemyEntry.AssetIndex  = EnemySet.AssetIndex;
						EnemyEntry.SpawnLink   = SpawnLink;

						TArray<FVector> BatchLocations;
						for (int32 j = 0; j < EnemySet.Count; ++j)
						{
							FVector EnemyPos;
							bool bFoundSpot = false;

							for (int32 ERetry = 0; ERetry < 10; ++ERetry)
							{
								FVector Tangent = Rand.GetUnitVector();
								Tangent = FVector::VectorPlaneProject(Tangent, PodNormal).GetSafeNormal();
								float Dist = Rand.FRandRange(400.0f, EnemyRadius);
								FVector EDir = (PodNormal + (Tangent * (Dist / SR))).GetSafeNormal();

								if (!Cache.GetPoint(EDir, EnemyPos))
									continue;

								bool bTooClose = false;
								for (const FVector& OtherPos : BatchLocations)
								{
									if (FVector::DistSquared(EnemyPos, OtherPos) < FMath::Square(200.0f))
									{
										bTooClose = true;
										break;
									}
								}
								if (!bTooClose)
								{
									bFoundSpot = true;
									break;
								}
							}

							if (bFoundSpot)
							{
								BatchLocations.Add(EnemyPos);
								FVector EUp = -EnemyPos.GetSafeNormal();
								EnemyEntry.Transforms.Add(FTransform(UKismetMathLibrary::MakeRotFromZ(EUp), EnemyPos));
							}
						}

						if (EnemyEntry.Transforms.Num() > 0)
							Results.Add(MoveTemp(EnemyEntry));
					}
				}
			}

			// TFuture completion provides the happens-before guarantee for this write
			AsyncBuildResult = MoveTemp(Results);
		});
}

void ULNPMassSpawnSubsystem::AssembleSpawnQueueFromAsyncResult()
{
	SpawnQueue.Empty();
	SpawnQueue.Reserve(AsyncBuildResult.Num());

	for (FLNPAsyncSpawnEntry& Entry : AsyncBuildResult)
	{
		if (!CapturedAssets.IsValidIndex(Entry.AssetIndex))
			continue;

		FLNPMassSpawnRequest Req;
		Req.ConfigAsset     = CapturedAssets[Entry.AssetIndex];
		Req.TargetTransforms = MoveTemp(Entry.Transforms);
		Req.RequestType     = Entry.RequestType;
		Req.SpawnLink       = Entry.SpawnLink;
		SpawnQueue.Add(MoveTemp(Req));
	}

	AsyncBuildResult.Empty();
	CapturedAssets.Empty();
	SpawnQueueHead = 0;

	UE_LOG(LogLootNPop, Log, TEXT("LNPMassSpawnSubsystem: Assembled %d spawn requests from async build."), SpawnQueue.Num());
}

void ULNPMassSpawnSubsystem::ProcessQueue()
{
	if (SpawnQueueHead >= SpawnQueue.Num() || ActiveConfig == nullptr)
		return;

	UMassSpawnerSubsystem* SpawnerSubsystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(GetWorld());
	if (SpawnerSubsystem == nullptr)
		return;

	UWorld* World = GetWorld();
	check(World);

	int32 SpawnsThisFrame = 0;
	int32 MaxPerFrame = ActiveConfig->MaxSpawnsPerFrame;

	while (SpawnQueueHead < SpawnQueue.Num() && SpawnsThisFrame < MaxPerFrame)
	{
		FLNPMassSpawnRequest& Request = SpawnQueue[SpawnQueueHead];
		
		int32 RemainingInBatch = Request.TargetTransforms.Num() - Request.ProcessedCount;
		int32 ToSpawn = FMath::Min(RemainingInBatch, MaxPerFrame - SpawnsThisFrame);

		if (ToSpawn > 0)
		{
			TArray<FTransform> Slice;
			for (int32 i = 0; i < ToSpawn; ++i)
			{
				Slice.Add(Request.TargetTransforms[Request.ProcessedCount + i]);
			}

			if (const UMassEntityConfigAsset* EntityConfig = Request.ConfigAsset)
			{
				const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World);
				if (EntityTemplate.IsValid())
				{
					TArray<FMassEntityHandle> OutEntities;
					SpawnerSubsystem->SpawnEntities(EntityTemplate, ToSpawn, OutEntities);
					
					// If it's a Pod, we should only be spawning one at a time based on our Enqueue logic
					if (Request.RequestType == ELNPSpawnRequestType::LootPod && OutEntities.Num() > 0)
					{
						Request.SpawnLink->PodHandle = OutEntities[0];
					}

					FMassEntityHandle ParentPod;
					FVector ParentLoc = FVector::ZeroVector;
					if (Request.SpawnLink.IsValid())
					{
						ParentPod = Request.SpawnLink->PodHandle;
						ParentLoc = Request.SpawnLink->PodLocation;
					}

					SetupSpawnedEntities(OutEntities, Slice, ParentPod, ParentLoc);
				}
			}

			Request.ProcessedCount += ToSpawn;
			SpawnsThisFrame += ToSpawn;
		}

		if (Request.IsComplete())
		{
			++SpawnQueueHead;
		}
	}

	// All processed: release memory and notify
	if (SpawnQueueHead >= SpawnQueue.Num())
	{
		SpawnQueue.Empty();
		SpawnQueueHead = 0;
		ActiveConfig = nullptr;
		UE_LOG(LogLootNPop, Log, TEXT("LNPMassSpawnSubsystem: All entities spawned."));
		OnSpawningComplete.Broadcast();
	}
}

void ULNPMassSpawnSubsystem::SetupSpawnedEntities(TConstArrayView<FMassEntityHandle> Entities, TConstArrayView<FTransform> Transforms, FMassEntityHandle ParentLootPod, const FVector& ParentPodLocation)
{
	UWorld* World = GetWorld();
	check(World);

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	int32 NumToProcess = FMath::Min(Entities.Num(), Transforms.Num());
	for (int32 i = 0; i < NumToProcess; ++i)
	{
		const FMassEntityHandle Entity = Entities[i];

		// 1. Set Transform
		if (FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
		{
			TransformFragment->SetTransform(Transforms[i]);
		}

		// 2. Set Leash Metadata (if enemy and parent is valid)
		if (ParentLootPod.IsValid())
		{
			if (FLNPEnemyFragment* EnemyFragment = EntityManager.GetFragmentDataPtr<FLNPEnemyFragment>(Entity))
			{
				EnemyFragment->ParentLootPod = ParentLootPod;
				EnemyFragment->ParentPodLocation = ParentPodLocation;
			}
		}
	}
	
	UE_LOG(LogLootNPop, Verbose, TEXT("LNPMassSpawnSubsystem: Initialized %d spawned entities."), NumToProcess);
}
