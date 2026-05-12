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
#include "AbilitySystemComponent.h"
#include "MassAgentComponent.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"

#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPCharacterMovementSettings.h"
#include "GameMode/LNPGameState.h"
#include "Gravity/LNPPawnGravityComponent.h"
#include "Interaction/LNPInteractionComponent.h"
#include "Player/LNPPlayerState.h"
#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPItemInstance.h"

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
	CameraBoom->bInheritPitch = true;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = true;

	// 4. Create Follow Camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 5. Create Mover Component
	MoverComponent = CreateDefaultSubobject<ULNPCharacterMoverComponent>(TEXT("MoverComponent"));

	// 6. Create Mass Agent Component
	MassAgentComponent = CreateDefaultSubobject<UMassAgentComponent>(TEXT("MassAgentComponent"));

	// 8. Create Gravity Component
	GravityComponent = CreateDefaultSubobject<ULNPPawnGravityComponent>(TEXT("GravityComponent"));

	// 9. Create Interaction Component
	InteractionComponent = CreateDefaultSubobject<ULNPInteractionComponent>(TEXT("InteractionComponent"));

	// Disable default Pawn rotation logic to avoid interference with custom gravity system
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
}

UAbilitySystemComponent* ALNPCharacterBase::GetAbilitySystemComponent() const
{
	if (const ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>())
		return PS->GetAbilitySystemComponent();
	return nullptr;
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
}

void ALNPCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Forward look input to GravityComponent to handle orientation correctly in custom gravity
	if (!CachedLookInput.IsNearlyZero() && GravityComponent)
	{
		GravityComponent->InputLook(CachedLookInput);
	}

	// Reset cached look input after consumption
	CachedLookInput = FRotator::ZeroRotator;
}

void ALNPCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Add Input Mapping Context
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALNPCharacterBase::OnMoveTriggered);
		EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ALNPCharacterBase::OnMoveCompleted);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALNPCharacterBase::OnLookTriggered);
		EIC->BindAction(LookAction, ETriggerEvent::Completed, this, &ALNPCharacterBase::OnLookCompleted);
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ALNPCharacterBase::OnJumpStarted);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ALNPCharacterBase::OnJumpReleased);
		EIC->BindAction(DashAction, ETriggerEvent::Started, this, &ALNPCharacterBase::OnDashStarted);
		EIC->BindAction(DashAction, ETriggerEvent::Completed, this, &ALNPCharacterBase::OnDashReleased);
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ALNPCharacterBase::OnInteractStarted);
		EIC->BindAction(InteractAction, ETriggerEvent::Completed, this, &ALNPCharacterBase::OnInteractReleased);
		EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &ALNPCharacterBase::OnAttackStarted);
		EIC->BindAction(AttackAction, ETriggerEvent::Completed, this, &ALNPCharacterBase::OnAttackReleased);
	}
}

void ALNPCharacterBase::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	OnProduceInput((float)SimTimeMs, InputCmdResult);
}

void ALNPCharacterBase::OnProduceInput(float DeltaMs, FMoverInputCmdContext& OutInputCmd)
{
	FCharacterDefaultInputs& CharacterInputs = OutInputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	// If we have AI intent but no controller, we proceed. Otherwise check controller.
	const bool bHasAIIntent = !AIMoveInput.IsNearlyZero() || !AIOrientationIntent.IsNearlyZero();

	if (GetController() == nullptr && !bHasAIIntent)
	{
		return;
	}

	// 1. Capture Control Rotation (Relevant for players)
	if (GetController())
	{
		CharacterInputs.ControlRotation = GetControlRotation();
	}

	// 2. Handle Sprinting Intent
	if (ULNPCharacterMoverComponent* LNPMover = Cast<ULNPCharacterMoverComponent>(MoverComponent))
	{
		LNPMover->SetWantsToRun(bIsDashPressed);
	}

	if (GetController())
	{
		// --- Player Input Logic ---
		
		// 3. Calculate Move Intent projected onto the gravity surface
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

		// 4. Calculate Orientation Intent
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

		// Optional: Clear intents after use if you want one-shot behavior, 
		// but StateTree tasks usually update this every frame.
		// AIMoveInput = FVector::ZeroVector;
		// AIOrientationIntent = FVector::ZeroVector;
	}

	// 5. Jump Input
	CharacterInputs.bIsJumpPressed = bIsJumpPressed;
	CharacterInputs.bIsJumpJustPressed = bIsJumpJustPressed;
	// ... (Base-Relative Movement follows) ...


	// 6. Base-Relative Movement (Optional)
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

	// Reset one-time inputs
	bIsJumpJustPressed = false;
	bIsDashJustPressed = false;
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
	CachedLookInput.Yaw = LookVector.X;
	CachedLookInput.Pitch = LookVector.Y;
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

void ALNPCharacterBase::OnDashStarted(const FInputActionValue& Value)
{
	bIsDashJustPressed = !bIsDashPressed;
	bIsDashPressed = true;

	// Cooldown check
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (LastDashTime > 0.0f && (CurrentTime - LastDashTime) < DashCooldown)
	{
		return;
	}

	// Restricted during aiming or when not on ground
	if (bIsAiming || !MoverComponent || !MoverComponent->IsOnGround())
	{
		return;
	}

	const bool bHasMoveInput = !CachedMoveInputIntent.IsNearlyZero();

	TObjectPtr<UAnimMontage> SelectedMontage = nullptr;
	FVector DashDirection = FVector::ZeroVector;

	if (bHasMoveInput)
	{
		// 1. Calculate World Direction based on input and control rotation
		const FVector WorldInputMoveDir = GetControlRotation().RotateVector(CachedMoveInputIntent).GetSafeNormal();
		DashDirection = WorldInputMoveDir;

		// 2. Select Montage based on relative input direction
		// 캐릭터의 시야를 고정한 상태로 움직일 경우(ex> 조준 모드)
		// foward direction과 dash direction을 비교해서 Montage를 선택해야 하지만,
		// 현재는 캐릭터가 항상 이동 방향을 바라보므로 DashForwardMontage를 쓰면 된다.
		SelectedMontage = DashForwardMontage;
	}
	else
	{
		// 3. Dodge Logic: No input means dodge backwards
		DashDirection = -GetActorForwardVector();
		SelectedMontage = DashBackwardMontage;
	}

	if (SelectedMontage)
	{
		// Update Cooldown
		LastDashTime = CurrentTime;

		// 4. Create and Queue Layered Move for Physical Movement
		TSharedPtr<FLayeredMove_LinearVelocity> DashMove = MakeShared<FLayeredMove_LinearVelocity>();

		const float DashDurationMs = DashDuration * 1000.0f;

		DashMove->Velocity = DashDirection * DashImpulseMagnitude;
		DashMove->DurationMs = DashDurationMs;

		// Use Override to make it feel snappy
		DashMove->MixMode = EMoveMixMode::OverrideVelocity;

		// Apply Inertia: Maintain velocity on ground after dash ends
		DashMove->FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;

		MoverComponent->QueueLayeredMove(DashMove);

		// 5. Play Animation locally first to get the instance for perfect sync
		float ActualStartingPos = 0.0f;
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			AnimInstance->Montage_Play(SelectedMontage, 1.0f);
			if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(SelectedMontage))
			{
				ActualStartingPos = MontageInstance->GetPosition();
			}
		}

		// 6. Create and Queue Layered Move for Networked Animation Synchronization
		TSharedPtr<FLayeredMove_AnimRootMotion> AnimSyncMove = MakeShared<FLayeredMove_AnimRootMotion>();
		AnimSyncMove->MontageState.Montage = SelectedMontage;
		AnimSyncMove->MontageState.PlayRate = 1.0f;
		AnimSyncMove->MontageState.StartingMontagePosition = ActualStartingPos;
		AnimSyncMove->MontageState.CurrentPosition = ActualStartingPos;
		AnimSyncMove->DurationMs = DashDurationMs;

		MoverComponent->QueueLayeredMove(AnimSyncMove);
	}
}

void ALNPCharacterBase::OnDashReleased(const FInputActionValue& Value)
{
	bIsDashPressed = false;
	bIsDashJustPressed = false;
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

void ALNPCharacterBase::OnAttackStarted(const FInputActionValue& Value)
{
	bIsAttackJustPressed = !bIsAttackPressed;
	bIsAttackPressed = true;

	const ALNPPlayerState* PS = GetPlayerState<ALNPPlayerState>();
	if (nullptr == PS)
		return;

	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	if (nullptr == ASC)
		return;

	const ULNPEquipmentComponent* EqComp = PS->GetEquipmentComponent();
	if (nullptr == EqComp)
		return;

	const FLNPWeaponInstance& WeaponSlot = EqComp->GetWeaponSlot();
	if (!WeaponSlot.IsValid() || !WeaponSlot.GrantedAbilities.IsValidIndex(0))
		return;

	ASC->TryActivateAbility(WeaponSlot.GrantedAbilities[0]);
}

void ALNPCharacterBase::OnAttackReleased(const FInputActionValue& Value)
{
	bIsAttackPressed = false;
	bIsAttackJustPressed = false;
}