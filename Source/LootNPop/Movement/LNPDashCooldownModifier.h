// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "LNPDashCooldownModifier.generated.h"

/**
 * Dash 쿨다운을 시뮬레이션 상태로 표현하는 Modifier.
 * 이동에는 아무 영향을 주지 않고 존재 자체가 "쿨다운 중"을 뜻한다 — DurationMs가 지나면 자동 소멸한다.
 *
 * 쿨다운을 컴포넌트 멤버 + 월드 시간(GetTimeSeconds)으로 두면, 서버가 원격 폰을 버퍼된 입력으로
 * 늦게 시뮬레이션하거나 롤백 후 과거 프레임을 재시뮬레이션할 때 클라이언트와 판정이 어긋나
 * 무한 리컨사일을 유발한다. Modifier는 SyncState에 실려 롤백과 함께 복원된다.
 */
USTRUCT(BlueprintType)
struct FLNPDashCooldownModifier : public FMovementModifierBase
{
	GENERATED_BODY()

public:
	FLNPDashCooldownModifier();
	virtual ~FLNPDashCooldownModifier() override {}

	// --- FMovementModifierBase 인터페이스 ---
	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	// ----------------------------------------
};

template<>
struct TStructOpsTypeTraits<FLNPDashCooldownModifier> : public TStructOpsTypeTraitsBase2<FLNPDashCooldownModifier>
{
	enum { WithCopy = true };
};
