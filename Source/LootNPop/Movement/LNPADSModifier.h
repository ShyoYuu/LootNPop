// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "LNPADSModifier.generated.h"

/**
 * ADS(정조준) 중 이동 속도를 제한하는 Modifier.
 * ULNPCharacterMovementSettings의 ADSWalkSpeed / ADSAcceleration을 적용한다.
 *
 * 구조는 FLNPGuardModifier와 동일하다 — 이 Modifier가 실어 나르는 것은 사실상
 * LNP.Mover.IsADS 태그이고, MaxSpeed는 FLNPMoveSpeedModifier가 그 태그를 보고 매 틱 계산한다.
 * 태그가 Mover SyncState에 실리므로 리시뮬레이션에서 함께 롤백된다.
 */
USTRUCT(BlueprintType)
struct FLNPADSModifier : public FMovementModifierBase
{
	GENERATED_BODY()

public:
	FLNPADSModifier();
	virtual ~FLNPADSModifier() override {}

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
struct TStructOpsTypeTraits<FLNPADSModifier> : public TStructOpsTypeTraitsBase2<FLNPADSModifier>
{
	enum { WithCopy = true };
};
