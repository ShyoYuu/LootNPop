// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "NativeGameplayTags.h"
#include "LNPSprintModifier.h"
#include "LNPCharacterMoverComponent.generated.h"

class UAnimMontage;

LOOTNPOP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LNPTAG_Mover_IsSprinting);

/**
 * Custom Mover component for LootNPop characters.
 * Handles sprinting, dash execution, and dynamic movement modifier updates.
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

	void SetIsAiming(bool bInIsAiming) { bIsAiming = bInIsAiming; }
	bool GetIsAiming() const { return bIsAiming; }

	/** Returns true if dash can be executed right now (on ground, not aiming, cooldown elapsed) */
	bool CanDash() const;

	/** Execute a dash using the given raw move input intent */
	void ExecuteDash(FVector MoveInputIntent);

protected:
	/** Fired just before a simulation tick. Used to apply state-driven modifier changes. */
	virtual void OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd) override;

	/** Overridden to ensure our custom simulation logic is always registered. */
	virtual void OnHandlerSettingChanged() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	float DashDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	float DashImpulseMagnitude = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	float DashCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	TObjectPtr<UAnimMontage> DashForwardMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	TObjectPtr<UAnimMontage> DashBackwardMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	TObjectPtr<UAnimMontage> DashLeftMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Dash")
	TObjectPtr<UAnimMontage> DashRightMontage;

private:
	/** Whether this component should directly handle sprinting logic based on intent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	uint8 bHandleSprintChanges : 1 = 1;

	/** If true, the character intends to sprint in the next simulation tick */
	UPROPERTY(BlueprintReadOnly, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	uint8 bWantsToRun : 1 = 0;

	/** Handle to the active sprint modifier */
	FMovementModifierHandle SprintModifierHandle;

	bool bIsAiming = false;
	float LastDashTime = -1.0f;
};
