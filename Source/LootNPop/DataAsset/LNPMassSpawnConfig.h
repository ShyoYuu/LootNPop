// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LNPMassSpawnConfig.generated.h"

class UMassEntityConfigAsset;

USTRUCT(BlueprintType)
struct FLNPEnemySpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "LNP")
	TObjectPtr<UMassEntityConfigAsset> EnemyEntityConfig;

	UPROPERTY(EditAnywhere, Category = "LNP")
	int32 Count = 10;
};

USTRUCT(BlueprintType)
struct FLNPLootPodSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "LNP")
	TObjectPtr<UMassEntityConfigAsset> LootPodEntityConfig;

	/** 이 LootPod 주변에 편성될 적들 */
	UPROPERTY(EditAnywhere, Category = "LNP")
	TArray<FLNPEnemySpawnEntry> AssociatedEnemies;

	/** 이 구성을 가진 세트의 생성 개수 */
	UPROPERTY(EditAnywhere, Category = "LNP")
	int32 PodSetCount = 5;
};

/**
 * Configuration asset for initial entity spawning.
 */
UCLASS()
class LOOTNPOP_API ULNPMassSpawnConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "LNP|Setup")
	TArray<FLNPLootPodSpawnEntry> LootPodSpawnSets;

	UPROPERTY(EditAnywhere, Category = "LNP|Constraints")
	float MinDistanceBetweenPods = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "LNP|Constraints")
	float EnemySpawnRadiusAroundPod = 800.0f;

	UPROPERTY(EditAnywhere, Category = "LNP|Constraints")
	int32 MaxRetryCount = 15;

	/** Maximum entities allowed to spawn in a single frame to maintain FPS */
	UPROPERTY(EditAnywhere, Category = "LNP|Performance")
	int32 MaxSpawnsPerFrame = 50;
};
