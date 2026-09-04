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

	/**
	 * 소유 클라이언트의 크로스헤어가 가리키는 월드 좌표. 로컬 제어 플레이어 폰만 채운다
	 * (AI·시뮬레이티드 프록시는 ZeroVector로 남는다).
	 *
	 * **왜 InputCmd에 실어야 하는가:** 원거리 발사 방향의 원본은 "총구에서 크로스헤어 지점으로"인데,
	 * 그 지점은 카메라에서 쏜 트레이스의 결과라 **소유 클라이언트만 알 수 있다.** 이 값이 없으면 서버는
	 * 원격 폰에 대해 ControlRotation 방향으로만 쏘게 되고, 그 광선은 카메라 광선과 **평행**하므로
	 * 총구와 카메라의 간격만큼 **거리와 무관하게 일정하게 빗나간다** — 게스트가 조준선대로 맞혀도
	 * 서버 판정이 나지 않던 원인이다. ControlRotation 옆에 두는 이유는 그것이 이미 조준의 원본이기 때문이다.
	 *
	 * ⚠️ LockOnTarget과 같은 이유로 **ShouldReconcile에 넣지 않는다** — Mover 시뮬레이션이 읽지 않고
	 * 어빌리티가 GetLastInputCmd()로 꺼내 쓰는 전달 수단일 뿐이라, 넣으면 조준을 움직일 때마다
	 * 이동 리시뮬레이션이 돈다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "LNP|Combat")
	FVector AimTargetLocation = FVector::ZeroVector;

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
		// 조준점도 조건부다 — AI 폰과 시뮬레이티드 프록시는 비트 하나만 쓴다.
		//
		// 값이 있을 때는 **양자화해서** 보낸다. `Ar << FVector`는 배정도 24바이트인데, 이 필드는
		// 발사체 무기를 든 동안 매 폴 나가므로 그대로 두면 폰 채널에서 가장 비싼 항목이 된다.
		// 1cm 정밀도(ScaleFactor=1)로 충분한 근거:
		//   · 필요한 범위 = 행성 반경(최대 30,000. 월드 반지름을 int16에 담는 제약상 상한 32,767)
		//     + 조준 트레이스 거리(50,000) → 최악이 원점에서 약 80,000이다.
		//     하늘을 보고 아무것도 맞지 않았을 때가 그 최악이고, 그때는 보정각이 어차피 0에 가깝다.
		//   · 소비처는 총구에서 이 점으로 향하는 **방향**뿐이다. 1cm 오차의 각오차는
		//     50m에서 0.011°, 1m에서 0.57°로 발사 방향 보정 허용각에 비하면 무시할 수준이다.
		// 엔진의 최신 경로는 헤더 7비트 + 성분당 필요비트×3을 쓰므로 실제 비용은 약 7바이트다
		// (MaxBitsPerComponent 인자는 레거시 경로에서만 쓰인다).
		bool bHasAimTarget = (Ar.IsSaving() ? !AimTargetLocation.IsZero() : false);
		Ar.SerializeBits(&bHasAimTarget, 1);
		if (bHasAimTarget)
		{
			SerializePackedVector<1, 24>(AimTargetLocation, Ar);
		}
		else if (Ar.IsLoading())
		{
			AimTargetLocation = FVector::ZeroVector;
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
		AimTargetLocation = FromInputs.AimTargetLocation;
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
