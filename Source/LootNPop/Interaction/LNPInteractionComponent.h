// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LNPInteractionComponent.generated.h"

class ALNPLootPod;

/**
 * Component responsible for searching and interacting with ALNPLootPod actors.
 */
UCLASS(ClassGroup = (LNP), meta = (BlueprintSpawnableComponent))
class LOOTNPOP_API ULNPInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULNPInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Continuously searches for the best interaction target nearby */
	void UpdateInteractionCandidate();

	/** Initiates actual interaction with the current candidate */
	void PerformInteraction();

	/** Returns all currently highlighted interaction targets (for UI) */
	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	TArray<AActor*> GetInteractionCandidates() const;

	/** Returns the first valid interaction target (for backward compatibility or simple logic) */
	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	AActor* GetFirstInteractionCandidate() const;

protected:
	/** Distance to search for interactable objects */
	UPROPERTY(EditAnywhere, Category = "LNP|Interaction")
	float InteractionRadius = 500.0f;

	/** The pods/actors this character is currently looking at/near (Potential targets) */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> InteractionCandidates;
};

