// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPSphereWorldGenerator.h"
#include "Kismet/KismetMathLibrary.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "DataAsset/LNPOctantPoolData.h"

/**
 * Pre-defined rotations for 8 octants to form a sphere.
 */
const FRotator ALNPSphereWorldGenerator::OctantRotations[8] = {
	FRotator(0.f, 0.f, 0.f),    // (+X, +Y, +Z)
	FRotator(0.f, 90.f, 0.f),   // (-Y, +X, +Z)
	FRotator(0.f, 180.f, 0.f),  // (-X, -Y, +Z)
	FRotator(0.f, 270.f, 0.f),  // (+Y, -X, +Z)
	FRotator(180.f, 0.f, 0.f),   // (+X, -Y, -Z) - Flipped via Roll 180
	FRotator(180.f, 90.f, 0.f),  // (+Y, +X, -Z)
	FRotator(180.f, 180.f, 0.f), // (-X, +Y, -Z)
	FRotator(180.f, 270.f, 0.f)  // (-Y, -X, -Z)
};

ALNPSphereWorldGenerator::ALNPSphereWorldGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ALNPSphereWorldGenerator::BeginPlay()
{
	Super::BeginPlay();
	
	GenerateWorld();
}

void ALNPSphereWorldGenerator::GenerateWorld()
{
	if (OctantPoolData == nullptr || OctantPoolData->OctantPool.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SphereWorldGenerator: OctantPool is empty!"));
		return;
	}

	// Initialize random stream with seed
	FRandomStream RandomStream(Seed == 0 ? FMath::Rand() : Seed);

	// Create a "deck" of indices to ensure minimal duplicates
	TArray<int32> SelectedIndices;
	while (SelectedIndices.Num() < 8)
	{
		TArray<int32> Batch;
		for (int32 i = 0; i < OctantPoolData->OctantPool.Num(); ++i)
		{
			Batch.Add(i);
		}

		// Shuffle the current batch
		for (int32 i = Batch.Num() - 1; i > 0; --i)
		{
			int32 j = RandomStream.RandRange(0, i);
			Batch.Swap(i, j);
		}

		SelectedIndices.Append(Batch);
	}

	FVector Center = GetActorLocation();

	for (int32 i = 0; i < 8; ++i)
	{
		// Pick from the shuffled indices
		int32 OctantIndex = SelectedIndices[i];
		TSoftObjectPtr<UWorld> OctantLevel = OctantPoolData->OctantPool[OctantIndex];

		if (!OctantLevel.IsNull())
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			// Combine generator's rotation with the pre-defined octant rotation
			FRotator FinalRotation = UKismetMathLibrary::ComposeRotators(OctantRotations[i], GetActorRotation());

			// Spawn a Level Instance Actor			
			if (ALevelInstance* LevelInstance = GetWorld()->SpawnActor<ALevelInstance>(ALevelInstance::StaticClass(), Center, FinalRotation, SpawnParams))
			{
				// Assign the Level Asset
				LevelInstance->SetWorldAsset(OctantLevel);
				LevelInstance->UpdateLevelInstanceFromWorldAsset();
				LevelInstance->LoadLevelInstance();


#if WITH_EDITOR
				LevelInstance->SetActorLabel(FString::Printf(TEXT("Octant_LVI_%d_%s"), i, *OctantLevel.GetAssetName()));
#endif
			}
		}
	}
}
