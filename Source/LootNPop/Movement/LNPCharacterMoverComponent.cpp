// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPCharacterMovementSettings.h"
#include "Movement/LNPAsyncWalkingMode.h"
#include "Character/LNPCharacterBase.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "DefaultMovementSet/Modes/AsyncFallingMode.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemComponent.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(LNPTAG_Mover_IsSprinting, "LNP.Mover.IsSprinting", "Character is sprinting");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(LNPTAG_Mover_IsGuarding, "LNP.Mover.IsGuarding",  "Character is guarding");

ULNPCharacterMoverComponent::ULNPCharacterMoverComponent()
{
	bHandleSprintChanges = 1;
	bHandleGuardChanges = 1;
	bWantsToRun = 0;

	// 기본 이동 모드
	MovementModes.Add(TEXT("LNPAsyncWalking"), CreateDefaultSubobject<ULNPAsyncWalkingMode>(TEXT("AsyncWalkingMode")));
	MovementModes.Add(TEXT("AsyncFalling"), CreateDefaultSubobject<UAsyncFallingMode>(TEXT("AsyncFallingMode")));

	StartingMovementMode = TEXT("AsyncFalling");
}

bool ULNPCharacterMoverComponent::IsSprinting() const
{
	return HasGameplayTag(LNPTAG_Mover_IsSprinting, true);
}

bool ULNPCharacterMoverComponent::CanSprint() const
{
	return IsOnGround() && !IsGuarding();
}

bool ULNPCharacterMoverComponent::IsGuarding() const
{
	return HasGameplayTag(LNPTAG_Mover_IsGuarding, true);
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

void ULNPCharacterMoverComponent::OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
	// Guard Modifier 관리 (Sprint보다 먼저 처리 — Guard 중에는 Sprint 불가)
	if (bHandleGuardChanges)
	{
		const FLNPGuardModifier* ActiveModifier = static_cast<const FLNPGuardModifier*>(FindMovementModifier(GuardModifierHandle));
		if (ActiveModifier == nullptr)
		{
			ActiveModifier = FindMovementModifierByType<FLNPGuardModifier>();

			// Handle로는 못 찾았는데 Type으로 찾았을 땐 Handle이 유효하지 않은 상태이므로 갱신해준다.
			if (ActiveModifier != nullptr)
				GuardModifierHandle = ActiveModifier->GetHandle();
		}

		const bool bIsGuarding  = (ActiveModifier != nullptr);
		const bool bShouldGuard = bWantsToGuard;

		if (bIsGuarding && !bShouldGuard)
		{
			CancelModifierFromHandle(GuardModifierHandle);
			GuardModifierHandle.Invalidate();
		}
		else if (!bIsGuarding && bShouldGuard)
		{
			TSharedPtr<FLNPGuardModifier> NewModifier = MakeShared<FLNPGuardModifier>();
			GuardModifierHandle = QueueMovementModifier(NewModifier);
		}
	}

	// Sprint Modifier 관리 (CanSprint 내부에서 IsGuarding() 체크)
	if (bHandleSprintChanges)
	{
		const FLNPSprintModifier* ActiveModifier = static_cast<const FLNPSprintModifier*>(FindMovementModifier(SprintModifierHandle));
		if (ActiveModifier == nullptr)
		{
			ActiveModifier = FindMovementModifierByType<FLNPSprintModifier>();
			if (ActiveModifier != nullptr)
				SprintModifierHandle = ActiveModifier->GetHandle();
		}

		const bool bIsSprinting  = (ActiveModifier != nullptr);
		const bool bShouldSprint = bWantsToRun && CanSprint();

		if (bIsSprinting && !bShouldSprint)
		{
			CancelModifierFromHandle(SprintModifierHandle);
			SprintModifierHandle.Invalidate();
		}
		else if (!bIsSprinting && bShouldSprint)
		{
			TSharedPtr<FLNPSprintModifier> NewModifier = MakeShared<FLNPSprintModifier>();
			SprintModifierHandle = QueueMovementModifier(NewModifier);
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
