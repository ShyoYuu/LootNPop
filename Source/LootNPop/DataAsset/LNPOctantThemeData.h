// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
#include "LNPOctantThemeData.generated.h"

/**
 * 메시와 스폰 가중치를 가진 단일 프랍 항목을 나타낸다.
 */
USTRUCT(BlueprintType)
struct FLNPPropEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Theme")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Theme", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Theme")
	FVector MinScale = FVector(0.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Theme")
	FVector MaxScale = FVector(1.2f);
};

/**
 * Octant 생성을 위한 테마별 프랍 설정을 담은 Data Asset.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPOctantThemeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Theme")
	TArray<FLNPPropEntry> PropEntries;
};
