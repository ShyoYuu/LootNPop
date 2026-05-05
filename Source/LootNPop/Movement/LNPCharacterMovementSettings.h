// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "LNPCharacterMovementSettings.generated.h"

/**
 * Custom movement settings for LootNPop characters.
 * Extends Mover's shared settings system.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPCharacterMovementSettings : public UObject, public IMovementSettingsInterface
{
	GENERATED_BODY()

public:
	virtual FString GetDisplayName() const override { return TEXT("LNP Character Movement Settings"); }

	/** Sprinting speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SprintSpeed = 1200.0f;

	/** Acceleration while sprinting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SprintAcceleration = 6000.0f;
};
