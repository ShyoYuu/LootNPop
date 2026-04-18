// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "NativeGameplayTags.h"
#include "LNPSprintModifier.h"
#include "LNPCharacterMoverComponent.generated.h"

LOOTNPOP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LNP_Mover_IsSprinting);

/**
 * Custom Mover component for LootNPop characters.
 * Handles dynamic movement setting updates (like sprinting) using Modifiers and custom Settings.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LOOTNPOP_API ULNPCharacterMoverComponent : public UCharacterMoverComponent
{
	GENERATED_BODY()

public:
	ULNPCharacterMoverComponent();

	/** Set whether the character wants to run (sprint) or not */
	UFUNCTION(BlueprintCallable, Category = "LNP|Movement")
	void SetWantsToRun(bool bInWantsToRun) { bWantsToRun = bInWantsToRun; }

	/** Returns true if the character is currently sprinting */
	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool IsSprinting() const;

	/** Check if the character can currently sprint */
	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool CanSprint() const;

protected:
	/** Fired just before a simulation tick. Used to apply state-driven modifier changes. */
	virtual void OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd);

	/** Overridden to ensure our custom simulation logic is always registered. */
	virtual void OnHandlerSettingChanged() override;

private:
	/** Whether this component should directly handle sprinting logic based on intent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	uint8 bHandleSprintChanges : 1 = 1;

	/** If true, the character intends to sprint in the next simulation tick */
	UPROPERTY(BlueprintReadOnly, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	uint8 bWantsToRun : 1 = 0;

	/** Handle to the active sprint modifier */
	FMovementModifierHandle SprintModifierHandle;
};

