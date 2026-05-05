// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LNPLootPod.generated.h"

UCLASS()
class LOOTNPOP_API ALNPLootPod : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ALNPLootPod();

	/** Unique identifier for reward and data lookup in MassEntity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|LootPod")
	int32 PodID = 0;

	/** Initiates the looting process, updating both Mass state and visuals */
	UFUNCTION(BlueprintCallable, Category = "LNP|Interaction")
	void StartLooting();

	/** Updates the Niagara VFX based on the current state */
	UFUNCTION(BlueprintCallable, Category = "LNP|Visuals")
	void UpdateVisuals(ELNPLootPodState NewState);

	/** Returns the current local state of the pod */
	ELNPLootPodState GetCurrentState() const { return CurrentState; }

	/** Checks if the interactor is within valid distance and angle to interact with this pod */
	UFUNCTION(BlueprintNativeEvent, Category = "LNP|Interaction")
	bool CanInteract(const APawn* Interactor) const;

	UFUNCTION()
	UMassAgentComponent* GetMassAgentComponent() const { return MassAgentComponent; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class USmartObjectComponent> SmartObjectComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UNiagaraComponent> LootPillarVFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class USphereComponent> LootingZoneSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UMassAgentComponent> MassAgentComponent;

	/** Current local state of the pod, kept in sync with visuals */
	UPROPERTY(VisibleAnywhere, Category = "LNP|LootPod")
	ELNPLootPodState CurrentState = ELNPLootPodState::Idle;

	/** Max angle (in degrees) from the pod's forward vector that interaction is allowed */
	UPROPERTY(EditAnywhere, Category = "LNP|Interaction")
	float MaxInteractionAngle = 60.0f;

	// --- Visual Settings ---
	UPROPERTY(EditAnywhere, Category = "LNP|Visuals")
	FLinearColor IdleColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "LNP|Visuals")
	FLinearColor LootingColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, Category = "LNP|Visuals")
	FLinearColor PoppedColor = FLinearColor::Yellow;

	/** Name of the Niagara parameter for color */
	UPROPERTY(EditAnywhere, Category = "LNP|Visuals")
	FName ColorParameterName = TEXT("User.Color");
};
