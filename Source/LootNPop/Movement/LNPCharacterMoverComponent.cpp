// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPCharacterMovementSettings.h"
#include "Movement/LNPAsyncSmoothWalkingMode.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "DefaultMovementSet/Modes/AsyncFallingMode.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(LNP_Mover_IsSprinting, "LNP.Mover.IsSprinting", "Character is sprinting");

ULNPCharacterMoverComponent::ULNPCharacterMoverComponent()
{
	bHandleSprintChanges = 1;
	bWantsToRun = 0;

	// Default movement modes
	MovementModes.Add(TEXT("LNPAsyncSmoothWalking"), CreateDefaultSubobject<ULNPAsyncSmoothWalkingMode>(TEXT("AsyncSmoothWalkingMode")));
	MovementModes.Add(TEXT("AsyncFalling"), CreateDefaultSubobject<UAsyncFallingMode>(TEXT("AsyncFallingMode")));

	StartingMovementMode = TEXT("AsyncFalling");
}

bool ULNPCharacterMoverComponent::IsSprinting() const
{
	return HasGameplayTag(LNP_Mover_IsSprinting, true);
}

bool ULNPCharacterMoverComponent::CanSprint() const
{
	// Character can sprint if they are on the ground.
	return IsOnGround();
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
	//Super::OnMoverPreSimulationTick(TimeStep, InputCmd);
}

void ULNPCharacterMoverComponent::OnHandlerSettingChanged()
{
	// Super will add/remove OnMoverPreSimulationTick based on handle jump/stance settings.
	Super::OnHandlerSettingChanged();

	const bool bIsHandlingAnySettings = bHandleSprintChanges;

	if (bIsHandlingAnySettings)
	{
		OnPreSimulationTick.AddUniqueDynamic(this, &ULNPCharacterMoverComponent::OnMoverPreSimulationTick);
	}
	else
	{
		OnPreSimulationTick.RemoveDynamic(this, &ULNPCharacterMoverComponent::OnMoverPreSimulationTick);
	}
}

