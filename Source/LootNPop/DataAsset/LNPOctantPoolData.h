// Fill out your copyright notice in the Description page of Project Settings.

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
