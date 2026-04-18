// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "LNPSprintModifier.generated.h"

/**
 * Modifier that applies sprinting settings to the character.
 * Swaps MaxSpeed and Acceleration in UCommonLegacyMovementSettings using values from ULNPCharacterMovementSettings.
 */
USTRUCT(BlueprintType)
struct FLNPSprintModifier : public FMovementModifierBase
{
	GENERATED_BODY()

public:
	FLNPSprintModifier();
	virtual ~FLNPSprintModifier() override {}

	// --- FMovementModifierBase Interface ---
	virtual void OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;
	virtual void OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;
	
	virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	
	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	// ----------------------------------------
};

template<>
struct TStructOpsTypeTraits<FLNPSprintModifier> : public TStructOpsTypeTraitsBase2<FLNPSprintModifier>
{
	enum { WithCopy = true };
};
