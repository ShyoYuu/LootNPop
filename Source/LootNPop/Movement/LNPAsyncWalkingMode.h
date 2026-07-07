// Copyright (c) 2026 LootNPop. All rights reserved.

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
