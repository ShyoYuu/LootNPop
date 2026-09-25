// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "Subsystems/WorldSubsystem.h"
#include <atomic>
#include "LNPMassWorldCollision.generated.h"

/**
 * exact query 분류(RuntimeCollision.md "쿼리 분류"). 정확성 필수와 품질 향상용 예산을 섞지 않도록 counter를 따로 센다.
 */
enum class ELNPWorldQueryClass : uint8
{
	ProjectileMandatory,
	AirborneMandatory,
	GroundRiskFallback,
	DynamicSupportContact,
	PeriodicGroundValidation,
	DebugValidation,

	Count
};

namespace LNPWorldQuery
{
	/** 생략할 수 없는 분류인지. 앞의 네 분류가 필수다. */
	inline bool IsMandatory(const ELNPWorldQueryClass Class)
	{
		return Class < ELNPWorldQueryClass::PeriodicGroundValidation;
	}
}

/**
 * LNPWorldExact query 입력.
 *
 * self/owner 제외 규칙:
 * - Pawn(플레이어·ActorPromoted 적)은 LNPWorldExact 기본 응답이 Ignore라 제외할 필요가 없다.
 * - PureEntity와 Mass 투사체는 world collision body가 없어 자기 자신에 맞지 않는다.
 * - 그 밖에 제외가 필요한 Actor는 게임 스레드에서 미리 구한 UObject unique ID로만 넘긴다. worker는 UObject를 만지지 않는다.
 */
struct FLNPWorldQueryParams
{
	ELNPWorldQueryClass Class = ELNPWorldQueryClass::DebugValidation;
	TArray<uint32, TInlineAllocator<2>> IgnoredActorIds;

	FLNPWorldQueryParams() = default;
	explicit FLNPWorldQueryParams(const ELNPWorldQueryClass InClass) : Class(InClass) {}
};

/** worker가 받는 exact hit 결과. UObject 참조가 없는 POD다. */
struct FLNPWorldHit
{
	bool bBlockingHit = false;
	bool bStartPenetrating = false;

	/** Time은 Start→End 비율(0~1), Distance는 cm. miss면 Time=1이다. */
	float Time = 1.f;
	float Distance = 0.f;

	/** sweep이면 shape 중심의 위치, line이면 ImpactPoint와 같다. */
	FVector Location = FVector::ZeroVector;
	FVector ImpactPoint = FVector::ZeroVector;
	FVector ImpactNormal = FVector::ZeroVector;

	FLNPExactHitIdentity Identity;
};

/** ProbeSupport 입력. Up은 호출자가 정한다(내부형 구에서는 중심 방향, 동적 패널은 패널 법선). */
struct FLNPSupportProbeQuery
{
	FVector Position = FVector::ZeroVector;
	FVector Up = FVector::UpVector;
	float MaxStepUp = 50.f;
	float MaxDrop = 100.f;
	float Radius = 30.f;

	/** walkable 판정의 최소 dot(ImpactNormal, Up). 기본값은 약 45도. */
	float WalkableMinDot = 0.7071f;
};

struct FLNPSupportProbeResult
{
	/** 지지면 역할(Support)이 있고 walkable 법선이며 시작부터 겹치지 않은 hit. */
	bool bSupported = false;

	/** hit 법선이 walkable인지. 역할과 무관하다. */
	bool bWalkableNormal = false;

	FLNPWorldHit Hit;
};

/**
 * MassWorldCollision — LNPWorldExact 동기 scene query의 공통 입구(RuntimeCollision.md "MassWorldCollision").
 *
 * - 모든 스레드에서 호출할 수 있다. Mass worker에서는 동기 query만 허용된다(D-025).
 * - 결과는 POD다. hit 의미는 ULNPHitIdentitySubsystem의 게시된 snapshot으로 해석한다(D-037).
 * - 분류별 count·시간, 미해석 hit를 atomic counter로 모은다. 락 대기는 LockProbe CVar를 켰을 때만 잰다.
 * - debug draw는 MPSC 큐에 쌓고 게임 스레드 Tick에서 그린다.
 */
UCLASS()
class LOOTNPOP_API ULNPMassWorldCollisionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 모든 스레드. */
	bool RaycastWorld(const FVector& Start, const FVector& End, const FLNPWorldQueryParams& Params, FLNPWorldHit& OutHit) const;
	bool SweepSphereWorld(const FVector& Start, const FVector& End, float Radius, const FLNPWorldQueryParams& Params, FLNPWorldHit& OutHit) const;

	/** Rotation은 캡슐 축(로컬 Z)의 방향이다. */
	bool SweepCapsuleWorld(const FVector& Start, const FVector& End, const FQuat& Rotation, float Radius, float HalfHeight,
		const FLNPWorldQueryParams& Params, FLNPWorldHit& OutHit) const;

	/** Position + Up*MaxStepUp에서 Position - Up*MaxDrop까지 구를 sweep해 지지면을 찾는다. 지지면이면 true. */
	bool ProbeSupport(const FLNPSupportProbeQuery& Query, const FLNPWorldQueryParams& Params, FLNPSupportProbeResult& OutResult) const;

	/** 모든 스레드. */
	uint64 GetQueryCount(const ELNPWorldQueryClass Class) const { return Stats[static_cast<int32>(Class)].Count.load(std::memory_order_relaxed); }
	uint64 GetUnknownHitCount() const { return UnknownHits.load(std::memory_order_relaxed); }

	/** 모든 스레드. 전 분류 합계의 누적 count·query 시간·락 probe 시간(ns). 프레임 차분으로 프레임당 비용을 얻는다. */
	void GetTotals(uint64& OutCount, uint64& OutQueryNs, uint64& OutLockNs) const;

	/**
	 * 모든 스레드. world collision envelope 최대 반지름(cm, RuntimeCollision.md "최외곽 반지름 안전망").
	 * 이보다 바깥에는 exact geometry가 없으므로, 바깥으로 나간 개체는 exact 판정이 이미 빗나간 것이다. 0이면 아직 source가 없다.
	 * snapshot 참조를 한 번 복사하므로 프레임마다 한 번 받아 두고 쓴다.
	 */
	float GetWorldEnvelopeRadius() const;

	/** 모든 스레드. 소비자가 envelope 밖으로 나간 개체를 종료할 때 부른다. exact 판정 누락의 신호다. */
	void NoteEnvelopeEscape() const { EnvelopeEscapes.fetch_add(1, std::memory_order_relaxed); }
	uint64 GetEnvelopeEscapeCount() const { return EnvelopeEscapes.load(std::memory_order_relaxed); }

	/** 게임 스레드. */
	void ResetStats();
	void Report() const;

	// UTickableWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool RunQuery(const FVector& Start, const FVector& End, const FQuat& Rotation, const FCollisionShape& Shape,
		const FLNPWorldQueryParams& Params, FLNPWorldHit& OutHit) const;

	struct FDebugSegment
	{
		FVector Start;
		FVector End;
		FVector ImpactPoint;
		bool bHit;
		bool bKnown;
	};

	UPROPERTY(Transient)
	TObjectPtr<ULNPHitIdentitySubsystem> HitIdentity;

	mutable TQueue<FDebugSegment, EQueueMode::Mpsc> DebugQueue;

	/** 분류별 통계. worker가 relaxed로 더한다. */
	struct FClassStats
	{
		std::atomic<uint64> Count = 0;
		std::atomic<uint64> Hits = 0;
		std::atomic<uint64> QueryNs = 0;
		std::atomic<uint64> MaxQueryNs = 0;
		std::atomic<uint64> LockNs = 0;
	};
	mutable FClassStats Stats[static_cast<int32>(ELNPWorldQueryClass::Count)];
	mutable std::atomic<uint64> UnknownHits = 0;
	mutable std::atomic<uint64> EnvelopeEscapes = 0;
};

/** worker는 const query 함수만 호출한다. 통계는 atomic, debug draw는 MPSC 큐다. */
template<>
struct TMassExternalSubsystemTraits<ULNPMassWorldCollisionSubsystem> final
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeWrite = false,
	};
};
