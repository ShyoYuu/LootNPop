// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/AsyncWalkingMode.h"
#include "LNPAsyncWalkingMode.generated.h"

/**
 * 
 */
UCLASS()
class LOOTNPOP_API ULNPAsyncWalkingMode : public UAsyncWalkingMode
{
	GENERATED_BODY()
	
public:
	ULNPAsyncWalkingMode(const FObjectInitializer& ObjectInitializer);
};
