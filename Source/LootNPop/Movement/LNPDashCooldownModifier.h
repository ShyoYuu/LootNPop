// Copyright LootNPop. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "LNPDashCooldownModifier.generated.h"

class UAnimMontage;

/**
 * Dash 쿨다운을 시뮬레이션 상태로 표현하는 Modifier.
 * 이동에는 아무 영향을 주지 않고 존재 자체가 "쿨다운 중"을 뜻한다 — DurationMs가 지나면 자동 소멸한다.
 *
 * 쿨다운을 컴포넌트 멤버 + 월드 시간(GetTimeSeconds)으로 두면, 서버가 원격 폰을 버퍼된 입력으로
 * 늦게 시뮬레이션하거나 롤백 후 과거 프레임을 재시뮬레이션할 때 클라이언트와 판정이 어긋나
 * 무한 리컨사일을 유발한다. Modifier는 SyncState에 실려 롤백과 함께 복원된다.
 *
 * 시뮬레이티드 프록시가 대시 몽타주를 재생하는 단서도 이 Modifier다(ULNPCharacterMoverComponent::UpdateSyncedMontageState).
 * 대시 무브(약 0.2초)는 지연 환경에서 프록시가 재시뮬레이션으로 따라잡는 동안 이미 끝나 보이지 않지만,
 * 쿨다운은 몽타주보다 길게 남아 시작 시각과 몽타주를 함께 전달한다.
 */
USTRUCT(BlueprintType)
struct FLNPDashCooldownModifier : public FMovementModifierBase
{
	GENERATED_BODY()

public:
	FLNPDashCooldownModifier();
	virtual ~FLNPDashCooldownModifier() override {}

	/** 이 대시가 고른 몽타주. 없으면 nullptr — 프록시는 재생하지 않는다. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = Mover)
	TObjectPtr<UAnimMontage> DashMontage = nullptr;

	/** 대시 시작부터 무적 종료까지의 시간(ms). 서버의 누름 추정 보정을 포함하며, 프록시의 코스메틱 판정에만 쓴다. */
	float DodgeIFrameEndOffsetMs = 0.f;

	/** 현재 시뮬레이션 시각에서 남은 무적 시간(초). 시작 전·만료·미시작이면 -1이다. */
	float GetDodgeIFrameRemainingSeconds(double CurrentSimTimeMs) const;

	// --- FMovementModifierBase 인터페이스 ---
	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	// ----------------------------------------
};

template<>
struct TStructOpsTypeTraits<FLNPDashCooldownModifier> : public TStructOpsTypeTraitsBase2<FLNPDashCooldownModifier>
{
	enum { WithCopy = true };
};
