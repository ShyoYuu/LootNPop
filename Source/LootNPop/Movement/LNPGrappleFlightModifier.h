// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "LNPGrappleFlightModifier.generated.h"

/**
 * 그래플 비행 중임을 시뮬레이션 상태로 표현하는 Modifier. 이동에는 아무 영향을 주지 않는다.
 *
 * ⚠️ **쿨다운이 아니다.** DurationMs가 비행 시간과 정확히 같아 도착하는 순간 사라지므로
 * "다시 쓸 수 없는 시간"이 0이다. 존재 이유는 오직 하나 — 그래플 입력 버퍼 창(0.05초 ≈ 3틱)이
 * 같은 그래플을 세 번 큐잉하는 것을 막는 것이다. 대시가 쿨다운 Modifier에 그 역할을 겸하게 한 것과
 * 같은 구조이되, 그래플에는 쿨다운이 없어 상태만 남겼다.
 *
 * 컴포넌트 멤버 + 월드 시간으로 두면 안 되는 이유는 FLNPDashCooldownModifier 주석과 같다 —
 * 롤백·지연 시뮬레이션에서 판정이 갈려 무한 리컨사일을 유발한다.
 */
USTRUCT(BlueprintType)
struct FLNPGrappleFlightModifier : public FMovementModifierBase
{
	GENERATED_BODY()

public:
	FLNPGrappleFlightModifier();
	virtual ~FLNPGrappleFlightModifier() override {}

	// --- FMovementModifierBase 인터페이스 ---
	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	// ----------------------------------------
};

template<>
struct TStructOpsTypeTraits<FLNPGrappleFlightModifier> : public TStructOpsTypeTraitsBase2<FLNPGrappleFlightModifier>
{
	enum { WithCopy = true };
};
