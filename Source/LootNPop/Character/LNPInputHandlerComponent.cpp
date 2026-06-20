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

#include "MassAgentComponent.h"
#include "MassEntitySubsystem.h"
#include "HitDetection/LNPGuardParryTypes.h"

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
}

void ULNPInputHandlerComponent::CacheASC(UAbilitySystemComponent* InASC)
{
	ASC = InASC;
}

void ULNPInputHandlerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CachedLookInput.IsNearlyZero() && GravityComponent)
	{
		GravityComponent->InputLook(CachedLookInput);
	}
	CachedLookInput = FRotator::ZeroRotator;

	
	// --- Buffering for jump and dash ---
	// Cooldown이 끝나기 직전(0.05초)에 Input이 들어왔다면 Cooldown이 끝나자마자 실행될 수 있도록 버퍼링
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
	// ---
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
		EIC->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ULNPInputHandlerComponent::OnAttackTriggered);
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

		const float RotationMagMin = (1e-3f);
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

FLNPParryStateFragment* ULNPInputHandlerComponent::GetParryFragment() const
{
	UWorld* World = GetWorld();
	if (!World)
		return nullptr;

	UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSub)
		return nullptr;

	AActor* Owner = GetOwner();
	if (!Owner)
		return nullptr;

	UMassAgentComponent* AgentComp = Owner->FindComponentByClass<UMassAgentComponent>();
	if (!AgentComp)
		return nullptr;

	FMassEntityManager& EM = MassSub->GetMutableEntityManager();
	const FMassEntityHandle Handle = AgentComp->GetEntityHandle();
	if (!EM.IsEntityValid(Handle))
		return nullptr;

	return EM.GetFragmentDataPtr<FLNPParryStateFragment>(Handle);
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
	// Guard 테스트를 위해 잠시 주석
	//bIsJumpJustPressed = !bIsJumpPressed;
	//bIsJumpPressed = true;
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

void ULNPInputHandlerComponent::OnAttackTriggered(const FInputActionValue& Value)
{
	bIsAttackJustPressed = !bIsAttackPressed;
	bIsAttackPressed = true;

	const bool bIsFreeAim = ASC && ASC->HasMatchingGameplayTag(TAG_AimMode_FreeAim);
	ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(GetOwner());
	if (!Character)
		return;

	if (bIsFreeAim || bIsAttackJustPressed)
	{
		// FreeAim 모드: ETriggerEvent::Triggered로 홀드 중 매 프레임 호출 → 연사 속도는 GAS 쿨다운이 제어.
		// FreeAim 모드가 아닐 때는 bIsAttackJustPressed로 입력 시작 시에만 호출.
		// 탭 입력 시 쿨다운이 0.05f 이내에 끝나는 경우를 위해 버퍼 적용.
		if (!Character->TryActivateAttack())
		{
			bIsAttackBuffered = true;
			AttackBufferTime = GetWorld()->GetTimeSeconds();
		}
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

	if (MoverComponent)
		MoverComponent->SetWantsToGuard(true);

	if (ASC)
	{
		ASC->AddLooseGameplayTag(TAG_State_Guarding);

		// 패링 창: 가드 입력 직후 ParryWindowDuration 동안만 활성
		ASC->AddLooseGameplayTag(TAG_State_ParryWindow);
		GetWorld()->GetTimerManager().SetTimer(
			ParryWindowTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (ASC)
					ASC->RemoveLooseGameplayTag(TAG_State_ParryWindow);
				if (FLNPParryStateFragment* PF = GetParryFragment())
					PF->bIsParrying = false;
			}),
			ParryWindowDuration, false);

		if (FLNPParryStateFragment* PF = GetParryFragment())
		{
			PF->bIsGuarding = true;
			PF->bIsParrying = true;
		}
	}
}

void ULNPInputHandlerComponent::OnGuardReleased(const FInputActionValue& Value)
{
	bIsGuardPressed = false;
	bIsGuardJustPressed = false;

	if (MoverComponent)
		MoverComponent->SetWantsToGuard(false);

	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(TAG_State_Guarding);
		ASC->RemoveLooseGameplayTag(TAG_State_ParryWindow);
		GetWorld()->GetTimerManager().ClearTimer(ParryWindowTimer);

		if (FLNPParryStateFragment* PF = GetParryFragment())
		{
			PF->bIsGuarding = false;
			PF->bIsParrying = false;
		}
	}
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
	// SlotIndex 0 = TestWeaponList[0] (예: Pistol)
	// SlotIndex 1 = TestWeaponList[1] (예: Rifle)
	// SlotIndex 2 = TestWeaponList[2] (예: LongSword)
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
