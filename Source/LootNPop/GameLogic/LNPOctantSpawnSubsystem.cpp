// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "Kismet/KismetMathLibrary.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "DataAsset/LNPOctantPoolData.h"
#include "Config/LNPSettings.h"
#include "GameMode/LNPGameState.h"

const FRotator ULNPOctantSpawnSubsystem::OctantRotations[8] = {
	FRotator(0.f, 0.f, 0.f),
	FRotator(0.f, 90.f, 0.f),
	FRotator(0.f, 180.f, 0.f),
	FRotator(0.f, 270.f, 0.f),
	FRotator(180.f, 0.f, 0.f),
	FRotator(180.f, 90.f, 0.f),
	FRotator(180.f, 180.f, 0.f),
	FRotator(180.f, 270.f, 0.f)
};

void ULNPOctantSpawnSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	StartWorldGeneration();
}

void ULNPOctantSpawnSubsystem::Tick(float DeltaTime)
{
	if (!bIsGenerating) return;

	bool bAllLoaded = true;
	for (ALevelInstance* Octant : SpawnedOctants)
	{
		if (!Octant || !Octant->IsLoaded())
		{
			bAllLoaded = false;
			break;
		}
	}

	if (bAllLoaded && SpawnedOctants.Num() >= 8)
	{
		bIsGenerating = false;
		UE_LOG(LogTemp, Log, TEXT("LNPOctantSpawnSubsystem: All 8 octants are fully loaded. Broadcasting Finished event."));
		OnWorldGenerationFinished.Broadcast();
		SpawnedOctants.Empty(); // Clear references
	}
}

TStatId ULNPOctantSpawnSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPOctantSpawnSubsystem, STATGROUP_Tickables);
}

void ULNPOctantSpawnSubsystem::StartWorldGeneration()
{
	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	if (Settings == nullptr)
		return;

	ULNPOctantPoolData* PoolData = Settings->OctantPool.LoadSynchronous();
	if (PoolData == nullptr || PoolData->OctantPool.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("LNPOctantSpawnSubsystem: OctantPool is empty or not set in LNPSettings!"));
		return;
	}

	UWorld* World = GetWorld();
	check(World);

	int32 OctantGenSeed = 0;
	if (ALNPGameState* GS = World->GetGameState<ALNPGameState>())
		OctantGenSeed = GS->OctantGenSeed;

	FRandomStream RandomStream(OctantGenSeed == 0 ? FMath::Rand() : OctantGenSeed);

	SpawnedOctants.Empty();
	bIsGenerating = true;

	TArray<int32> SelectedIndices;
	while (SelectedIndices.Num() < 8)
	{
		TArray<int32> Batch;
		for (int32 i = 0; i < PoolData->OctantPool.Num(); ++i)
			Batch.Add(i);
		for (int32 i = Batch.Num() - 1; i > 0; --i)
			Batch.Swap(i, RandomStream.RandRange(0, i));
		SelectedIndices.Append(Batch);
	}

	for (int32 i = 0; i < 8; ++i)
	{
		int32 OctantIndex = SelectedIndices[i];
		TSoftObjectPtr<UWorld> OctantLevel = PoolData->OctantPool[OctantIndex];

		if (!OctantLevel.IsNull())
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (ALevelInstance* LevelInstance = World->SpawnActor<ALevelInstance>(ALevelInstance::StaticClass(), FVector::ZeroVector, OctantRotations[i], SpawnParams))
			{
				LevelInstance->SetWorldAsset(OctantLevel);
				LevelInstance->LoadLevelInstance();
				SpawnedOctants.Add(LevelInstance);
#if WITH_EDITOR
				LevelInstance->SetActorLabel(FString::Printf(TEXT("Octant_LVI_%d"), i));
#endif
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("LNPOctantSpawnSubsystem: Spawned 8 LevelInstances. Waiting for load..."));
}
