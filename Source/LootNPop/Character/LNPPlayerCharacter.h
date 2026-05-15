// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/LNPCharacterBase.h"
#include "LNPPlayerCharacter.generated.h"

/**
 * Player-controlled character class.
 */
UCLASS()
class LOOTNPOP_API ALNPPlayerCharacter : public ALNPCharacterBase
{
	GENERATED_BODY()

public:
	ALNPPlayerCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual const ULNPWeaponData* GetActiveWeaponDef() const override;
	virtual bool TryActivateAttack() override;
};
