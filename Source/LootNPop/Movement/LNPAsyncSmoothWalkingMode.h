// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/AsyncWalkingMode.h"
#include "Movement/LNPSmoothWalkingState.h"
#include "LNPAsyncSmoothWalkingMode.generated.h"

class UCommonLegacyMovementSettings;

/**
 * Custom movement mode that combines the Async Physics benefits of AsyncWalkingMode
 * with the high-quality interpolation of SmoothWalkingMode.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPAsyncSmoothWalkingMode : public UAsyncWalkingMode
{
	GENERATED_BODY()

public:
	ULNPAsyncSmoothWalkingMode(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegistered(const FName ModeName) override;
	virtual void OnUnregistered() override;

	/** --- Smooth Walking Parameters (Mirrored from SmoothWalkingMode) --- */
	
	/** Base acceleration to apply when the desired velocity magnitude is greater than the current velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float Acceleration = 4000.f;

	/** Base deceleration to apply when the desired velocity magnitude is less than the current velocity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float Deceleration = 4000.f;

	/**
	 * Value between 0 and 1 that controls how the acceleration is applied. When set to 1 this will effectively add the acceleration in the direction 
	 * of the desired velocity on top of the current velocity then clip the result. This emulates the behavior of the default walking mode. When set 
	 * to 0 it will apply an acceleration which changes the current velocity directly towards the desired velocity. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float DirectionalAccelerationFactor = 1.0f;

	/**
	 * Applies an additional force that rotates the velocity to make it face the desired direction of travel. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0"))
	float TurningStrength = 10.0f;

	/** Controls how much smoothing is applied to the velocity changes of the movement when accelerating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "s"))
	float AccelerationSmoothingTime = 0.1f;

	/** Controls how much smoothing is applied to the velocity changes of the movement when decelerating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "s"))
	float DecelerationSmoothingTime = 0.1f;

	/**
	 * Controls how much smoothing is compensated for by tracking a future target.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float AccelerationSmoothingCompensation = 0.0f;

	/** This parameter acts the same as AccelerationSmoothingCompensation but is applied during deceleration instead */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float DecelerationSmoothingCompensation = 0.1f;

	/** Controls the point at which the velocity will "snap" to the desired velocity once it is close enough */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float VelocityDeadzoneThreshold = 0.01f;

	/** Controls the point at which the acceleration will "snap" to zero when the desired velocity is reached */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float AccelerationDeadzoneThreshold = 0.001f;

	/** Controls how quickly the built-up internal velocity will be modified when the character's movement is influenced by the outside factors */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "s"))
	float OutsideInfluenceSmoothingTime = 0.05f;

	/** Controls how much smoothing is applied to the facing direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "s"))
	float FacingSmoothingTime = 0.25f;

	/** Smooths facing using a double spring rather than a single spring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings")
	bool bSmoothFacingWithDoubleSpring = true;

	/** Controls the point at which the facing will "snap" to the desired facing once it is close enough */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "deg"))
	float FacingDeadzoneThreshold = 0.1f;

	/** Controls the point at which the angular velocity will "snap" to zero when the desired facing is reached */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "deg/s"))
	float AngularVelocityDeadzoneThreshold = 0.01f;

protected:
	/** Overridden to apply smooth spring interpolation before physical movement occurs. */
	virtual void GenerateMove_Implementation(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	/** Overridden to capture and sync the smoothing state. */
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

private:
	/** Helper to run the spring math, ported from SmoothWalkingMode's internal logic. */
	void ApplySmoothing(FMoverTickStartData& StartState, float DeltaSeconds, const FVector& DesiredVelocity, const FQuat& DesiredFacing, const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity) const;
};
