// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LNPOctantPoolData.generated.h"

/**
 * 
 */
UCLASS()
class LOOTNPOP_API ULNPOctantPoolData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|World Generation")
	TArray<TSoftObjectPtr<UWorld>> OctantPool;
};
