// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/LNPPawnInputComponent.h"
#include "Character/LNPCharacterBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/Pawn.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "Player/LNPPlayerController.h"
#include "LootNPop.h"

#include "Movement/LNPCharacterMoverComponent.h"
#include "Gravity/LNPPawnGravityComponent.h"
#include "Interaction/LNPInteractionComponent.h"

ULNPPawnInputComponent::ULNPPawnInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULNPPawnInputComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	MoverComponent = Owner->FindComponentByClass<ULNPCharacterMoverComponent>();
	GravityComponent = Owner->FindComponentByClass<ULNPPawnGravityComponent>();
	InteractionComponent = Owner->FindComponentByClass<ULNPInteractionComponent>();
}

void ULNPPawnInputComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CachedLookInput.IsNearlyZero() && GravityComponent)
	{
		GravityComponent->InputLook(CachedLookInput);
	}
	CachedLookInput = FRotator::ZeroRotator;

	if (bIsDashBuffered)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - DashBufferTime > 0.05f)
			bIsDashBuffered = false;
		else if (MoverComponent && MoverComponent->CanDash())
		{
			bIsDashBuffered = false;
			MoverComponent->ExecuteDash(CachedMoveInputIntent);
		}
	}

	if (bIsAttackBuffered)
	{
		ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(GetOwner());
		if (!Character || 0.05f < (GetWorld()->GetTimeSeconds() - AttackBufferTime) || Character->TryActivateAttack())
			bIsAttackBuffered = false;
	}
}

void ULNPPawnInputComponent::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	APawn* Pawn = CastChecked<APawn>(GetOwner());

	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ULNPPawnInputComponent::OnMoveTriggered);
		EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ULNPPawnInputComponent::OnMoveCompleted);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ULNPPawnInputComponent::OnLookTriggered);
		EIC->BindAction(LookAction, ETriggerEvent::Completed, this, &ULNPPawnInputComponent::OnLookCompleted);
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ULNPPawnInputComponent::OnJumpStarted);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ULNPPawnInputComponent::OnJumpReleased);
		EIC->BindAction(DashAction, ETriggerEvent::Started, this, &ULNPPawnInputComponent::OnDashStarted);
		EIC->BindAction(DashAction, ETriggerEvent::Completed, this, &ULNPPawnInputComponent::OnDashReleased);
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ULNPPawnInputComponent::OnInteractStarted);
		EIC->BindAction(InteractAction, ETriggerEvent::Completed, this, &ULNPPawnInputComponent::OnInteractReleased);
		EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &ULNPPawnInputComponent::OnAttackStarted);
		EIC->BindAction(AttackAction, ETriggerEvent::Completed, this, &ULNPPawnInputComponent::OnAttackReleased);
	}
}

void ULNPPawnInputComponent::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	OnProduceInput((float)SimTimeMs, InputCmdResult);
}

void ULNPPawnInputComponent::OnProduceInput(float DeltaMs, FMoverInputCmdContext& OutInputCmd)
{
	APawn* Pawn = CastChecked<APawn>(GetOwner());
	FCharacterDefaultInputs& CharacterInputs = OutInputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	const bool bHasAIIntent = !AIMoveInput.IsNearlyZero() || !AIOrientationIntent.IsNearlyZero();

	if (Pawn->GetController() == nullptr && !bHasAIIntent)
	{
		return;
	}

	if (Pawn->GetController())
	{
		CharacterInputs.ControlRotation = Pawn->GetControlRotation();
	}

	if (MoverComponent)
	{
		MoverComponent->SetWantsToRun(bIsDashPressed);
	}

	if (Pawn->GetController())
	{
		// --- Player Input Logic ---

		FVector UpDir = GravityComponent ? GravityComponent->GetUpDirection() : FVector::UpVector;
		FQuat ControlQuat = CharacterInputs.ControlRotation.Quaternion();
		FVector ControlForward = ControlQuat.GetForwardVector();

		FVector RightDir = FVector::CrossProduct(UpDir, ControlForward).GetSafeNormal();
		if (RightDir.IsNearlyZero())
		{
			RightDir = FVector::CrossProduct(UpDir, ControlQuat.GetUpVector()).GetSafeNormal();
		}
		FVector HorizonForward = FVector::CrossProduct(RightDir, UpDir).GetSafeNormal();

		const FVector FinalDirectionalIntent = (HorizonForward * CachedMoveInputIntent.X) + (RightDir * CachedMoveInputIntent.Y);
		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FinalDirectionalIntent);

		static float RotationMagMin(1e-3);
		const bool bHasAffirmativeMoveInput = (CharacterInputs.GetMoveInput().Size() >= RotationMagMin);

		CharacterInputs.OrientationIntent = FVector::ZeroVector;

		if (bHasAffirmativeMoveInput)
		{
			if (bOrientRotationToMovement)
			{
				CharacterInputs.OrientationIntent = CharacterInputs.GetMoveInput().GetSafeNormal();
			}
			else
			{
				CharacterInputs.OrientationIntent = HorizonForward;
			}
			LastAffirmativeMoveInput = CharacterInputs.GetMoveInput();
		}
		else if (bMaintainLastInputOrientation)
		{
			CharacterInputs.OrientationIntent = LastAffirmativeMoveInput;
		}
	}
	else
	{
		// --- AI Intent Logic (StateTree) ---

		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, AIMoveInput);

		if (!AIOrientationIntent.IsNearlyZero())
		{
			CharacterInputs.OrientationIntent = AIOrientationIntent;
		}
		else if (!AIMoveInput.IsNearlyZero())
		{
			CharacterInputs.OrientationIntent = AIMoveInput.GetSafeNormal();
		}
	}

	CharacterInputs.bIsJumpPressed = bIsJumpPressed;
	CharacterInputs.bIsJumpJustPressed = bIsJumpJustPressed;

	CharacterInputs.bUsingMovementBase = false;
	if (bUseBaseRelativeMovement && MoverComponent)
	{
		if (UPrimitiveComponent* MovementBase = MoverComponent->GetMovementBase())
		{
			FName MovementBaseBoneName = MoverComponent->GetMovementBaseBoneName();
			FVector RelativeMoveInput, RelativeOrientDir;

			UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterInputs.GetMoveInput(), RelativeMoveInput);
			UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterInputs.OrientationIntent, RelativeOrientDir);

			CharacterInputs.SetMoveInput(CharacterInputs.GetMoveInputType(), RelativeMoveInput);
			CharacterInputs.OrientationIntent = RelativeOrientDir;

			CharacterInputs.bUsingMovementBase = true;
			CharacterInputs.MovementBase = MovementBase;
			CharacterInputs.MovementBaseBoneName = MovementBaseBoneName;
		}
	}

	bIsJumpJustPressed = false;
	bIsDashJustPressed = false;
	bIsInteractJustPressed = false;
	bIsAttackJustPressed = false;
}

// --- Input Event Implementation ---

void ULNPPawnInputComponent::OnMoveTriggered(const FInputActionValue& Value)
{
	const FVector MovementVector = Value.Get<FVector>();
	CachedMoveInputIntent.X = FMath::Clamp(MovementVector.X, -1.0f, 1.0f);
	CachedMoveInputIntent.Y = FMath::Clamp(MovementVector.Y, -1.0f, 1.0f);
	CachedMoveInputIntent.Z = FMath::Clamp(MovementVector.Z, -1.0f, 1.0f);
}

void ULNPPawnInputComponent::OnMoveCompleted(const FInputActionValue& Value)
{
	CachedMoveInputIntent = FVector::ZeroVector;
}

void ULNPPawnInputComponent::OnLookTriggered(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
	CachedLookInput.Yaw = LookVector.X;
	CachedLookInput.Pitch = LookVector.Y;
}

void ULNPPawnInputComponent::OnLookCompleted(const FInputActionValue& Value)
{
	CachedLookInput = FRotator::ZeroRotator;
}

void ULNPPawnInputComponent::OnJumpStarted(const FInputActionValue& Value)
{
	bIsJumpJustPressed = !bIsJumpPressed;
	bIsJumpPressed = true;
}

void ULNPPawnInputComponent::OnJumpReleased(const FInputActionValue& Value)
{
	bIsJumpPressed = false;
	bIsJumpJustPressed = false;
}

void ULNPPawnInputComponent::OnDashStarted(const FInputActionValue& Value)
{
	bIsDashJustPressed = !bIsDashPressed;
	bIsDashPressed = true;

	if (MoverComponent && MoverComponent->CanDash())
	{
		MoverComponent->ExecuteDash(CachedMoveInputIntent);
	}
	else
	{
		bIsDashBuffered = true;
		DashBufferTime = GetWorld()->GetTimeSeconds();
	}
}

void ULNPPawnInputComponent::OnDashReleased(const FInputActionValue& Value)
{
	bIsDashPressed = false;
	bIsDashJustPressed = false;
}

void ULNPPawnInputComponent::OnInteractStarted(const FInputActionValue& Value)
{
	bIsInteractJustPressed = !bIsInteractPressed;
	bIsInteractPressed = true;

	if (InteractionComponent)
	{
		InteractionComponent->PerformInteraction();
	}
}

void ULNPPawnInputComponent::OnInteractReleased(const FInputActionValue& Value)
{
	bIsInteractPressed = false;
	bIsInteractJustPressed = false;
}

void ULNPPawnInputComponent::OnAttackStarted(const FInputActionValue& Value)
{
	bIsAttackJustPressed = !bIsAttackPressed;
	bIsAttackPressed = true;

	ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(GetOwner());
	if (!Character || !Character->TryActivateAttack())
	{
		bIsAttackBuffered = true;
		AttackBufferTime = GetWorld()->GetTimeSeconds();
	}
}

void ULNPPawnInputComponent::OnAttackReleased(const FInputActionValue& Value)
{
	bIsAttackPressed = false;
	bIsAttackJustPressed = false;
}
