// Copyright Epic Games, Inc. All Rights Reserved.

#include "LNPCharacterBase.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "AbilitySystemComponent.h"
#include "Gameplay/LNPGameState.h"
#include "MassAgentComponent.h"
#include "Component/LNPPawnGravityComponent.h"
#include "Component/LNPInteractionComponent.h"

ALNPCharacterBase::ALNPCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// Disable Actor-level movement replication (handled by Mover component)
	SetReplicatingMovement(false);

	// 1. Create Capsule Component
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	CapsuleComponent->InitCapsuleSize(42.f, 96.0f);
	SetRootComponent(CapsuleComponent);

	// 2. Create Mesh Component
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(CapsuleComponent);

	// 3. Create Camera Boom
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Mesh);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// 4. Create Follow Camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 5. Create Mover Component
	MoverComponent = CreateDefaultSubobject<UCharacterMoverComponent>(TEXT("MoverComponent"));

	// 6. Create GAS Ability System Component
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// 7. Create Mass Agent Component
	MassAgentComponent = CreateDefaultSubobject<UMassAgentComponent>(TEXT("MassAgentComponent"));

	// 8. Create Gravity Component
	GravityComponent = CreateDefaultSubobject<ULNPPawnGravityComponent>(TEXT("GravityComponent"));

	// 9. Create Interaction Component
	InteractionComponent = CreateDefaultSubobject<ULNPInteractionComponent>(TEXT("InteractionComponent"));
}

UAbilitySystemComponent* ALNPCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

TArray<AActor*> ALNPCharacterBase::GetInteractionCandidates() const
{
	return InteractionComponent ? InteractionComponent->GetInteractionCandidates() : TArray<AActor*>();
}

AActor* ALNPCharacterBase::GetInteractionCandidate() const
{
	return InteractionComponent ? InteractionComponent->GetFirstInteractionCandidate() : nullptr;
}

void ALNPCharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void ALNPCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Add Input Mapping Context
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}

		PC->PlayerCameraManager->ViewPitchMax = 89.0f;
		PC->PlayerCameraManager->ViewPitchMin = -89.0f;
	}
}

void ALNPCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ToDo
	// Input 처리를 GravityComponent에서 하고 있으니 이상하다.
	// GravityComponent의 CachedLookInput를 제거하고,
	// ULNPPawnGravityComponent::UpdateControllerOrientation()에서는 플레이어 컨트롤러의 ControlRotation만 처리하고,
	// 여기서 AddControllerPitchInput(), AddControllerYawInput()를 실행하도록 수정하자.
	if (GravityComponent != nullptr)
		GravityComponent->InputLook(CachedLookInput);

	// Reset cached look input after consumption
	CachedLookInput = FRotator::ZeroRotator;
}

void ALNPCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALNPCharacterBase::OnMoveTriggered);
		EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ALNPCharacterBase::OnMoveCompleted);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALNPCharacterBase::OnLookTriggered);
		EIC->BindAction(LookAction, ETriggerEvent::Completed, this, &ALNPCharacterBase::OnLookCompleted);
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ALNPCharacterBase::OnJumpStarted);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ALNPCharacterBase::OnJumpReleased);
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ALNPCharacterBase::OnInteractStarted);
		EIC->BindAction(InteractAction, ETriggerEvent::Completed, this, &ALNPCharacterBase::OnInteractReleased);
	}
}

void ALNPCharacterBase::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	OnProduceInput((float)SimTimeMs, InputCmdResult);
}

void ALNPCharacterBase::OnProduceInput(float DeltaMs, FMoverInputCmdContext& OutInputCmd)
{
	FCharacterDefaultInputs& CharacterInputs = OutInputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	if (GetController() == nullptr)
	{
		return;
	}

	// 1. Capture Control Rotation
	CharacterInputs.ControlRotation = GetControlRotation();

	// 2. Calculate Move Intent
	// Transform move vector to world space based on control rotation
	const FVector FinalDirectionalIntent = CharacterInputs.ControlRotation.RotateVector(CachedMoveInputIntent);
	CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FinalDirectionalIntent);

	// 3. Calculate Orientation Intent
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
			CharacterInputs.OrientationIntent = CharacterInputs.ControlRotation.Vector().GetSafeNormal();
		}
		LastAffirmativeMoveInput = CharacterInputs.GetMoveInput();
	}
	else if (bMaintainLastInputOrientation)
	{
		CharacterInputs.OrientationIntent = LastAffirmativeMoveInput;
	}

	// 4. Jump Input
	CharacterInputs.bIsJumpPressed = bIsJumpPressed;
	CharacterInputs.bIsJumpJustPressed = bIsJumpJustPressed;

	// 5. Base-Relative Movement (Optional)
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

	// Reset one-time inputs (e.g., JumpJustPressed)
	bIsJumpJustPressed = false;
}

// --- Input Event Implementation ---

void ALNPCharacterBase::OnMoveTriggered(const FInputActionValue& Value)
{
	const FVector MovementVector = Value.Get<FVector>();
	CachedMoveInputIntent.X = FMath::Clamp(MovementVector.X, -1.0f, 1.0f);
	CachedMoveInputIntent.Y = FMath::Clamp(MovementVector.Y, -1.0f, 1.0f);
	CachedMoveInputIntent.Z = FMath::Clamp(MovementVector.Z, -1.0f, 1.0f);

	//UE_LOG(LogTemp, Log, TEXT("Move Input: X=%.2f, Y=%.2f, Z=%.2f"), CachedMoveInputIntent.X, CachedMoveInputIntent.Y, CachedMoveInputIntent.Z);
}

void ALNPCharacterBase::OnMoveCompleted(const FInputActionValue& Value)
{
	CachedMoveInputIntent = FVector::ZeroVector;
}

void ALNPCharacterBase::OnLookTriggered(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
	CachedLookInput.Yaw = FMath::Clamp(LookVector.X, -1.0f, 1.0f);
	CachedLookInput.Pitch = FMath::Clamp(LookVector.Y, -1.0f, 1.0f);

	//UE_LOG(LogTemp, Log, TEXT("Look Input: Yaw=%.2f, Pitch=%.2f"), CachedLookInput.Yaw, CachedLookInput.Pitch);
}

void ALNPCharacterBase::OnLookCompleted(const FInputActionValue& Value)
{
	CachedLookInput = FRotator::ZeroRotator;
}

void ALNPCharacterBase::OnJumpStarted(const FInputActionValue& Value)
{
	bIsJumpJustPressed = !bIsJumpPressed;
	bIsJumpPressed = true;
}

void ALNPCharacterBase::OnJumpReleased(const FInputActionValue& Value)
{
	bIsJumpPressed = false;
	bIsJumpJustPressed = false;
}

void ALNPCharacterBase::OnInteractStarted(const FInputActionValue& Value)
{
	bIsInteractJustPressed = !bIsInteractPressed;
	bIsInteractPressed = true;

	if (InteractionComponent != nullptr)
	{
		InteractionComponent->PerformInteraction();
	}
}

void ALNPCharacterBase::OnInteractReleased(const FInputActionValue& Value)
{
	bIsInteractPressed = false;
	bIsInteractJustPressed = false;
}

