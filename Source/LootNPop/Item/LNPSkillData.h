// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Item/LNPItemDefinitionBase.h"
#include "LNPSkillData.generated.h"

/** Data asset for Active and Passive skill items.
 *  Which slot it occupies (active/passive) determines behavior; the data is identical. */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPSkillData : public ULNPItemDefinitionBase
{
	GENERATED_BODY()
};
