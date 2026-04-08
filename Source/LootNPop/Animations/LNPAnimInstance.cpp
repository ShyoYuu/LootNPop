// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animations/LNPAnimInstance.h"
#include "Player/LNPCharacterBase.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "KismetAnimationLibrary.h"

void ULNPAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// Cache references for better performance
	OwningMoverCharacter = Cast<ALNPCharacterBase>(TryGetPawnOwner());
	if (OwningMoverCharacter != nullptr)
	{
		MoverComponent = OwningMoverCharacter->GetMoverComponent();
	}
}

void ULNPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Try to cache character if it hasn't been cached yet (delayed initialization)
	if (OwningMoverCharacter == nullptr)
	{
		OwningMoverCharacter = Cast<ALNPCharacterBase>(TryGetPawnOwner());

		if (MoverComponent == nullptr && OwningMoverCharacter != nullptr)
		{
			MoverComponent = OwningMoverCharacter->GetMoverComponent();
		}
	}

	if (OwningMoverCharacter == nullptr || MoverComponent == nullptr)
	{
		return;
	}

	// 1. Update Velocity and Ground Speed
	Velocity = MoverComponent->GetVelocity();
	
	/**
	 * Spherical World Fix:
	 * Size2D() only works in world XY plane. 
	 * We must project velocity onto the plane perpendicular to the character's local Up vector.
	 */
	FVector CharacterUp = OwningMoverCharacter->GetActorUpVector();
	FVector GroundVelocity = FVector::VectorPlaneProject(Velocity, CharacterUp);
	GroundSpeed = GroundVelocity.Size();

	// 2. Determine if the character should move
	// Logic: Character has enough horizontal speed and intentional movement input
	bShouldMove = (3.0f < GroundSpeed) && (!MoverComponent->GetMovementIntent().IsZero());

	// 3. Air and Falling State via Mover Component
	bIsAirborne = MoverComponent->IsAirborne();
	bIsFalling = MoverComponent->IsFalling();
	bIsSwimming = MoverComponent->IsSwimming();

	// 4. Calculate Movement Direction
	Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, OwningMoverCharacter->GetActorRotation());

	// 5. Determine if the character should strafe
	// If the character doesn't rotate to movement, it's likely strafing (e.g., looking at a target while moving).
	bShouldStrafe = !OwningMoverCharacter->GetOrientRotationToMovement();
}
