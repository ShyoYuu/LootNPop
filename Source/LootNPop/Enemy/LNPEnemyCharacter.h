// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Character/LNPCharacterBase.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "LNPEnemyCharacter.generated.h"

class ULNPEnemyConfig;
struct FMassEntityHandle;

/**
 * Generic Enemy Character that acts as a shell.
 * Visuals and Abilities are initialized from ULNPEnemyConfig.
 */
UCLASS()
class LOOTNPOP_API ALNPEnemyCharacter : public ALNPCharacterBase
{
	GENERATED_BODY()

public:
	ALNPEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	/** Initializes the character using the provided configuration asset */
	void InitializeFromConfig(ULNPEnemyConfig* InConfig);

	/** Mass -> Actor sync: Called when actor is spawned/activated from Mass */
	void SyncFromEntity(FMassEntityHandle InEntityHandle, float InHealth, ELNPTargetingState InTargetingState);

	/** Actor -> Mass sync: Called when actor is about to be deactivated/destroyed back to Mass */
	void SyncToEntity(float& OutHealth);

protected:
	virtual void BeginPlay() override;

	/** The configuration asset used to initialize this enemy */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Enemy")
	TObjectPtr<ULNPEnemyConfig> EnemyConfig;
};
