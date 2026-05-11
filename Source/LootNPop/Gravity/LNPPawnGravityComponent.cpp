// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gravity/LNPPawnGravityComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameMode/LNPGameState.h"

ULNPPawnGravityComponent::ULNPPawnGravityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULNPPawnGravityComponent::BeginPlay()
{
	Super::BeginPlay();

	// 1. Cache the MoverComponent from the owning actor
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		CachedMoverComponent = OwnerPawn->FindComponentByClass<UCharacterMoverComponent>();
	}

	// 2. Initial synchronization with GameState settings
	if (ALNPGameState* GS = GetWorld()->GetGameState<ALNPGameState>())
	{
		if (GS->bIsSphereWorld)
			SetGravity(ELNPGravityType::RadialOutward, FVector::ZeroVector);
		else
			SetGravity(ELNPGravityType::Fixed, FVector::DownVector);
	}

	AddTickPrerequisiteActor(GetOwner());
	LastGravityType = GravityType;
	LastUpDir = FVector::UpVector;
}

void ULNPPawnGravityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CachedMoverComponent == nullptr)
		return;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr)
		return;

	FVector PawnUpDir = FVector::UpVector;
	FVector PawnDownDir = FVector::DownVector;

	// Calculate target surface normal and gravity direction based on current mode
	switch (GravityType)
	{
	case ELNPGravityType::Fixed:
		PawnDownDir = FixedGravityDirection.GetSafeNormal();
		PawnUpDir = -PawnDownDir;
		break;

	case ELNPGravityType::RadialInward:
		PawnDownDir = (GravityOrigin - OwnerPawn->GetActorLocation()).GetSafeNormal();
		PawnUpDir = -PawnDownDir;
		break;

	case ELNPGravityType::RadialOutward:
		PawnDownDir = (OwnerPawn->GetActorLocation() - GravityOrigin).GetSafeNormal();
		PawnUpDir = -PawnDownDir;
		break;

	default:
		return;
	}

	// 1. Detect Mode Transitions
	if (LastGravityType != GravityType)
	{
		// Force initial physical update on transition
		UpdatePawnOrientation(PawnUpDir, PawnDownDir);
		LastGravityType = GravityType;
	}

	// 2. Update orientations if needed (Direction changed or it's a dynamic gravity world)
	// For Dynamic modes, this always runs to ensure smooth transition as pawn moves.
	const bool bDirectionChanged = !LastUpDir.Equals(PawnUpDir, 0.001f);

	if (GravityType != ELNPGravityType::Fixed || bDirectionChanged)
	{
		UpdatePawnOrientation(PawnUpDir, PawnDownDir);
		UpdateControllerOrientation(DeltaTime, PawnUpDir);
	}

	LastUpDir = PawnUpDir;
}

void ULNPPawnGravityComponent::SetGravity(const ELNPGravityType NewType, const FVector& NewDirectionOrOrigin)
{
	GravityType = NewType;
	if (NewType == ELNPGravityType::Fixed)
	{
		FixedGravityDirection = NewDirectionOrOrigin.GetSafeNormal();
	}
	else
	{
		GravityOrigin = NewDirectionOrOrigin;
	}
}

FVector ULNPPawnGravityComponent::GetUpDirection() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::UpVector;
	}

	switch (GravityType)
	{
	case ELNPGravityType::Fixed:
		return -FixedGravityDirection.GetSafeNormal();

	case ELNPGravityType::RadialInward:
		return (Owner->GetActorLocation() - GravityOrigin).GetSafeNormal();

	case ELNPGravityType::RadialOutward:
		return (GravityOrigin - Owner->GetActorLocation()).GetSafeNormal();
	}

	return FVector::UpVector;
}

void ULNPPawnGravityComponent::InputLook(const FRotator& LookDelta)
{
	PendingLookInput += LookDelta;
}

void ULNPPawnGravityComponent::UpdatePawnOrientation(const FVector& PawnUpDir, const FVector& PawnDownDir)
{
	const FVector CustomGravityVector = PawnDownDir * GravityStrength;
	
	CachedMoverComponent->SetGravityOverride(true, CustomGravityVector);
	CachedMoverComponent->SetUpDirectionOverride(true, PawnUpDir);

	// We no longer call SetActorRotation here. 
	// The Mover component handles capsule orientation smoothly via UpDirectionOverride.
}

void ULNPPawnGravityComponent::UpdateControllerOrientation(float DeltaTime, const FVector& TargetUpDir)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr || OwnerPawn->GetController() == nullptr)
		return;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (PC == nullptr)
		return;

	FQuat CurrentControlQuat = PC->GetControlRotation().Quaternion();
	
	// 1. Curvature Compensation
	if (!LastUpDir.Equals(TargetUpDir, 0.0001f))
	{
		const FQuat CurvatureDelta = FQuat::FindBetweenNormals(LastUpDir, TargetUpDir);
		CurrentControlQuat = CurvatureDelta * CurrentControlQuat;
	}

	// 2. Apply Look Input (Yaw and Pitch)
	if (!PendingLookInput.IsNearlyZero())
	{
		// Yaw: Rotate around the local Up axis
		const FQuat YawQuat(TargetUpDir, FMath::DegreesToRadians(PendingLookInput.Yaw * 2.0));
		CurrentControlQuat = YawQuat * CurrentControlQuat;

		// Pitch: Rotate around the local Right axis
		const FVector CurrentRight = FVector::CrossProduct(TargetUpDir, CurrentControlQuat.GetForwardVector()).GetSafeNormal();
		if (!CurrentRight.IsNearlyZero())
		{
			const FQuat PitchQuat(CurrentRight, FMath::DegreesToRadians(-PendingLookInput.Pitch * 2.0));
			CurrentControlQuat = PitchQuat * CurrentControlQuat;
		}

		PendingLookInput = FRotator::ZeroRotator;
	}

	// 3. Stabilization & Pitch Clamping relative to local Up
	FVector ViewForward = CurrentControlQuat.GetForwardVector();
	const float CosAngleFromUp = FVector::DotProduct(ViewForward, TargetUpDir);
	
	const float MaxCosLimit = 0.996f;  // Approx 85 degrees
	const float MinCosLimit = -0.996f;

	if (CosAngleFromUp > MaxCosLimit || CosAngleFromUp < MinCosLimit)
	{
		const float ClampedCos = FMath::Clamp(CosAngleFromUp, MinCosLimit, MaxCosLimit);
		const float ClampedSin = FMath::Sqrt(1.0f - (ClampedCos * ClampedCos));
		const FVector HorizonForward = FVector::VectorPlaneProject(ViewForward, TargetUpDir).GetSafeNormal();

		ViewForward = (HorizonForward * ClampedSin) + (TargetUpDir * ClampedCos);
	}

	// 4. Final Reconstruction: Preserve ViewForward while forcing a stable Right axis relative to Gravity
	// This keeps the Pitch intact while ensuring the camera doesn't roll relative to the surface.
	FVector FinalRight = FVector::CrossProduct(TargetUpDir, ViewForward).GetSafeNormal();
	FVector FinalUp = FVector::CrossProduct(ViewForward, FinalRight).GetSafeNormal();
	
	const FRotator FinalControlRotation = FMatrix(ViewForward, FinalRight, FinalUp, FVector::ZeroVector).Rotator();
	PC->SetControlRotation(FinalControlRotation);
}
