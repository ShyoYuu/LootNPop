// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MoverSimulationTypes.h"
#include "AbilitySystemInterface.h"
#include "MassAgentComponent.h"
#include "LNPCharacterBase.generated.h"

class UCharacterMoverComponent;
class UCapsuleComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UAbilitySystemComponent;
class UMassAgentComponent;
class ULNPPawnGravityComponent;
class ULNPInteractionComponent;
class ALNPLootPod;
struct FInputActionValue;

/**
 * ALNPCharacterBase
 */
UCLASS(Abstract)
class LOOTNPOP_API ALNPCharacterBase : public APawn, public IMoverInputProducerInterface, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ALNPCharacterBase(const FObjectInitializer& ObjectInitializer);

	// --- Component Accessors ---
	UFUNCTION(BlueprintPure, Category = "LNP|Mover")
	UCharacterMoverComponent* GetMoverComponent() const { return MoverComponent; }

	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool GetOrientRotationToMovement() const { return bOrientRotationToMovement; }

	// IAbilitySystemInterface implementation
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Returns all currently highlighted interaction targets (for UI) */
	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	TArray<AActor*> GetInteractionCandidates() const;

	/** Returns the first valid interaction target (for backward compatibility) */
	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	AActor* GetInteractionCandidate() const;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// --- Mover Input Generation Logic ---
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;
	virtual void OnProduceInput(float DeltaMs, FMoverInputCmdContext& OutInputCmd);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterMoverComponent> MoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMassAgentComponent> MassAgentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gravity", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPPawnGravityComponent> GravityComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPInteractionComponent> InteractionComponent;

protected:
	// --- Input Settings ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	// Movement settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	bool bUseBaseRelativeMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	bool bOrientRotationToMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	bool bMaintainLastInputOrientation = false;

private:
	FVector CachedMoveInputIntent = FVector::ZeroVector;
	FRotator CachedLookInput = FRotator::ZeroRotator;
	FVector LastAffirmativeMoveInput = FVector::ZeroVector;

	bool bIsJumpPressed = false;
	bool bIsJumpJustPressed = false;
	bool bIsInteractPressed = false;
	bool bIsInteractJustPressed = false;

	// Input event handlers
	void OnMoveTriggered(const FInputActionValue& Value);
	void OnMoveCompleted(const FInputActionValue& Value);
	void OnLookTriggered(const FInputActionValue& Value);
	void OnLookCompleted(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpReleased(const FInputActionValue& Value);
	void OnInteractStarted(const FInputActionValue& Value);
	void OnInteractReleased(const FInputActionValue& Value);
};