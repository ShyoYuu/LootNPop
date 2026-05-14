// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "LNPGameplayEffect_Damage.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Damage)

/** Instant damage GE. Magnitude is set via SetByCaller with TAG_GE_Data_Damage (pass as negative value). */
UCLASS()
class LOOTNPOP_API ULNPGameplayEffect_Damage : public UGameplayEffect
{
	GENERATED_BODY()
public:
	ULNPGameplayEffect_Damage();
};
