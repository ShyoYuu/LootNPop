// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Item/LNPItemDefinitionBase.h"
#include "LNPSkillData.generated.h"

/** 액티브 및 패시브 스킬 아이템 Data Asset.
 *  점유하는 슬롯(액티브/패시브)이 동작을 결정하며 데이터는 동일하다. */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPSkillData : public ULNPItemDefinitionBase
{
	GENERATED_BODY()
};
