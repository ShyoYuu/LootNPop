// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPCharacterMovementSettings.h"
#include "Movement/LNPAsyncWalkingMode.h"
#include "Movement/LNPModifierInputs.h"
#include "Character/LNPCharacterBase.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "DefaultMovementSet/Modes/AsyncFallingMode.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemComponent.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(LNP_Mover_IsSprinting, "LNP.Mover.IsSprinting", "Character is sprinting");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(LNP_Mover_IsGuarding, "LNP.Mover.IsGuarding",  "Character is guarding");

// UCommonLegacyMovementSettings 못 가져왔을 때 fallback용
const FName DefaultWalkingMode = TEXT("LNPAsyncWalking");
const FName DefaultFallingMode = TEXT("AsyncFalling");

ULNPCharacterMoverComponent::ULNPCharacterMoverComponent()
{
	bHandleSprintChanges = 1;
	bHandleGuardChanges = 1;
	bWantsToRun = 0;

	// 기본 이동 모드
	MovementModes.Add(DefaultWalkingMode, CreateDefaultSubobject<ULNPAsyncWalkingMode>(TEXT("LNPAsyncWalkingMode")));
	MovementModes.Add(DefaultFallingMode, CreateDefaultSubobject<UAsyncFallingMode>(TEXT("AsyncFallingMode")));

	StartingMovementMode = DefaultFallingMode;
}

bool ULNPCharacterMoverComponent::IsSprinting() const
{
	return HasGameplayTag(LNP_Mover_IsSprinting, true);
}

bool ULNPCharacterMoverComponent::CanSprint() const
{
	return IsOnGround() && !IsGuarding();
}

bool ULNPCharacterMoverComponent::IsGuarding() const
{
	return HasGameplayTag(LNP_Mover_IsGuarding, true);
}

bool ULNPCharacterMoverComponent::CanGuard()
{
	return true;
}

bool ULNPCharacterMoverComponent::CanDash() const
{
	return IsOnGround() && !bIsAiming &&
		(LastDashTime <= 0.0f || (GetWorld()->GetTimeSeconds() - LastDashTime) >= DashCooldown);
}

void ULNPCharacterMoverComponent::ExecuteDash(FVector MoveInputIntent)
{
	APawn* Pawn = CastChecked<APawn>(GetOwner());
	const bool bHasMoveInput = !MoveInputIntent.IsNearlyZero();
	FVector DashDirection = FVector::ZeroVector;
	FGameplayTag DirTag;

	ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(Pawn);
	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	const bool bIsStrafe = ASC &&
		(ASC->HasMatchingGameplayTag(TAG_AimMode_FreeAim) || ASC->HasMatchingGameplayTag(TAG_AimMode_LockOn));

	if (bIsStrafe && Character)
	{
		// FreeAim/LockOn: 이동 인풋 기준 4방향 태그 → ChooserTable 평가
		DashDirection = bHasMoveInput
			? Pawn->GetControlRotation().RotateVector(MoveInputIntent).GetSafeNormal()
			: -Pawn->GetActorForwardVector();

		
		if (bHasMoveInput)
		{
			// MoveInputIntent는 카메라 로컬 공간 (X=Forward, Y=Right)
			const float Angle = FMath::RadiansToDegrees(FMath::Atan2(MoveInputIntent.Y, MoveInputIntent.X));
			if      (Angle >= -45.f && Angle <   45.f) DirTag = TAG_Montage_Value_Direction_Front;
			else if (Angle >=  45.f && Angle <  135.f) DirTag = TAG_Montage_Value_Direction_Right;
			else if (Angle >= -135.f && Angle < -45.f) DirTag = TAG_Montage_Value_Direction_Left;
			else                                       DirTag = TAG_Montage_Value_Direction_Back;
		}
		else
		{
			DirTag = TAG_Montage_Value_Direction_Back;
		}
	}
	else
	{
		// AimMode_None: 기존 로직 (앞/뒤 2방향)
		if (bHasMoveInput)
		{
			DashDirection = Pawn->GetControlRotation().RotateVector(MoveInputIntent).GetSafeNormal();
			DirTag = TAG_Montage_Value_Direction_Front;
		}
		else
		{
			DashDirection = -Pawn->GetActorForwardVector();
			DirTag = TAG_Montage_Value_Direction_Back;
		}
	}

	UAnimMontage* SelectedMontage = Character->EvaluateMontage(TAG_Montage_Situation_Dash, DirTag);
	if (!SelectedMontage)
		return;

	LastDashTime = GetWorld()->GetTimeSeconds();

	const float DashDurationMs = DashDuration * 1000.0f;

	TSharedPtr<FLayeredMove_LinearVelocity> DashMove = MakeShared<FLayeredMove_LinearVelocity>();
	DashMove->Velocity = DashDirection * DashImpulseMagnitude;
	DashMove->DurationMs = DashDurationMs;
	DashMove->MixMode = EMoveMixMode::OverrideVelocity;
	DashMove->FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;
	QueueLayeredMove(DashMove);

	float ActualStartingPos = 0.0f;
	if (USkeletalMeshComponent* Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			AnimInstance->Montage_Play(SelectedMontage, 1.0f);
			if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(SelectedMontage))
				ActualStartingPos = MontageInstance->GetPosition();
		}
	}

	TSharedPtr<FLayeredMove_AnimRootMotion> AnimSyncMove = MakeShared<FLayeredMove_AnimRootMotion>();
	AnimSyncMove->MontageState.Montage = SelectedMontage;
	AnimSyncMove->MontageState.PlayRate = 1.0f;
	AnimSyncMove->MontageState.StartingMontagePosition = ActualStartingPos;
	AnimSyncMove->MontageState.CurrentPosition = ActualStartingPos;
	AnimSyncMove->DurationMs = DashDurationMs;
	QueueLayeredMove(AnimSyncMove);
}

void ULNPCharacterMoverComponent::ApplyKnockback(const FVector HitFromDirection, const float Strength)
{
	if (HitFromDirection.IsNearlyZero() || Strength <= 0.f)
		return;

	if (TSharedPtr<FApplyVelocityEffect> KnockbackEffect = MakeShared<FApplyVelocityEffect>())
	{
		KnockbackEffect->VelocityToApply = HitFromDirection.GetSafeNormal() * Strength;
		KnockbackEffect->bAdditiveVelocity = true;

		// GroundMovementMode에서는 매 틱 속도를 MaxWalkSpeed로 클램핑하고 위치를 지면에 스냅해버려서 의도한 넉백 느낌이 안남. 따라서 AirMovementMode로 적용.
		if (const UCommonLegacyMovementSettings* CommonSettings = FindSharedSettings<UCommonLegacyMovementSettings>())
			KnockbackEffect->ForceMovementMode = CommonSettings->AirMovementModeName;
		else
			KnockbackEffect->ForceMovementMode = GetMovementModeName();
		QueueInstantMovementEffect(KnockbackEffect);
	}
}

void ULNPCharacterMoverComponent::LaunchWithVelocity(FVector InVelocity)
{
	if (InVelocity.IsNearlyZero())
		return;

	TSharedPtr<FLayeredMove_Launch> LaunchMove = MakeShared<FLayeredMove_Launch>();
	LaunchMove->LaunchVelocity = InVelocity;
	LaunchMove->DurationMs = 0.f;
	LaunchMove->MixMode = EMoveMixMode::OverrideVelocity;
	if (const UCommonLegacyMovementSettings* CommonSettings = FindSharedSettings<UCommonLegacyMovementSettings>())
		LaunchMove->ForceMovementMode = CommonSettings->AirMovementModeName;
	else
		LaunchMove->ForceMovementMode = DefaultFallingMode;
	QueueLayeredMove(LaunchMove);
}

void ULNPCharacterMoverComponent::OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
	const AActor* DebugOwner = GetOwner();
	const APawn*  DebugPawn  = Cast<APawn>(DebugOwner);
	// 1P/2P를 구분하기 위한 접두어. 예: "[BP_LNPPlayer_C_1 Auth=0 Local=1]"
	const FString DebugTag = DebugOwner
		? FString::Printf(TEXT("%s Auth=%d Local=%d"), *DebugOwner->GetName(), DebugOwner->HasAuthority(), DebugPawn && DebugPawn->IsLocallyControlled())
		: TEXT("?");

	// bWantsToGuard/bWantsToRun 멤버 변수 대신 InputCmd에서 읽는다 — Jump가 FCharacterDefaultInputs::
	// bIsJumpJustPressed를 InputCmd로 전달받는 것과 동일한 방식. 평범한 컴포넌트 멤버는 Mover의
	// 예측·복제·리시뮬레이션 파이프라인을 타지 않아 원격 클라이언트에서 신뢰할 수 없었다.
	const FLNPModifierInputs* ModifierInputs = InputCmd.InputCollection.FindDataByType<FLNPModifierInputs>();

	// Guard Modifier 관리 (Sprint보다 먼저 처리 — Guard 중에는 Sprint 불가)
	if (bHandleGuardChanges)
	{
		const FLNPGuardModifier* ActiveModifier = static_cast<const FLNPGuardModifier*>(FindMovementModifier(GuardModifierHandle));
		if (ActiveModifier == nullptr)
		{
			ActiveModifier = FindMovementModifierByType<FLNPGuardModifier>();
		}

		const bool bIsGuarding = HasGameplayTag(LNP_Mover_IsGuarding, true);
		const bool bShouldGuard = ModifierInputs && ModifierInputs->bWantsToGuard;

		if (bIsGuarding && (!bShouldGuard || !CanGuard()))
		{
			UE_LOG(LogLootNPop, Log, TEXT("[GuardDebug] PreSimTick [%s]: REMOVE guard modifier (bWantsToGuard=false)"), *DebugTag);
			if (ActiveModifier)
			{
				CancelModifierFromHandle(ActiveModifier->GetHandle());
				GuardModifierHandle.Invalidate();
				ActiveModifier = nullptr;
			}
		}
		else if (!bIsGuarding && bShouldGuard && CanGuard())
		{
			UE_LOG(LogLootNPop, Log, TEXT("[GuardDebug] PreSimTick [%s]: ADD guard modifier (bWantsToGuard=true)"), *DebugTag);
			TSharedPtr<FLNPGuardModifier> NewModifier = MakeShared<FLNPGuardModifier>();
			GuardModifierHandle = QueueMovementModifier(NewModifier);
			ActiveModifier = NewModifier.Get();
		}

		// 진단용: MaxSpeed가 이전 틱과 다르면 원인 불문 로그 (스팸 방지를 위해 값이 바뀔 때만)
		if (const UCommonLegacyMovementSettings* CommonSettings = FindSharedSettings<UCommonLegacyMovementSettings>())
		{
			if (!FMath::IsNearlyEqual(CommonSettings->MaxSpeed, DebugLastLoggedMaxSpeed, 0.01f))
			{
				UE_LOG(LogLootNPop, Log, TEXT("[GuardDebug] PreSimTick [%s]: MaxSpeed changed %.1f -> %.1f (bShouldGuard=%d, bIsGuarding=%d)"),
					*DebugTag, DebugLastLoggedMaxSpeed, CommonSettings->MaxSpeed, bShouldGuard, bIsGuarding);
				DebugLastLoggedMaxSpeed = CommonSettings->MaxSpeed;
			}
		}
	}

	// Sprint Modifier 관리 (CanSprint 내부에서 IsGuarding() 체크)
	if (bHandleSprintChanges)
	{
		const FLNPSprintModifier* ActiveModifier = static_cast<const FLNPSprintModifier*>(FindMovementModifier(SprintModifierHandle));
		if (ActiveModifier == nullptr)
		{
			ActiveModifier = FindMovementModifierByType<FLNPSprintModifier>();
		}

		const bool bIsSprinting  = HasGameplayTag(LNP_Mover_IsSprinting, true);
		const bool bShouldSprint = ModifierInputs && ModifierInputs->bWantsToSprint;

		if (bIsSprinting && (!bShouldSprint || !CanSprint()))
		{
			if (ActiveModifier)
			{
				CancelModifierFromHandle(ActiveModifier->GetHandle());
				SprintModifierHandle.Invalidate();
				ActiveModifier = nullptr;
			}
		}
		else if (!bIsSprinting && bShouldSprint && CanSprint())
		{
			TSharedPtr<FLNPSprintModifier> NewModifier = MakeShared<FLNPSprintModifier>();
			SprintModifierHandle = QueueMovementModifier(NewModifier);
			ActiveModifier = NewModifier.Get();
		}
	}

	// 기본 기능(점프, 앉기) 처리를 위해 Super 호출
	Super::OnMoverPreSimulationTick(TimeStep, InputCmd);
}

void ULNPCharacterMoverComponent::OnHandlerSettingChanged()
{
	// Super는 점프/자세 설정에 따라 OnMoverPreSimulationTick을 추가/제거한다.
	//Super::OnHandlerSettingChanged();

	const bool bIsHandlingAnySettings = bHandleSprintChanges || bHandleGuardChanges || bHandleJump || bHandleStanceChanges;

	if (bIsHandlingAnySettings)
	{
		OnPreSimulationTick.AddUniqueDynamic(this, &ULNPCharacterMoverComponent::OnMoverPreSimulationTick);
	}
	else
	{
		OnPreSimulationTick.RemoveDynamic(this, &ULNPCharacterMoverComponent::OnMoverPreSimulationTick);
	}
}
