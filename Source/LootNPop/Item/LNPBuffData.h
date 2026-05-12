// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Item/LNPItemDefinitionBase.h"
#include "LNPBuffData.generated.h"

UCLASS(BlueprintType)
class LOOTNPOP_API ULNPBuffData : public ULNPItemDefinitionBase
{
	GENERATED_BODY()
public:
	/** Maximum duration in seconds. 0 = infinite. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff", meta = (ClampMin = "0"))
	float MaxDuration = 0.0f;
};
