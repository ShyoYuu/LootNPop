// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemInterface.h"
#include "MassAgentComponent.h"
#include "LNPCharacterBase.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UAbilitySystemComponent;
class UMassAgentComponent;

class ULNPCharacterMoverComponent;
class ULNPPawnInputComponent;
class ULNPPawnGravityComponent;
class ULNPInteractionComponent;
class ALNPLootPod;
class ULNPWeaponData;


/**
 * ALNPCharacterBase
 */
UCLASS(Abstract)
class LOOTNPOP_API ALNPCharacterBase : public APawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ALNPCharacterBase(const FObjectInitializer& ObjectInitializer);

	// --- Component Accessors ---
	UFUNCTION(BlueprintPure, Category = "LNP|Mover")
	ULNPCharacterMoverComponent* GetMoverComponent() const { return MoverComponent; }

	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool GetOrientRotationToMovement() const;

	// IAbilitySystemInterface implementation
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	TArray<AActor*> GetInteractionCandidates() const;

	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	AActor* GetInteractionCandidate() const;

	USkeletalMeshComponent*        GetMesh()                  const { return Mesh; }
	UCapsuleComponent*             GetCapsule()               const { return CapsuleComponent; }
	ULNPPawnInputComponent*        GetInputHandlerComponent() const { return InputHandlerComponent; }

	void SetAIMoveInput(FVector InMoveInput);
	void SetAIOrientationIntent(FVector InOrientationIntent);

	virtual bool TryActivateAttack();

	/** Returns the weapon data currently equipped/configured for this character. */
	virtual const ULNPWeaponData* GetActiveWeaponDef() const { return nullptr; }

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPCharacterMoverComponent> MoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMassAgentComponent> MassAgentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPPawnInputComponent> InputHandlerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Gravity", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPPawnGravityComponent> GravityComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPInteractionComponent> InteractionComponent;
};
