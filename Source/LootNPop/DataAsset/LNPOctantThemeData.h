// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
#include "LNPOctantThemeData.generated.h"

/**
 * Represents a single prop entry with a mesh and its spawning weight.
 */
USTRUCT(BlueprintType)
struct FLNPPropEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
};

/**
 * Data asset containing theme-specific prop configurations for Octant generation.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPOctantThemeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Theme")
	TArray<FLNPPropEntry> PropEntries;
};
