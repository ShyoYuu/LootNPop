// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LNPGameplayEffect_Cooldown.generated.h"

/**
 * Cooldown GE applied by ranged (and future) attack abilities.
 * Duration is injected per-spec by the ability so a single class
 * covers all weapons with different fire rates.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayEffect_Cooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	ULNPGameplayEffect_Cooldown();
};
