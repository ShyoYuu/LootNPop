// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Character/LNPCharacterBase.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "GameplayAbilitySpec.h"
#include "LNPEnemyCharacter.generated.h"

class ULNPEnemyConfig;
class UAbilitySystemComponent;
class ULNPBaseAttributeSet;
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

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual bool TryActivateAttack() override;

	/** Initializes the character using the provided configuration asset */
	void InitializeFromConfig(ULNPEnemyConfig* InConfig);

	/** Mass -> Actor sync: Called when actor is spawned/activated from Mass */
	void SyncFromEntity(FMassEntityHandle InEntityHandle, float InHealth, ELNPTargetingState InTargetingState);

	/** Actor -> Mass sync: Called when actor is about to be deactivated/destroyed back to Mass */
	void SyncToEntity(float& OutHealth) const;

	/** Activates physics ragdoll and disables movement. Safe to call multiple times. */
	void TriggerRagdoll();

protected:
	virtual void BeginPlay() override;
	virtual const ULNPWeaponData* GetActiveWeaponDef() const override;

	/** The configuration asset used to initialize this enemy */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Enemy")
	TObjectPtr<ULNPEnemyConfig> EnemyConfig;

	/** Handle to the weapon attack ability granted from DefaultAbilities[0] */
	FGameplayAbilitySpecHandle WeaponAbilityHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|GAS")
	TObjectPtr<UAbilitySystemComponent> ASC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|GAS")
	TObjectPtr<ULNPBaseAttributeSet> AttributeSet;
};
