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

	// Establish tick order: GravityComponent updates first, then Character/Pawn
	GetOwner()->AddTickPrerequisiteComponent(this);
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
		PawnUpDir = (GravityOrigin - OwnerPawn->GetActorLocation()).GetSafeNormal();
		PawnDownDir = -PawnUpDir;
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

void ULNPPawnGravityComponent::UpdatePawnOrientation(const FVector& PawnUpDir, const FVector& PawnDownDir)
{
	const FVector CustomGravityVector = PawnDownDir * GravityStrength;
	
	CachedMoverComponent->SetGravityOverride(true, CustomGravityVector);
	CachedMoverComponent->SetUpDirectionOverride(true, PawnUpDir);

	// Align the actor's vertical axis with the surface normal
	const FQuat CurrentQuat = GetOwner()->GetActorQuat();
	if (!CurrentQuat.GetUpVector().Equals(PawnUpDir, 0.001f))
	{
		const FQuat AlignmentDelta = FQuat::FindBetweenNormals(CurrentQuat.GetUpVector(), PawnUpDir);
		GetOwner()->SetActorRotation(AlignmentDelta * CurrentQuat);
	}
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
	
	// 1. Compensate for surface curvature by adjusting the entire view frame
	if (!LastUpDir.Equals(TargetUpDir, 0.001f))
	{
		const FQuat CurvatureDelta = FQuat::FindBetweenNormals(LastUpDir, TargetUpDir);
		CurrentControlQuat = CurvatureDelta * CurrentControlQuat;
	}

	// 2. Stabilization & Pitch Clamping
	FVector ViewForward = CurrentControlQuat.GetForwardVector();
	const float CosAngleFromUp = FVector::DotProduct(ViewForward, TargetUpDir);
	
	const float MaxCosLimit = 0.996f;  // Approx 85 degrees up/down limit
	const float MinCosLimit = -0.996f;

	if (CosAngleFromUp > MaxCosLimit || CosAngleFromUp < MinCosLimit)
	{
		const float ClampedCos = FMath::Clamp(CosAngleFromUp, MinCosLimit, MaxCosLimit);
		const float ClampedSin = FMath::Sqrt(1.0f - (ClampedCos * ClampedCos));
		const FVector HorizonForward = FVector::VectorPlaneProject(ViewForward, TargetUpDir).GetSafeNormal();

		ViewForward = (HorizonForward * ClampedSin) + (TargetUpDir * ClampedCos);
	}

	// 4. Final Reconstruction: Ensure vertical stabilization relative to Gravity
	// MakeFromXZ ensures that Z (Up) is exactly aligned with TargetUpDir, removing roll relative to gravity.
	const FRotator FinalControlRotation = FRotationMatrix::MakeFromXZ(ViewForward, TargetUpDir).Rotator();
	PC->SetControlRotation(FinalControlRotation);
}
