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

	/** 이 LootPod 주변의 Enemy 편성 정보 */
	UPROPERTY(EditAnywhere, Category = "LNP")
	TArray<FLNPEnemySpawnEntry> AssociatedEnemies;

	/** 이 구성을 가진 세트의 생성 개수 */
	UPROPERTY(EditAnywhere, Category = "LNP")
	int32 PodSetCount = 5;
};

/**
 * 초기 Entity 스폰을 위한 설정 에셋.
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

	/** FPS 유지를 위해 단일 프레임에 허용되는 최대 스폰 Entity 수 */
	UPROPERTY(EditAnywhere, Category = "LNP|Performance")
	int32 MaxSpawnsPerFrame = 50;
};
