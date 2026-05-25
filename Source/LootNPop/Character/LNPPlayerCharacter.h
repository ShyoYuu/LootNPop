// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/LNPCharacterBase.h"
#include "LNPPlayerCharacter.generated.h"

class UGameplayCameraComponent;
class ULNPInteractionComponent;

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

	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	TArray<AActor*> GetInteractionCandidates() const;

	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	AActor* GetInteractionCandidate() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGameplayCameraComponent> GameplayCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPInteractionComponent> InteractionComponent;
};
