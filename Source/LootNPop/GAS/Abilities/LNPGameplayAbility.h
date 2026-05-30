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
	/** 아바타 Pawn이 LNP 캐릭터이면 반환한다. */
	UFUNCTION(BlueprintPure, Category = "LNP|Ability")
	ALNPCharacterBase* GetOwningCharacter() const;

	/** 이 Ability를 활성화하는 ASC를 소유한 LNP PlayerState를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "LNP|Ability")
	ALNPPlayerState* GetOwningLNPPlayerState() const;
};
