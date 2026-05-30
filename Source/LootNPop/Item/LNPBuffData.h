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
	/** 최대 지속 시간(초). 0 = 무한. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff", meta = (ClampMin = "0"))
	float MaxDuration = 0.0f;
};
