// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPMassSpawnSubsystem.h"
#include "DataAsset/LNPMassSpawnConfig.h"
#include "Config/LNPSettings.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "LootNPop.h"

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
	ProcessQueue();
}

TStatId ULNPMassSpawnSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPMassSpawnSubsystem, STATGROUP_Tickables);
}

void ULNPMassSpawnSubsystem::EnqueueSpawnProject(ULNPMassSpawnConfig* InConfig, const float SphereRadius)
{
	if (InConfig == nullptr)
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	ActiveConfig = InConfig;
	OccupiedPodLocations.Empty();
	SpawnQueue.Empty();
	SpawnQueueHead = 0;

	for (const FLNPLootPodSpawnEntry& PodSet : InConfig->LootPodSpawnSets)
	{
		for (int32 i = 0; i < PodSet.PodSetCount; ++i)
		{
			FVector PodLocation;
			FVector RandomDir = FVector::DownVector; //RandomStream.GetUnitVector();

			if (FindValidSurfacePoint(RandomDir, SphereRadius, InConfig->MinDistanceBetweenPods, InConfig->MaxRetryCount, PodLocation))
			{
				OccupiedPodLocations.Add(PodLocation);

				// Create a shared link for this Pod and its associated enemies
				TSharedPtr<FLNPSpawnLink> SpawnLink = MakeShared<FLNPSpawnLink>();
				SpawnLink->PodLocation = PodLocation;

				// 1. Queue Pod Spawn
				if (PodSet.LootPodEntityConfig)
				{
					FLNPMassSpawnRequest PodRequest;
					PodRequest.ConfigAsset = PodSet.LootPodEntityConfig;
					PodRequest.RequestType = ELNPSpawnRequestType::LootPod;
					PodRequest.SpawnLink = SpawnLink;
					
					FVector PodUpVector = -PodLocation.GetSafeNormal();
					FRotator PodRotation = UKismetMathLibrary::MakeRotFromZ(PodUpVector);
					PodRequest.TargetTransforms.Add(FTransform(PodRotation, PodLocation));
					
					SpawnQueue.Add(PodRequest);
				}

				// 2. Queue Associated Enemy Spawn Requests
				for (const FLNPEnemySpawnEntry& EnemyEntry : PodSet.AssociatedEnemies)
				{
					FLNPMassSpawnRequest EnemyRequest;
					EnemyRequest.ConfigAsset = EnemyEntry.EnemyEntityConfig;
					EnemyRequest.RequestType = ELNPSpawnRequestType::Enemy;
					EnemyRequest.SpawnLink = SpawnLink;

					FVector PodNormal = PodLocation.GetSafeNormal();
					TArray<FVector> CurrentBatchEnemyLocations;

					for (int32 j = 0; j < EnemyEntry.Count; ++j)
					{
						FVector EnemyPos;
						bool bFoundValidSpot = false;

						for (int32 Retry = 0; Retry < 10; ++Retry)
						{
							FVector RandomTangent = RandomStream.GetUnitVector();
							RandomTangent = FVector::VectorPlaneProject(RandomTangent, PodNormal).GetSafeNormal();
							
							float DistFromPod = RandomStream.FRandRange(400.0f, InConfig->EnemySpawnRadiusAroundPod);
							FVector EnemyDir = (PodNormal + (RandomTangent * (DistFromPod / SphereRadius))).GetSafeNormal();
							
							if (FindValidSurfacePoint(EnemyDir, SphereRadius, 0.0f, 5, EnemyPos))
							{
								bool bTooCloseToOthers = false;
								for (const FVector& OtherPos : CurrentBatchEnemyLocations)
								{
									if (FVector::DistSquared(EnemyPos, OtherPos) < FMath::Square(200.0f))
									{
										bTooCloseToOthers = true;
										break;
									}
								}

								if (!bTooCloseToOthers)
								{
									bFoundValidSpot = true;
									break;
								}
							}
						}

						if (bFoundValidSpot)
						{
							CurrentBatchEnemyLocations.Add(EnemyPos);
							FVector EnemyUpVector = -EnemyPos.GetSafeNormal();
							FRotator EnemyRot = UKismetMathLibrary::MakeRotFromZ(EnemyUpVector);
							EnemyRequest.TargetTransforms.Add(FTransform(EnemyRot, EnemyPos));
						}
					}
					
					if (EnemyRequest.TargetTransforms.Num() > 0)
					{
						SpawnQueue.Add(EnemyRequest);
					}
				}
			}
		}
	}
}

bool ULNPMassSpawnSubsystem::FindValidSurfacePoint(const FVector& SearchDir, float SphereRadius, float MinDistance, int32 MaxRetries, FVector& OutLocation)
{
	UWorld* World = GetWorld();
	check(World);

	for (int32 i = 0; i < MaxRetries; ++i)
	{
		FVector CurrentDir = (i == 0) ? SearchDir : (SearchDir + RandomStream.GetUnitVector() * 0.05f).GetSafeNormal();
		
		FVector Start = CurrentDir * (SphereRadius * 0.5f); 
		FVector End = CurrentDir * (SphereRadius * 1.5f);

		FHitResult Hit;
		FCollisionQueryParams Params;
		
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			float Dot = FVector::DotProduct(Hit.ImpactNormal, -CurrentDir);
			if (Dot < 0.766f)
				continue; 

			if (MinDistance > 0.0f)
			{
				bool bTooClose = false;
				for (const FVector& OccLoc : OccupiedPodLocations)
				{
					if (FVector::DistSquared(Hit.ImpactPoint, OccLoc) < FMath::Square(MinDistance))
					{
						bTooClose = true;
						break;
					}
				}
				if (bTooClose)
					continue;
			}

			OutLocation = Hit.ImpactPoint;
			return true;
		}
	}

	return false;
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
