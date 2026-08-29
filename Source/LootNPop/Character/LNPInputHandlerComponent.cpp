// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Character/LNPInputHandlerComponent.h"
#include "Character/LNPCharacterBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "Player/LNPPlayerController.h"
#include "LootNPop.h"

#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPModifierInputs.h"
#include "Gravity/LNPPawnGravityComponent.h"
#include "Camera/LNPControlRotationComponent.h"
#include "Interaction/LNPInteractionComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "LNPGameplayTags.h"

#include "MassAgentComponent.h"
#include "MassEntitySubsystem.h"
#include "HitDetection/LNPGuardParryTypes.h"
#include "Camera/LNPLockOnComponent.h"
#include "HAL/IConsoleManager.h"

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
	ControlRotationComponent = Owner->FindComponentByClass<ULNPControlRotationComponent>();
	InteractionComponent = Owner->FindComponentByClass<ULNPInteractionComponent>();
	LockOnComponent = Owner->FindComponentByClass<ULNPLockOnComponent>();

	// ControlRotationComponent는 이 컴포넌트가 look 입력을 적립한 후 실행되어야 함
	if (ControlRotationComponent)
		ControlRotationComponent->AddTickPrerequisiteComponent(this);

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

	TickDebugAutoAction(DeltaTime);

	if (!CachedLookInput.IsNearlyZero() && ControlRotationComponent)
	{
		ControlRotationComponent->InputLook(CachedLookInput);
	}
	CachedLookInput = FRotator::ZeroRotator;

	
	// --- Buffering for jump and dash ---
	// Cooldown이 끝나기 직전(0.05초)에 Input이 들어왔다면 Cooldown이 끝나자마자 실행될 수 있도록 버퍼링
	// 버퍼가 열려 있는 동안 OnProduceInput이 매 틱 대시 의도를 InputCmd에 싣고,
	// 실행 가부는 시뮬레이션(ULNPCharacterMoverComponent::OnMoverPreSimulationTick)이 판정한다.
	// 여기서는 창을 닫기만 한다 — 대시가 성사되면 쿨다운 Modifier가 남은 창의 재실행을 막는다.
	if (bIsDashBuffered && GetWorld()->GetTimeSeconds() - DashBufferTime > 0.05f)
	{
		bIsDashBuffered = false;
	}

	if (bIsAttackBuffered)
	{
		ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(GetOwner());
		if (!Character || 0.05f < (GetWorld()->GetTimeSeconds() - AttackBufferTime) || Character->TryActivateAttack())
			bIsAttackBuffered = false;
	}
	// ---
}

void ULNPInputHandlerComponent::SetGameplayInputEnabled(bool bEnabled)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn == nullptr || DefaultMappingContext == nullptr)
		return;

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (PC == nullptr)
		return;

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (Subsystem == nullptr)
		return;

	if (bEnabled)
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}
	else
	{
		Subsystem->RemoveMappingContext(DefaultMappingContext);

		// 매핑을 떼면 Completed/Released 이벤트가 오지 않으므로, 눌린 채로 굳지 않도록 직접 턴다.
		CachedMoveInputIntent = FVector::ZeroVector;
		CachedLookInput = FRotator::ZeroRotator;
		bIsJumpPressed = false;
		bIsDashPressed = false;
		bIsInteractPressed = false;
		bIsAttackPressed = false;
		bIsGuardPressed = false;
		bIsADSPressed = false;
		bIsLockOnPressed = false;
	}
}

void ULNPInputHandlerComponent::SetGameplayInputBlocked(bool bBlocked)
{
	bGameplayInputBlocked = bBlocked;

	if (!bBlocked)
		return;

	// 매핑은 그대로 두므로 Release 이벤트는 계속 오지만, 차단 시점에 눌려 있던 상태가
	// 그대로 소비되지 않도록 즉시 턴다 (SetGameplayInputEnabled(false)와 같은 이유).
	CachedMoveInputIntent = FVector::ZeroVector;
	bIsJumpPressed = false;
	bIsJumpJustPressed = false;
	bIsDashPressed = false;
	bIsDashJustPressed = false;
	bIsDashBuffered = false;
	bIsInteractPressed = false;
	bIsInteractJustPressed = false;
	bIsAttackPressed = false;
	bIsAttackJustPressed = false;
	bIsAttackBuffered = false;
	bIsGuardPressed = false;
	bIsGuardJustPressed = false;
	bIsADSPressed = false;
	bIsADSJustPressed = false;
	bIsLockOnPressed = false;
	bIsLockOnJustPressed = false;
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
		EIC->BindAction(ADSAction, ETriggerEvent::Started, this, &ULNPInputHandlerComponent::OnADSStarted);
		EIC->BindAction(ADSAction, ETriggerEvent::Completed, this, &ULNPInputHandlerComponent::OnADSReleased);
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

	// Guard/Sprint/Dash/ADS 의도를 InputCmd에 실어 보낸다 (Jump가 FCharacterDefaultInputs::bIsJumpJustPressed로 전달되는 것과 동일한 방식).
	FLNPModifierInputs& ModifierInputs = OutInputCmd.InputCollection.FindOrAddMutableDataByType<FLNPModifierInputs>();
	ModifierInputs.bWantsToGuard  = IsGuardActive();
	ModifierInputs.bWantsToSprint = bIsDashPressed;
	ModifierInputs.bWantsToDash   = bIsDashBuffered;
	ModifierInputs.bWantsToADS    = IsADSActive();
	ModifierInputs.DashInputIntent = bIsDashBuffered ? CachedMoveInputIntent : FVector::ZeroVector;
	// AI 속도도 여기에 싣는다 — 컴포넌트 멤버로만 두면 클라이언트 재시뮬레이션이 CDO MaxSpeed로
	// 폴백해 서버보다 훨씬 빠르게 앞서 나간다 (FLNPModifierInputs::AIDesiredSpeed 주석 참조).
	ModifierInputs.AIDesiredSpeed = AIDesiredSpeed;

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

		// 락온 활성 시 이동 방향과 무관하게 카메라 전방을 바라본다
		const bool bLockOnActive = LockOnComponent && LockOnComponent->IsLockOnActive();
		const bool bShouldFaceMoveDir = bFaceMoveDirection && !bLockOnActive;

		if (bHasAffirmativeMoveInput)
		{
			if (bShouldFaceMoveDir)
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
		else if (!bShouldFaceMoveDir)
		{
			CharacterInputs.OrientationIntent = HorizonForward;
		}
	}
	else
	{
		// --- AI Intent 처리 (StateTree) ---

		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, bBlockMovement ? FVector::ZeroVector : AIMoveInput);

		if (!AIOrientationIntent.IsNearlyZero())
		{
			CharacterInputs.OrientationIntent = AIOrientationIntent;
		}
		else if (!bBlockMovement && !AIMoveInput.IsNearlyZero())
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
	if (!EM.IsEntityActive(Handle))
		return nullptr;

	return EM.GetFragmentDataPtr<FLNPParryStateFragment>(Handle);
}

// --- Input Event Implementation ---

void ULNPInputHandlerComponent::OnMoveTriggered(const FInputActionValue& Value)
{
	// 사망 연출 중에는 Look만 살린다 (SetGameplayInputBlocked).
	if (bGameplayInputBlocked)
		return;

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
	// 사망 연출 중에는 Look만 살린다 (SetGameplayInputBlocked).
	if (bGameplayInputBlocked)
		return;

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
	// 사망 연출 중에는 Look만 살린다 (SetGameplayInputBlocked).
	if (bGameplayInputBlocked)
		return;

	// 경직 중 대시 금지. 대시는 LayeredMove라 TAG_Block_MovementInput으로 막히지 않는다.
	if (ASC && ASC->HasMatchingGameplayTag(TAG_State_Staggered))
		return;

	bIsDashJustPressed = !bIsDashPressed;
	bIsDashPressed = true;

	// 여기서 직접 ExecuteDash를 호출하면 InputCmd를 타지 않아 서버가 재현할 수 없다.
	// 의도만 버퍼에 남기고, 실행은 시뮬레이션이 InputCmd를 읽어 수행한다.
	bIsDashBuffered = true;
	DashBufferTime = GetWorld()->GetTimeSeconds();
}

void ULNPInputHandlerComponent::OnDashReleased(const FInputActionValue& Value)
{
	bIsDashPressed = false;
	bIsDashJustPressed = false;
}

void ULNPInputHandlerComponent::OnInteractStarted(const FInputActionValue& Value)
{
	// 사망 연출 중에는 Look만 살린다 (SetGameplayInputBlocked).
	if (bGameplayInputBlocked)
		return;

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

bool ULNPInputHandlerComponent::IsFreeAimMode() const
{
	return ASC && ASC->HasMatchingGameplayTag(TAG_AimMode_FreeAim);
}

bool ULNPInputHandlerComponent::IsADSActive() const
{
	return bIsADSPressed && IsFreeAimMode();
}

bool ULNPInputHandlerComponent::IsGuardActive() const
{
	return bIsGuardPressed && !IsFreeAimMode();
}

void ULNPInputHandlerComponent::OnAttackTriggered(const FInputActionValue& Value)
{
	// 사망 연출 중에는 Look만 살린다 (SetGameplayInputBlocked).
	if (bGameplayInputBlocked)
		return;

	bIsAttackJustPressed = !bIsAttackPressed;
	bIsAttackPressed = true;

	const bool bIsFreeAim = IsFreeAimMode();
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
	// 사망 연출 중에는 Look만 살린다 (SetGameplayInputBlocked).
	if (bGameplayInputBlocked)
		return;

	// Guard와 ADS는 같은 키를 공유한다 — 근접 무기면 Guard, 총기(FreeAim)면 ADS로 갈린다.
	if (IsFreeAimMode())
		return;

	// 경직 중에는 가드로 빠져나갈 수 없다 (CanGuard가 상시 true인 한계를 이 지점에서 좁힌다).
	if (ASC && ASC->HasMatchingGameplayTag(TAG_State_Staggered))
		return;

	bIsGuardJustPressed = !bIsGuardPressed;
	bIsGuardPressed = true;

	UE_LOG(LogLootNPop, Log, TEXT("[Guard] OnGuardStarted [%s]"), *GetNameSafe(GetOwner()));

	// Guard 의도는 OnProduceInput에서 매 틱 InputCmd로 전달되므로 여기서 별도로 MoverComponent에 쓸 필요 없다.

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
			// 로컬(예측) 갱신 — RTT 역보정 없이 즉시 만료 시각 설정. 서버 RPC 처리 시 보정된 값으로 덮어써진다.
			PF->ParryWindowExpiryTime = GetWorld()->GetTimeSeconds() + ParryWindowDuration;
		}
	}

	// 리슨 서버 호스트가 스스로를 호출하면 즉시 로컬 실행되므로 HasAuthority 분기 없이 항상 호출한다.
	Server_SetGuardState(true);
}

void ULNPInputHandlerComponent::ReleaseGuardState()
{
	bIsGuardPressed = false;
	bIsGuardJustPressed = false;

	// Guard 의도는 OnProduceInput에서 매 틱 InputCmd로 전달되므로 여기서 별도로 MoverComponent에 쓸 필요 없다.

	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(TAG_State_Guarding);
		ASC->RemoveLooseGameplayTag(TAG_State_ParryWindow);
		GetWorld()->GetTimerManager().ClearTimer(ParryWindowTimer);

		if (FLNPParryStateFragment* PF = GetParryFragment())
		{
			PF->bIsGuarding = false;
			PF->bIsParrying = false;
			PF->ParryWindowExpiryTime = -1.0;
		}
	}

	Server_SetGuardState(false);
}

void ULNPInputHandlerComponent::Client_ForceReleaseGuard_Implementation()
{
	if (!bIsGuardPressed)
		return;

	UE_LOG(LogLootNPop, Log, TEXT("[Guard] Force released by stagger [%s]"), *GetNameSafe(GetOwner()));
	ReleaseGuardState();
}

void ULNPInputHandlerComponent::OnGuardReleased(const FInputActionValue& Value)
{
	UE_LOG(LogLootNPop, Log, TEXT("[Guard] OnGuardReleased [%s]"), *GetNameSafe(GetOwner()));

	ReleaseGuardState();
}

void ULNPInputHandlerComponent::NotifyAimModeChanged()
{
	// 조준 모드가 바뀌면 이 키가 의미하는 행동 자체가 바뀐다. 눌린 채로 남겨 두면
	// 총을 든 채 가드·패링이 유지된다(ASC 태그와 패링 프래그먼트는 폴링이 아니라 명령형이라 스스로 풀리지 않는다).
	if (bIsGuardPressed)
		ReleaseGuardState();

	// ADS는 IsADSActive()가 조준 모드를 함께 보므로 이미 무력화돼 있지만,
	// 근접으로 바꿨다가 다시 총으로 돌아올 때 조용히 재개되지 않도록 눌림 상태도 함께 턴다.
	bIsADSPressed = false;
	bIsADSJustPressed = false;
}

void ULNPInputHandlerComponent::Server_SetGuardState_Implementation(bool bGuarding)
{
	FLNPParryStateFragment* PF = GetParryFragment();
	if (!PF)
		return;

	const double Now = GetWorld()->GetTimeSeconds();

	if (bGuarding)
	{
		PF->bIsGuarding = true;
		PF->bIsParrying = true;

		// 방어자 RTT/2만큼 과거로 되돌려 실제 입력 시각을 복원한다 (섹션 5.1).
		// 보정 클램프 상한은 패링 창 절반으로 보수적 설정.
		float RewindSeconds = 0.f;
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
			if (const APlayerState* PS = OwnerPawn->GetPlayerState<APlayerState>())
				RewindSeconds = FMath::Clamp(PS->GetPingInMilliseconds() * 0.0005f, 0.f, ParryWindowDuration * 0.5f);

		PF->ParryWindowExpiryTime = Now - RewindSeconds + ParryWindowDuration;

		GetWorld()->GetTimerManager().SetTimer(
			ServerParryWindowTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (FLNPParryStateFragment* PF2 = GetParryFragment())
					PF2->bIsParrying = false;
			}),
			FMath::Max(0.01f, ParryWindowDuration - RewindSeconds), false);
	}
	else
	{
		PF->bIsGuarding = false;
		PF->bIsParrying = false;
		PF->ParryWindowExpiryTime = -1.0;
		GetWorld()->GetTimerManager().ClearTimer(ServerParryWindowTimer);
	}
}

void ULNPInputHandlerComponent::OnADSStarted(const FInputActionValue& Value)
{
	// 사망 연출 중에는 Look만 살린다 (SetGameplayInputBlocked).
	if (bGameplayInputBlocked)
		return;

	// Guard와 ADS는 같은 키를 공유한다 — OnGuardStarted의 조건과 정확히 반대다.
	if (!IsFreeAimMode())
		return;

	bIsADSJustPressed = !bIsADSPressed;
	bIsADSPressed = true;
}

void ULNPInputHandlerComponent::OnADSReleased(const FInputActionValue& Value)
{
	bIsADSPressed = false;
	bIsADSJustPressed = false;
}

void ULNPInputHandlerComponent::OnLockOnStarted(const FInputActionValue& Value)
{
	// 사망 연출 중에는 Look만 살린다 (SetGameplayInputBlocked).
	if (bGameplayInputBlocked)
		return;

	bIsLockOnJustPressed = !bIsLockOnPressed;
	bIsLockOnPressed = true;

	if (LockOnComponent)
	{
		LockOnComponent->ToggleLockOn();
	}
}

void ULNPInputHandlerComponent::OnLockOnReleased(const FInputActionValue& Value)
{
	bIsLockOnPressed = false;
	bIsLockOnJustPressed = false;
}

void ULNPInputHandlerComponent::OnActiveSkillStarted(const FInputActionValue& Value, int32 SlotIndex)
{
	// 사망 연출 중에는 Look만 살린다 (SetGameplayInputBlocked).
	if (bGameplayInputBlocked)
		return;

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

// ──────────────────────────────────────────────────────────────────────────────
// PIE 멀티플레이 테스트 전용 디버그 오토 액션 — Shift+F1 창 전환 없이 한쪽을 자동화한다.
// 콘솔: LNP.Debug.AuthorityAutoAction / LNP.Debug.ClientAutoAction  (0=끔, 1=공격, 2=가드·패링 펄스)
// ──────────────────────────────────────────────────────────────────────────────

namespace
{
	TAutoConsoleVariable<int32> CVarDebugAuthorityAutoAction(
		TEXT("LNP.Debug.AuthorityAutoAction"), 0,
		TEXT("Server (HasAuthority) character auto-behavior. 0=off 1=attack 2=guard/parry pulse"), ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarDebugClientAutoAction(
		TEXT("LNP.Debug.ClientAutoAction"), 0,
		TEXT("Non-server (client) character auto-behavior. 0=off 1=attack 2=guard/parry pulse"), ECVF_Cheat);
}

void ULNPInputHandlerComponent::TickDebugAutoAction(float DeltaTime)
{
	const APawn* Owner = Cast<APawn>(GetOwner());
	// HasAuthority()는 서버 월드에서 처리되는 모든 폰(호스트 자신 + 원격 클라이언트의 서버측 복제본)에
	// 전부 true를 반환하므로, IsLocallyControlled()로 먼저 "이 월드 인스턴스가 실제로 조작 중인
	// 바로 그 캐릭터"만 남겨야 한다. 그렇지 않으면 AuthorityAutoAction이 서버 월드의 모든 폰에 적용된다.
	if (!Owner || !Owner->IsLocallyControlled())
		return;

	OnAttackReleased(FInputActionValue());

	const int32 Mode = Owner->HasAuthority()
		? CVarDebugAuthorityAutoAction.GetValueOnGameThread()
		: CVarDebugClientAutoAction.GetValueOnGameThread();

	if (Mode == 0)
	{
		DebugAutoActionTimer = 0.f;
		if (bDebugGuardPulseActive)
		{
			OnGuardReleased(FInputActionValue());
			bDebugGuardPulseActive = false;
		}
		return;
	}

	DebugAutoActionTimer -= DeltaTime;
	if (DebugAutoActionTimer > 0.f)
		return;

	if (Mode == 1) // 자동 공격 — 기존 입력 경로(TryActivateAttack) 그대로 재사용
	{
		OnAttackTriggered(FInputActionValue());
		DebugAutoActionTimer = 0.6f;
	}
	else if (Mode == 2) // 자동 가드 펄스: 0.3초 가드 유지 → 0.5초 대기 → 반복
	{
		if (bDebugGuardPulseActive)
		{
			OnGuardReleased(FInputActionValue());
			bDebugGuardPulseActive = false;
			DebugAutoActionTimer = 0.5f;
		}
		else
		{
			OnGuardStarted(FInputActionValue());
			bDebugGuardPulseActive = true;
			DebugAutoActionTimer = 0.3f;
		}
	}
}
