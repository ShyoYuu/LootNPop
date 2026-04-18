// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LNPAnimInstance.generated.h"

class ALNPCharacterBase;
class UCharacterMoverComponent;

/**
 * ULNPAnimInstance
 * Custom AnimInstance class for LootNPop project.
 * Handles character state updates in C++ for better performance and maintainability.
 */
UCLASS()
class LOOTNPOP_API ULNPAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** Reference to the owning character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Character")
	TObjectPtr<ALNPCharacterBase> OwningMoverCharacter;

	/** Reference to the character's mover component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Character")
	TObjectPtr<UCharacterMoverComponent> MoverComponent;

	/** Current velocity of the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	FVector Velocity;

	/** Horizontal speed of the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	float GroundSpeed;

	/** Indicates if the character should be playing movement animations */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bShouldMove;

	/** Is the character currently in the falling state? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsFalling;

	/** Is the character currently in the air (any non-grounded state)? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsAirborne;

	/** Movement direction relative to character rotation (-180 to 180) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	float Direction;

	/** Should the character use strafing animations? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bShouldStrafe;

	/** Is the character currently swimming? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsSwimming;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsCrouching;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsSprinting;
};
