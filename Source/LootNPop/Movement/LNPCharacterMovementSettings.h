// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"
#include "LNPCharacterMovementSettings.generated.h"

/**
 * LootNPop 캐릭터용 커스텀 이동 설정.
 * Mover의 Shared Settings를 확장한다.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPCharacterMovementSettings : public UObject, public IMovementSettingsInterface
{
	GENERATED_BODY()

public:
	virtual FString GetDisplayName() const override { return TEXT("LNP Character Movement Settings"); }

	/** Sprint 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SprintSpeed = 1200.0f;

	/** Sprint 중 가속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|Movement", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SprintAcceleration = 6000.0f;
};
