// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoverDataModelTypes.h"
#include "LNPModifierInputs.generated.h"

/**
 * Guard/Sprint/Dash/ADS 의도를 Mover InputCmd에 싣기 위한 커스텀 입력 데이터.
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
	 * ADS(정조준) 의도. 카메라·감도는 로컬 상태로 충분하지만, 대시·질주 차단은
	 * 시뮬레이션 판정이라 서버와 리시뮬레이션이 재현할 수 있도록 InputCmd에 실어야 한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	bool bWantsToADS = false;

	/**
	 * Dash 방향 결정에 쓰이는 카메라 로컬 이동 입력 (X=Forward, Y=Right).
	 * 서버가 원격 폰을 시뮬레이션할 때 폰의 현재 입력을 읽으면 해당 프레임 값이 아니므로 반드시 InputCmd로 전달한다.
	 * bWantsToDash가 true인 프레임에만 의미를 가진다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	FVector DashInputIntent = FVector::ZeroVector;

	/**
	 * AI 폰이 원하는 이동 속도(cm/s). 0 이하면 미지정 — FLNPMoveSpeedModifier가 MaxSpeed로 반영한다.
	 *
	 * **왜 InputCmd에 실어야 하는가:** 이 값을 `ULNPInputHandlerComponent`의 평범한 멤버로만 두면
	 * 서버에서만 채워지고, 클라이언트는 CDO의 `MaxSpeed`(엔진 기본 800)로 폴백한다.
	 * 이 프로젝트의 Mover는 Async 모드 + Chaos 물리 예측(`bEnablePhysicsPrediction=True`)이라
	 * **클라이언트가 적 폰의 이동을 직접 재시뮬레이션**하므로, 속도가 틀리면 서버보다 4배 빠르게
	 * 앞서 나가다 `ResimulationErrorPositionThreshold`(10cm)를 넘겨 매번 되감긴다
	 * (2026-08-28 실측: 호스트 180 vs 게스트 435~800).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "LNP|Movement")
	float AIDesiredSpeed = 0.f;

	/**
	 * 플레이어가 락온한 대상. 없으면 null.
	 *
	 * **왜 InputCmd에 실어야 하는가:** ULNPLockOnComponent의 타겟은 로컬 상태라 서버가 원격 클라이언트의
	 * 락온을 영영 알 수 없다. 근접 공격 보정이 이 값을 쓰는데, 서버만 모르면 서버는 자동 탐색으로 다른 적을
	 * 고르게 된다 — 락온은 "자동 탐색이 고른 것 말고 이 적을 치겠다"는 명시적 의사표현이라 정반대 결과다.
	 *
	 * ⚠️ 이 값은 Mover 시뮬레이션이 읽지 않는다(이동 모드·모디파이어 어느 것도 참조하지 않음).
	 * 어빌리티가 GetLastInputCmd()로 꺼내 쓰기 위한 전달 수단일 뿐이라 **ShouldReconcile에 넣지 않는다** —
	 * 넣으면 NetGUID가 아직 안 풀린 프레임마다 불필요한 이동 리시뮬레이션이 돈다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "LNP|Combat")
	TObjectPtr<AActor> LockOnTarget = nullptr;

	virtual FMoverDataStructBase* Clone() const override
	{
		return new FLNPModifierInputs(*this);
	}

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
	{
		Ar.SerializeBits(&bWantsToGuard, 1);
		Ar.SerializeBits(&bWantsToSprint, 1);
		Ar.SerializeBits(&bWantsToDash, 1);
		Ar.SerializeBits(&bWantsToADS, 1);
		// Dash 방향은 대시를 요청한 프레임에만 필요하다 — 평상시 대역폭을 쓰지 않도록 조건부로 직렬화한다.
		if (bWantsToDash)
		{
			Ar << DashInputIntent;
		}
		// AI 속도도 같은 이유로 조건부다 — 플레이어 폰은 항상 0이라 비트 하나만 쓴다.
		bool bHasAIDesiredSpeed = (AIDesiredSpeed > 0.f);
		Ar.SerializeBits(&bHasAIDesiredSpeed, 1);
		if (bHasAIDesiredSpeed)
		{
			Ar << AIDesiredSpeed;
		}
		else if (Ar.IsLoading())
		{
			AIDesiredSpeed = 0.f;
		}
		// 락온 타겟도 조건부다 — 락온하지 않은 평상시에는 비트 하나만 쓴다.
		bool bHasLockOnTarget = (Ar.IsSaving() ? (LockOnTarget != nullptr) : false);
		Ar.SerializeBits(&bHasLockOnTarget, 1);
		if (bHasLockOnTarget)
		{
			Ar << LockOnTarget;
		}
		else if (Ar.IsLoading())
		{
			LockOnTarget = nullptr;
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
		if (bWantsToGuard != Authority.bWantsToGuard || bWantsToSprint != Authority.bWantsToSprint
			|| bWantsToDash != Authority.bWantsToDash || bWantsToADS != Authority.bWantsToADS)
		{
			return true;
		}
		// 속도가 어긋나면 이동 결과가 바로 갈라지므로 반드시 재조정 대상이다.
		if (!FMath::IsNearlyEqual(AIDesiredSpeed, Authority.AIDesiredSpeed))
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
		bWantsToADS = FromInputs.bWantsToADS;
		DashInputIntent = FromInputs.DashInputIntent;
		// 속도는 StateTree가 단계적으로 바꾸는 값(0 / 배회 / 추격)이라 중간값이 의미 없다 — 함께 스냅한다.
		AIDesiredSpeed = FromInputs.AIDesiredSpeed;
		LockOnTarget = FromInputs.LockOnTarget;
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
