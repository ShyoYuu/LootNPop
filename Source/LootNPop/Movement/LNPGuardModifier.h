// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "LNPGuardModifier.generated.h"

/**
 * 가드 중 이동 속도를 제한하는 Modifier.
 * ULNPCharacterMovementSettings의 GuardAcceleration을 적용한다.
 *
 * MaxSpeed는 건드리지 않는다 — 이 Modifier가 실어 나르는 것은 사실상 LNP.Mover.IsGuarding 태그이고,
 * GuardWalkSpeed는 FLNPMoveSpeedModifier가 그 태그를 보고 매 틱 반영한다. (ADS와 동일한 구조)
 */
USTRUCT(BlueprintType)
struct FLNPGuardModifier : public FMovementModifierBase
{
	GENERATED_BODY()

public:
	FLNPGuardModifier();
	virtual ~FLNPGuardModifier() override {}

	virtual void OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;
	virtual void OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;

	virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	virtual void GetGameplayTags(FGameplayTagContainer& InOutTags) const override;

	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
};

template<>
struct TStructOpsTypeTraits<FLNPGuardModifier> : public TStructOpsTypeTraitsBase2<FLNPGuardModifier>
{
	enum { WithCopy = true };
};
