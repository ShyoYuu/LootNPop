// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "LNPGameplayAbility.generated.h"

class ALNPCharacterBase;
class ALNPPlayerState;

UCLASS(Abstract, BlueprintType, Blueprintable)
class LOOTNPOP_API ULNPGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
protected:
	/** Returns the Pawn avatar if it is an LNP character. */
	UFUNCTION(BlueprintPure, Category = "LNP|Ability")
	ALNPCharacterBase* GetOwningCharacter() const;

	/** Returns the LNP PlayerState that owns the ASC activating this ability. */
	UFUNCTION(BlueprintPure, Category = "LNP|Ability")
	ALNPPlayerState* GetOwningLNPPlayerState() const;
};
