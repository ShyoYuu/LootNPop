// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoverDataModelTypes.h"
#include "LNPModifierInputs.generated.h"

/**
 * Guard/Sprint/Dash 의도를 Mover InputCmd에 싣기 위한 커스텀 입력 데이터.
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

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	bool bWantsToDash = false;

	/**
	 * Dash 방향 결정에 쓰이는 카메라 로컬 이동 입력 (X=Forward, Y=Right).
	 * 서버가 원격 폰을 시뮬레이션할 때 폰의 현재 입력을 읽으면 해당 프레임 값이 아니므로 반드시 InputCmd로 전달한다.
	 * bWantsToDash가 true인 프레임에만 의미를 가진다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	FVector DashInputIntent = FVector::ZeroVector;

	virtual FMoverDataStructBase* Clone() const override
	{
		return new FLNPModifierInputs(*this);
	}

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
	{
		Ar.SerializeBits(&bWantsToGuard, 1);
		Ar.SerializeBits(&bWantsToSprint, 1);
		Ar.SerializeBits(&bWantsToDash, 1);
		// Dash 방향은 대시를 요청한 프레임에만 필요하다 — 평상시 대역폭을 쓰지 않도록 조건부로 직렬화한다.
		if (bWantsToDash)
		{
			Ar << DashInputIntent;
		}
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
		if (bWantsToGuard != Authority.bWantsToGuard || bWantsToSprint != Authority.bWantsToSprint || bWantsToDash != Authority.bWantsToDash)
		{
			return true;
		}
		// DashInputIntent는 bWantsToDash가 false면 직렬화되지 않아 값이 의미를 갖지 않는다 — 대시 프레임에서만 비교한다.
		return bWantsToDash && !DashInputIntent.Equals(Authority.DashInputIntent);
	}

	/**
	 * NetworkPrediction의 Smoothing 서비스가 프레임 사이를 보간할 때 컬렉션 내 모든 데이터 구조체에 대해 호출한다.
	 * 오버라이드하지 않으면 기본 구현이 check(false)로 크래시난다 (특히 standalone -game 모드에서 발현).
	 * 순간적인 입력 의도 bool이므로 실제 보간은 무의미 — From 값을 그대로 스냅한다.
	 */
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override
	{
		const FLNPModifierInputs& FromInputs = static_cast<const FLNPModifierInputs&>(From);
		bWantsToGuard = FromInputs.bWantsToGuard;
		bWantsToSprint = FromInputs.bWantsToSprint;
		bWantsToDash = FromInputs.bWantsToDash;
		DashInputIntent = FromInputs.DashInputIntent;
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
