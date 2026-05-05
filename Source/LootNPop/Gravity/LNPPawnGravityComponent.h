// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LNPPawnGravityComponent.generated.h"

class UCharacterMoverComponent;

/** Gravity operation modes */
UENUM(BlueprintType)
enum class ELNPGravityType : uint8
{
	None,
	Fixed,          // Static gravity (points in a specific fixed direction)
	RadialInward,   // Dynamic gravity: Pulls towards a center point (Planet-like)
	RadialOutward   // Dynamic gravity: Pushes away from a center point (Inverted sphere world)
};

/**
 * ULNPPawnGravityComponent
 * Manages custom gravity for the pawn (physics via MoverComponent) 
 * and handles curvature-aware camera/control rotation for the player.
 */
UCLASS( ClassGroup=(LNP), meta=(BlueprintSpawnableComponent) )
class LOOTNPOP_API ULNPPawnGravityComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	ULNPPawnGravityComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Updates the gravity mode and its direction/origin point */
	UFUNCTION(BlueprintCallable, Category = "LNP|Gravity")
	void SetGravity(const ELNPGravityType NewType, const FVector& NewDirectionOrOrigin);

	/** Returns the current calculated Up direction based on the current gravity type and owner location */
	UFUNCTION(BlueprintPure, Category = "LNP|Gravity")
	FVector GetUpDirection() const;

	/** Input look delta to be processed in the next tick */
	void InputLook(const FRotator& LookDelta);

protected:
	/** Currently active gravity mode */
	UPROPERTY(EditAnywhere, Category = "LNP|Gravity")
	ELNPGravityType GravityType = ELNPGravityType::None;

	/** Center of gravity origin (used in Radial modes) */
	UPROPERTY(EditAnywhere, Category = "LNP|Gravity")
	FVector GravityOrigin = FVector::ZeroVector;

	/** Desired gravity direction for Fixed mode (default is world down) */
	UPROPERTY(EditAnywhere, Category = "LNP|Gravity")
	FVector FixedGravityDirection = FVector::DownVector;

	/** Acceleration magnitude for gravity */
	UPROPERTY(EditAnywhere, Category = "LNP|Gravity")
	float GravityStrength = 2000.0f;

private:
	/** Cached reference to the owner's mover component */
	UPROPERTY(Transient)
	TObjectPtr<UCharacterMoverComponent> CachedMoverComponent;

	/** Accumulated look input from the character */
	FRotator PendingLookInput = FRotator::ZeroRotator;

	/** Keeps track of the previous mode to detect transitions */
	ELNPGravityType LastGravityType = ELNPGravityType::None;

	/** Cached Up direction for camera curvature compensation */
	FVector LastUpDir = FVector::UpVector;

	/** Updates physical gravity and capsule orientation based on new directions */
	void UpdatePawnOrientation(const FVector& PawnUpDir, const FVector& PawnDownDir);

	/** Adjusts the player controller's rotation matrix to match world curvature and look inputs */
	void UpdateControllerOrientation(float DeltaTime, const FVector& TargetUpDir);
};
