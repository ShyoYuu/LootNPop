// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "LNPSmoothWalkingState.generated.h"

/**
 * Custom state data for LNPAsyncSmoothWalkingMode, 
 * mirroring the logic of the internal FSmoothWalkingState.
 */
USTRUCT(BlueprintType)
struct FLNPSmoothWalkingState : public FMoverDataStructBase
{
	GENERATED_BODY()
	
	FLNPSmoothWalkingState() = default;

	// FMoverDataStructBase Interface
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual FMoverDataStructBase* Clone() const override { return new FLNPSmoothWalkingState(*this); }
	
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
	{
		bool bSuperSuccess = Super::NetSerialize(Ar, Map, bOutSuccess);
		Ar << SpringVelocity;
		Ar << SpringAcceleration;
		Ar << IntermediateVelocity;
		Ar << IntermediateFacing;
		Ar << IntermediateAngularVelocity;
		return bSuperSuccess;
	}

	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override
	{
		const FLNPSmoothWalkingState* AuthState = static_cast<const FLNPSmoothWalkingState*>(&AuthorityState);
		
		return (!(SpringVelocity - AuthState->SpringVelocity).IsNearlyZero(10.0f) ||
				!(SpringAcceleration - AuthState->SpringAcceleration).IsNearlyZero(50.0f) ||
				!(IntermediateVelocity - AuthState->IntermediateVelocity).IsNearlyZero(10.0f) ||
				 (IntermediateFacing.AngularDistance(AuthState->IntermediateFacing) > FMath::DegreesToRadians(10.0f) ||
				!(IntermediateAngularVelocity - AuthState->IntermediateAngularVelocity).IsNearlyZero(10.0f)));
	}

	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override
	{
		const FLNPSmoothWalkingState* FromState = static_cast<const FLNPSmoothWalkingState*>(&From);
		const FLNPSmoothWalkingState* ToState = static_cast<const FLNPSmoothWalkingState*>(&To);

		SpringVelocity = FMath::Lerp(FromState->SpringVelocity, ToState->SpringVelocity, Pct);
		SpringAcceleration = FMath::Lerp(FromState->SpringAcceleration, ToState->SpringAcceleration, Pct);
		IntermediateVelocity = FMath::Lerp(FromState->IntermediateVelocity, ToState->IntermediateVelocity, Pct);
		IntermediateFacing = FQuat::Slerp(FromState->IntermediateFacing, ToState->IntermediateFacing, Pct);
		IntermediateAngularVelocity = FMath::Lerp(FromState->IntermediateAngularVelocity, ToState->IntermediateAngularVelocity, Pct);
	}

	// State Variables
	UPROPERTY(BlueprintReadOnly, Category = "Mover")
	FVector SpringVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Mover")
	FVector SpringAcceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Mover")
	FVector IntermediateVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Mover")
	FQuat IntermediateFacing = FQuat::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Mover")
	FVector IntermediateAngularVelocity = FVector::ZeroVector;
};
