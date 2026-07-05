// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoverDataModelTypes.h"
#include "LNPModifierInputs.generated.h"

/**
 * Guard/Sprint 의도를 Mover InputCmd에 싣기 위한 커스텀 입력 데이터.
 * bWantsToGuard/bWantsToRun을 ULNPCharacterMoverComponent의 평범한 멤버 변수로 직접 두면
 * Mover의 예측·복제·리시뮬레이션 파이프라인을 타지 않는다 (Jump가 FCharacterDefaultInputs::bIsJumpJustPressed로
 * InputCmd를 통해 전달되는 것과 대조됨). ULNPInputHandlerComponent::OnProduceInput에서 매 틱 기록하고,
 * ULNPCharacterMoverComponent::OnMoverPreSimulationTick에서 InputCmd로부터 읽는다.
 */
USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPModifierInputs : public FMoverDataStructBase
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	bool bWantsToGuard = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	bool bWantsToSprint = false;

	virtual FMoverDataStructBase* Clone() const override
	{
		return new FLNPModifierInputs(*this);
	}

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
	{
		Ar.SerializeBits(&bWantsToGuard, 1);
		Ar.SerializeBits(&bWantsToSprint, 1);
		bOutSuccess = true;
		return true;
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FLNPModifierInputs::StaticStruct();
	}

	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override
	{
		const FLNPModifierInputs& Authority = static_cast<const FLNPModifierInputs&>(AuthorityState);
		return bWantsToGuard != Authority.bWantsToGuard || bWantsToSprint != Authority.bWantsToSprint;
	}
};

template<>
struct TStructOpsTypeTraits<FLNPModifierInputs> : public TStructOpsTypeTraitsBase2<FLNPModifierInputs>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
