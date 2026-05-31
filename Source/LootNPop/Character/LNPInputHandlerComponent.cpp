// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/LNPInputHandlerComponent.h"
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
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "LNPGameplayTags.h"

ULNPInputHandlerComponent::ULNPInputHandlerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULNPInputHandlerComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	MoverComponent = Owner->FindComponentByClass<ULNPCharacterMoverComponent>();
	GravityComponent = Owner->FindComponentByClass<ULNPPawnGravityComponent>();
	InteractionComponent = Owner->FindComponentByClass<ULNPInteractionComponent>();

	ActiveSkillPressed.SetNum(ActiveSkillActions.Num());
	ActiveSkillJustPressed.SetNum(ActiveSkillActions.Num());

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		ASC = ASI->GetAbilitySystemComponent();
	}
}

void ULNPInputHandlerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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

void ULNPInputHandlerComponent::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ULNPInputHandlerComponent::OnMoveTriggered);
		EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ULNPInputHandlerComponent::OnMoveCompleted);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ULNPInputHandlerComponent::OnLookTriggered);
		EIC->BindAction(LookAction, ETriggerEvent::Completed, this, &ULNPInputHandlerComponent::OnLookCompleted);
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ULNPInputHandlerComponent::OnJumpStarted);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ULNPInputHandlerComponent::OnJumpReleased);
		EIC->BindAction(DashAction, ETriggerEvent::Started, this, &ULNPInputHandlerComponent::OnDashStarted);
		EIC->BindAction(DashAction, ETriggerEvent::Completed, this, &ULNPInputHandlerComponent::OnDashReleased);
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ULNPInputHandlerComponent::OnInteractStarted);
		EIC->BindAction(InteractAction, ETriggerEvent::Completed, this, &ULNPInputHandlerComponent::OnInteractReleased);
		EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &ULNPInputHandlerComponent::OnAttackStarted);
		EIC->BindAction(AttackAction, ETriggerEvent::Completed, this, &ULNPInputHandlerComponent::OnAttackReleased);
		EIC->BindAction(GuardAction, ETriggerEvent::Started, this, &ULNPInputHandlerComponent::OnGuardStarted);
		EIC->BindAction(GuardAction, ETriggerEvent::Completed, this, &ULNPInputHandlerComponent::OnGuardReleased);
		EIC->BindAction(LockOnAction, ETriggerEvent::Started, this, &ULNPInputHandlerComponent::OnLockOnStarted);
		EIC->BindAction(LockOnAction, ETriggerEvent::Completed, this, &ULNPInputHandlerComponent::OnLockOnReleased);

		for (int32 i = 0; i < ActiveSkillActions.Num(); ++i)
		{
			if (ActiveSkillActions[i])
			{
				EIC->BindAction(ActiveSkillActions[i], ETriggerEvent::Started, this, &ULNPInputHandlerComponent::OnActiveSkillStarted, i);
				EIC->BindAction(ActiveSkillActions[i], ETriggerEvent::Completed, this, &ULNPInputHandlerComponent::OnActiveSkillReleased, i);
			}
		}
	}
}

void ULNPInputHandlerComponent::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	OnProduceInput((float)SimTimeMs, InputCmdResult);
}

void ULNPInputHandlerComponent::OnProduceInput(float DeltaMs, FMoverInputCmdContext& OutInputCmd)
{
	APawn* Pawn = CastChecked<APawn>(GetOwner());
	FCharacterDefaultInputs& CharacterInputs = OutInputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	const bool bHasAIIntent = !AIMoveInput.IsNearlyZero() || !AIOrientationIntent.IsNearlyZero();

	if (Pawn->GetController() == nullptr && !bHasAIIntent)
	{
		return;
	}

	const bool bBlockMovement = ASC != nullptr && ASC->HasMatchingGameplayTag(TAG_Block_MovementInput);

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
		// --- Player 입력 처리 ---

		FVector UpDir = GravityComponent ? GravityComponent->GetUpDirection() : FVector::UpVector;
		FQuat ControlQuat = CharacterInputs.ControlRotation.Quaternion();
		FVector ControlForward = ControlQuat.GetForwardVector();

		FVector RightDir = FVector::CrossProduct(UpDir, ControlForward).GetSafeNormal();
		if (RightDir.IsNearlyZero())
		{
			RightDir = FVector::CrossProduct(UpDir, ControlQuat.GetUpVector()).GetSafeNormal();
		}
		FVector HorizonForward = FVector::CrossProduct(RightDir, UpDir).GetSafeNormal();

		FVector FinalDirectionalIntent = FVector::ZeroVector;
		if (!bBlockMovement)
		{
			FinalDirectionalIntent = (HorizonForward * CachedMoveInputIntent.X) + (RightDir * CachedMoveInputIntent.Y);
		}
		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FinalDirectionalIntent);

		static float RotationMagMin(1e-3);
		const bool bHasAffirmativeMoveInput = (CharacterInputs.GetMoveInput().Size() >= RotationMagMin);

		CharacterInputs.OrientationIntent = FVector::ZeroVector;

		if (bHasAffirmativeMoveInput)
		{
			if (bFaceMoveDirection)
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
		else if (!bFaceMoveDirection)
		{
			CharacterInputs.OrientationIntent = HorizonForward;
		}
	}
	else
	{
		// --- AI Intent 처리 (StateTree) ---

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
	bIsGuardJustPressed = false;
}

// --- Input Event Implementation ---

void ULNPInputHandlerComponent::OnMoveTriggered(const FInputActionValue& Value)
{
	const FVector MovementVector = Value.Get<FVector>();
	CachedMoveInputIntent.X = FMath::Clamp(MovementVector.X, -1.0f, 1.0f);
	CachedMoveInputIntent.Y = FMath::Clamp(MovementVector.Y, -1.0f, 1.0f);
	CachedMoveInputIntent.Z = FMath::Clamp(MovementVector.Z, -1.0f, 1.0f);
}

void ULNPInputHandlerComponent::OnMoveCompleted(const FInputActionValue& Value)
{
	CachedMoveInputIntent = FVector::ZeroVector;
}

void ULNPInputHandlerComponent::OnLookTriggered(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
	CachedLookInput.Yaw = LookVector.X;
	CachedLookInput.Pitch = LookVector.Y;
}

void ULNPInputHandlerComponent::OnLookCompleted(const FInputActionValue& Value)
{
	CachedLookInput = FRotator::ZeroRotator;
}

void ULNPInputHandlerComponent::OnJumpStarted(const FInputActionValue& Value)
{
	bIsJumpJustPressed = !bIsJumpPressed;
	bIsJumpPressed = true;
}

void ULNPInputHandlerComponent::OnJumpReleased(const FInputActionValue& Value)
{
	bIsJumpPressed = false;
	bIsJumpJustPressed = false;
}

void ULNPInputHandlerComponent::OnDashStarted(const FInputActionValue& Value)
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

void ULNPInputHandlerComponent::OnDashReleased(const FInputActionValue& Value)
{
	bIsDashPressed = false;
	bIsDashJustPressed = false;
}

void ULNPInputHandlerComponent::OnInteractStarted(const FInputActionValue& Value)
{
	bIsInteractJustPressed = !bIsInteractPressed;
	bIsInteractPressed = true;

	if (InteractionComponent)
	{
		InteractionComponent->PerformInteraction();
	}
}

void ULNPInputHandlerComponent::OnInteractReleased(const FInputActionValue& Value)
{
	bIsInteractPressed = false;
	bIsInteractJustPressed = false;
}

void ULNPInputHandlerComponent::OnAttackStarted(const FInputActionValue& Value)
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

void ULNPInputHandlerComponent::OnAttackReleased(const FInputActionValue& Value)
{
	bIsAttackPressed = false;
	bIsAttackJustPressed = false;
}

void ULNPInputHandlerComponent::OnGuardStarted(const FInputActionValue& Value)
{
	bIsGuardJustPressed = !bIsGuardPressed;
	bIsGuardPressed = true;
}

void ULNPInputHandlerComponent::OnGuardReleased(const FInputActionValue& Value)
{
	bIsGuardPressed = false;
	bIsGuardJustPressed = false;
}

void ULNPInputHandlerComponent::OnLockOnStarted(const FInputActionValue& Value)
{
	bIsLockOnJustPressed = !bIsLockOnPressed;
	bIsLockOnPressed = true;
}

void ULNPInputHandlerComponent::OnLockOnReleased(const FInputActionValue& Value)
{
	bIsLockOnPressed = false;
	bIsLockOnJustPressed = false;
}

void ULNPInputHandlerComponent::OnActiveSkillStarted(const FInputActionValue& Value, int32 SlotIndex)
{
	ActiveSkillJustPressed[SlotIndex] = !ActiveSkillPressed[SlotIndex];
	ActiveSkillPressed[SlotIndex] = true;

	// 무기 장착 테스트: TestWeaponList 슬롯 직접 매핑
	// SlotIndex 0 = TestWeaponList[0] (예: LongSword)
	// SlotIndex 1 = TestWeaponList[1] (예: Pistol)
	// SlotIndex 2 = TestWeaponList[2] (예: Rifle)
	// 슬롯이 TestWeaponList 범위 밖이면 맨손으로 전환
	if (ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(GetOwner()))
	{
		Character->EquipTestWeapon(SlotIndex);
	}
}

void ULNPInputHandlerComponent::OnActiveSkillReleased(const FInputActionValue& Value, int32 SlotIndex)
{
	ActiveSkillPressed[SlotIndex] = false;
	ActiveSkillJustPressed[SlotIndex] = false;
}
