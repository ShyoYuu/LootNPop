// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPCharacterMovementSettings.h"
#include "Movement/LNPAsyncWalkingMode.h"

#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "DefaultMovementSet/Modes/AsyncFallingMode.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(LNPTAG_Mover_IsSprinting, "LNP.Mover.IsSprinting", "Character is sprinting");

ULNPCharacterMoverComponent::ULNPCharacterMoverComponent()
{
	bHandleSprintChanges = 1;
	bWantsToRun = 0;

	// Default movement modes
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
	return IsOnGround();
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

	TObjectPtr<UAnimMontage> SelectedMontage = nullptr;
	FVector DashDirection = FVector::ZeroVector;

	if (bHasMoveInput)
	{
		DashDirection = Pawn->GetControlRotation().RotateVector(MoveInputIntent).GetSafeNormal();
		SelectedMontage = DashForwardMontage;
	}
	else
	{
		DashDirection = -Pawn->GetActorForwardVector();
		SelectedMontage = DashBackwardMontage;
	}

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

void ULNPCharacterMoverComponent::OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
	// Manage the Sprint Modifier based on bWantsToRun and CanSprint
	if (bHandleSprintChanges)
	{
		const FLNPSprintModifier* ActiveModifier = static_cast<const FLNPSprintModifier*>(FindMovementModifier(SprintModifierHandle));
		if (!ActiveModifier)
		{
			ActiveModifier = FindMovementModifierByType<FLNPSprintModifier>();
		}

		const bool bIsSprinting = (ActiveModifier != nullptr);
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

	// Call Super to handle standard features (Jump, Crouch)
	Super::OnMoverPreSimulationTick(TimeStep, InputCmd);
}

void ULNPCharacterMoverComponent::OnHandlerSettingChanged()
{
	// Super will add/remove OnMoverPreSimulationTick based on handle jump/stance settings.
	//Super::OnHandlerSettingChanged();

	const bool bIsHandlingAnySettings = bHandleSprintChanges || bHandleJump || bHandleStanceChanges;

	if (bIsHandlingAnySettings)
	{
		OnPreSimulationTick.AddUniqueDynamic(this, &ULNPCharacterMoverComponent::OnMoverPreSimulationTick);
	}
	else
	{
		OnPreSimulationTick.RemoveDynamic(this, &ULNPCharacterMoverComponent::OnMoverPreSimulationTick);
	}
}
