// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPAsyncSmoothWalkingMode.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Animation/SpringMath.h"
#include "MoveLibrary/GroundMovementUtils.h"
#include "MoveLibrary/MovementUtils.h"
#include "Movement/LNPCharacterMovementSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LNPAsyncSmoothWalkingMode)

ULNPAsyncSmoothWalkingMode::ULNPAsyncSmoothWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SharedSettingsClasses.Add(ULNPCharacterMovementSettings::StaticClass());
}

void ULNPAsyncSmoothWalkingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonLegacySettings = GetMoverComponent()->FindSharedSettings<UCommonLegacyMovementSettings>();
	ensureMsgf(CommonLegacySettings.IsValid(), TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}

void ULNPAsyncSmoothWalkingMode::OnUnregistered()
{
	CommonLegacySettings = nullptr;

	Super::OnUnregistered();
}

void ULNPAsyncSmoothWalkingMode::GenerateMove_Implementation(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	const UMoverComponent* MoverComp = GetMoverComponent();
	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	
	if (!CommonLegacySettings.IsValid() || !StartingSyncState)
	{
		return;
	}

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	if (DeltaSeconds <= FLT_EPSILON)
	{
		return;
	}

	// Calculate Desired Orientation based on input
	FRotator IntendedOrientation_WorldSpace;
	if (!CharacterInputs || CharacterInputs->OrientationIntent.IsNearlyZero())
	{
		IntendedOrientation_WorldSpace = StartingSyncState->GetOrientation_WorldSpace();
	}
	else
	{
		IntendedOrientation_WorldSpace = CharacterInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
	}
	IntendedOrientation_WorldSpace = UMovementUtils::ApplyGravityToOrientationIntent(IntendedOrientation_WorldSpace, MoverComp->GetWorldToGravityTransform(), CommonLegacySettings->bShouldRemainVertical);

	// Calculate Raw Desired Velocity (Target we want to reach)
	FVector DesiredVelocity = FVector::ZeroVector;
	if (CharacterInputs)
	{
		DesiredVelocity = CharacterInputs->GetMoveInput_WorldSpace() * CommonLegacySettings->MaxSpeed;
	}

	// Apply Smooth Spring Interpolation
	FVector SmoothedVelocity = StartingSyncState->GetVelocity_WorldSpace();
	FVector SmoothedAngularVelocity = StartingSyncState->GetAngularVelocityDegrees_WorldSpace();
	
	// Hack: We need a mutable StartState to add/update the SyncState collection for the spring state
	ApplySmoothing(const_cast<FMoverTickStartData&>(StartState), DeltaSeconds, DesiredVelocity, IntendedOrientation_WorldSpace.Quaternion(), StartingSyncState->GetOrientation_WorldSpace().Quaternion(), SmoothedAngularVelocity, SmoothedVelocity);

	// Populate Proposed Move with Smoothed values
	OutProposedMove.LinearVelocity = SmoothedVelocity;
	OutProposedMove.AngularVelocityDegrees = SmoothedAngularVelocity;
	
	if (CharacterInputs)
	{
		OutProposedMove.DirectionIntent = CharacterInputs->GetMoveInput_WorldSpace();
	}
	OutProposedMove.bHasDirIntent = !OutProposedMove.DirectionIntent.IsNearlyZero();
}

void ULNPAsyncSmoothWalkingMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	// Perform standard Async physical movement (collision, step-up, floor adjustment)
	Super::SimulationTick_Implementation(Params, OutputState);

	// Copy the updated spring state into the OutputSyncState so it persists to the next frame
	if (const FLNPSmoothWalkingState* InSpringState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FLNPSmoothWalkingState>())
	{
		FLNPSmoothWalkingState& OutputSpringState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FLNPSmoothWalkingState>();
		OutputSpringState = *InSpringState;
	}
}

void ULNPAsyncSmoothWalkingMode::ApplySmoothing(FMoverTickStartData& StartState, float DeltaSeconds, const FVector& DesiredVelocity, const FQuat& DesiredFacing, const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity) const
{
	if (DeltaSeconds <= FLT_EPSILON)
	{
		return;
	}

	// Ported and adapted logic from SmoothWalkingMode.cpp
	bool bSmoothWalkingStateAdded = false;
	FLNPSmoothWalkingState& SpringState = StartState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FLNPSmoothWalkingState>(bSmoothWalkingStateAdded);

	if (bSmoothWalkingStateAdded)
	{
		SpringState.SpringVelocity = InOutVelocity;
		SpringState.SpringAcceleration = FVector::ZeroVector;
		SpringState.IntermediateVelocity = InOutVelocity;
		SpringState.IntermediateFacing = CurrentFacing;
		SpringState.IntermediateAngularVelocity = FVector::ZeroVector;
	}

	// Outside Influence Reconcile
	const float VelocityMatch = FMath::Clamp(SpringState.SpringVelocity.Dot(InOutVelocity) / 
		FMath::Max(InOutVelocity.Length() * SpringState.SpringVelocity.Length(), UE_SMALL_NUMBER), 0.0f, 1.0f);

	FMath::ExponentialSmoothingApprox(SpringState.IntermediateVelocity, InOutVelocity, DeltaSeconds, 
		(OutsideInfluenceSmoothingTime + UE_KINDA_SMALL_NUMBER) / (1.0f - VelocityMatch));

	SpringState.SpringVelocity = InOutVelocity;

	// Rotate intermediate velocity towards target
	if (TurningStrength > 0.0f)
	{
		if (!DesiredVelocity.IsNearlyZero())
		{
			FMath::ExponentialSmoothingApprox(
				SpringState.IntermediateVelocity,
				DesiredVelocity.GetSafeNormal() * SpringState.IntermediateVelocity.Length(),
				DeltaSeconds,
				SpringMath::StrengthToSmoothingTime(TurningStrength));
		}
	}

	// Acceleration/Deceleration Logic
	const bool bIsAccelerating = (1.01f * DesiredVelocity.SquaredLength()) > SpringState.SpringVelocity.SquaredLength();
	const float LateralAccelerationMagnitude =  bIsAccelerating ? (1.0f - DirectionalAccelerationFactor) * Acceleration : Deceleration;
	const float DirectionalAccelerationMagnitude = bIsAccelerating ? DirectionalAccelerationFactor * Acceleration : 0.0f;

	const float PreviousVelocityLength = SpringState.IntermediateVelocity.Length();
	const FVector VelocityDifference = DesiredVelocity - SpringState.IntermediateVelocity;

	const FVector LateralAccelerationVector = VelocityDifference.GetSafeNormal() * FMath::Min(LateralAccelerationMagnitude, VelocityDifference.Length() / FMath::Max(DeltaSeconds, UE_SMALL_NUMBER));
	const FVector DirectionalAccelerationVector = DesiredVelocity.GetSafeNormal() * DirectionalAccelerationMagnitude;
	const FVector DesiredAcceleration = LateralAccelerationVector + DirectionalAccelerationVector;

	FVector NextVelocity = VelocityDifference.Dot(DesiredAcceleration * DeltaSeconds) < VelocityDifference.SquaredLength() ?
		SpringState.IntermediateVelocity + DesiredAcceleration * DeltaSeconds : DesiredVelocity;

	NextVelocity = NextVelocity.GetClampedToMaxSize(FMath::Max(PreviousVelocityLength, DesiredVelocity.Length()));

	const float VelocitySmoothingTime = bIsAccelerating ? AccelerationSmoothingTime : DecelerationSmoothingTime;
	const float VelocitySmoothingCompensation = bIsAccelerating ? AccelerationSmoothingCompensation : DecelerationSmoothingCompensation;
	const float LagSeconds = DeltaSeconds + (VelocitySmoothingCompensation * VelocitySmoothingTime);

	FVector TrackVelocity = VelocityDifference.Dot(DesiredAcceleration * LagSeconds) < VelocityDifference.SquaredLength() ?
		SpringState.IntermediateVelocity + DesiredAcceleration * LagSeconds : DesiredVelocity;

	TrackVelocity = TrackVelocity.GetClampedToMaxSize(FMath::Max(PreviousVelocityLength, DesiredVelocity.Length()));

	// Spring Integration
	SpringMath::CriticalSpringDamper(SpringState.SpringVelocity, SpringState.SpringAcceleration, TrackVelocity, VelocitySmoothingTime, DeltaSeconds);

	if ((DesiredVelocity - SpringState.SpringVelocity).SquaredLength() < FMath::Square(VelocityDeadzoneThreshold))
	{
		SpringState.SpringVelocity = DesiredVelocity;
		if (SpringState.SpringAcceleration.SquaredLength() < FMath::Square(AccelerationDeadzoneThreshold))
		{
			SpringState.SpringAcceleration = FVector::ZeroVector;
		}
	}

	InOutVelocity = SpringState.SpringVelocity;
	SpringState.IntermediateVelocity = NextVelocity;

	// Facing Smoothing
	FVector CurrentAngularVelocityRadians = FMath::DegreesToRadians(InOutAngularVelocityDegrees);
	FQuat UpdatedFacing = CurrentFacing;

	if (bSmoothFacingWithDoubleSpring)
	{
		SpringMath::CriticalSpringDamperQuat(SpringState.IntermediateFacing, SpringState.IntermediateAngularVelocity, DesiredFacing, FacingSmoothingTime / 2.0f, DeltaSeconds);
		SpringMath::CriticalSpringDamperQuat(UpdatedFacing, CurrentAngularVelocityRadians, SpringState.IntermediateFacing, FacingSmoothingTime / 2.0f, DeltaSeconds);
	}
	else
	{
		SpringState.IntermediateFacing = DesiredFacing;
		SpringState.IntermediateAngularVelocity = CurrentAngularVelocityRadians;
		SpringMath::CriticalSpringDamperQuat(UpdatedFacing, CurrentAngularVelocityRadians, DesiredFacing, FacingSmoothingTime, DeltaSeconds);
	}

	if (DesiredFacing.AngularDistance(UpdatedFacing) < FMath::DegreesToRadians(FacingDeadzoneThreshold))
	{
		CurrentAngularVelocityRadians = DeltaSeconds > 0.0f ? ((CurrentFacing.Inverse() * UpdatedFacing).GetShortestArcWith(FQuat::Identity)).ToRotationVector() / DeltaSeconds : FVector::ZeroVector;
		SpringState.IntermediateFacing = DesiredFacing;

		if (CurrentAngularVelocityRadians.SquaredLength() < FMath::Square(FMath::DegreesToRadians(AngularVelocityDeadzoneThreshold)))
		{
			SpringState.IntermediateAngularVelocity = FVector::ZeroVector;
		}
	}

	InOutAngularVelocityDegrees = FMath::RadiansToDegrees(CurrentAngularVelocityRadians);
}
