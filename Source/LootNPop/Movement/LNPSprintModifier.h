// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "LNPSprintModifier.generated.h"

/**
 * 캐릭터에 Sprint 설정을 적용하는 Modifier.
 * ULNPCharacterMovementSettings의 SprintAcceleration을 적용한다.
 *
 * MaxSpeed는 건드리지 않는다 — 이 Modifier가 실어 나르는 것은 사실상 LNP.Mover.IsSprinting 태그이고,
 * 속도는 FLNPMoveSpeedModifier가 그 태그를 보고 매 틱 CDO 기준으로 계산한다.
 */
USTRUCT(BlueprintType)
struct FLNPSprintModifier : public FMovementModifierBase
{
	GENERATED_BODY()

public:
	FLNPSprintModifier();
	virtual ~FLNPSprintModifier() override {}

	// --- FMovementModifierBase 인터페이스 ---
	virtual void OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;
	virtual void OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;
	
	virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	virtual void GetGameplayTags(FGameplayTagContainer& InOutTags) const override;
	
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
