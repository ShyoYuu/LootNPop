// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "LNPHitGraceSubsystem.generated.h"

class APawn;
class UMassActorSubsystem;
struct FMassExecutionContext;

/**
 * 플레이어 피격 한 건의 피격자 쪽 효과 묶음(Hit·Guarded). 4경로(근접·직격·스플래시·화염)가 이것을 채워
 * LNPHitGrace::Dispatch에 넘긴다. 즉시 적용과 보류가 같은 목록을 쓰게 하려는 묶음이다 —
 * 경로마다 즉시·보류 두 벌을 쓰면 한쪽에만 효과가 빠진다.
 */
struct FLNPPlayerHitEffects
{
	float PoiseDamage     = 0.f;
	float PoiseMultiplier = 1.f;   // 가드로 막으면 PoiseGuardMultiplier

	TOptional<FLNPGuardBlockCommand::FEntry>        GuardBlock;
	TOptional<FLNPImpactCueCommand::FEntry>         ImpactCue;              // 근접 임팩트(공격자 Actor HitStop 포함)
	FMassEntityHandle                               EntityHitStopAttacker;  // 순수 엔티티 공격자만 — 비어 있으면 없음
	TOptional<FLNPApplyDamageGECommand::FEntry>     Damage;                 // HitReact 큐·넉백 포함
	TOptional<FLNPApplyPlayerBurnGECommand::FEntry> Burn;
};

/** 보류 중인 피격 한 건. */
struct FLNPHeldPlayerHit
{
	FMassEntityHandle      Defender;
	TWeakObjectPtr<AActor> Victim;
	const TCHAR*           Path          = nullptr;  // "Melee" | "Projectile" | "Splash" | "Flame" — 로그용 리터럴
	bool                   bJustEligible = false;    // 근접·직격 — 소급 저스트 회피 대상
	double                 HitTime       = 0.0;      // 서버 판정 시각
	double                 ReleaseTime   = 0.0;      // HitTime + 보류 길이
	FLNPPlayerHitEffects   Effects;
};

/** 판정 프로세서(Worker Thread)가 보류 묶음을 Game Thread의 ULNPHitGraceSubsystem에 넘기는 커맨드. */
struct FLNPHoldPlayerHitCommand : public FMassBatchedCommand
{
	FLNPHoldPlayerHitCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(FLNPHeldPlayerHit&& InHit)
	{
		Entries.Add(MoveTemp(InHit));
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override;

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FLNPHeldPlayerHit> Entries;
};

/**
 * 피격 유예 버퍼 (.context/JustDodge/phases/Phase04_HitGraceBuffer.md).
 *
 * 지연이 있으면 서버가 원격 게스트의 피격(넉백)을 먼저 처리하고, 뒤늦게 도착한 대시를 CanDash()가 거부한다.
 * 그래서 원격 게스트 피격자의 효과를 짧게 보류하고, 그 사이 도착한 대시가 먼저 눌렸다고 추정되면 회피로 소급한다.
 *
 *   U = 업링크 추정(누름 → 서버 대시 실행) = RTT/2 + 입력 버퍼 여유  — 누름 시각 추정에 쓴다
 *   H = 보류 길이 = U + 지터 여유                                      — 대시가 늦게 와도 아직 보류 중이게 한다
 *
 * 둘을 나누는 이유: 누름을 E − H로 추정하면 지터 여유만큼 더 이르게 잡혀 "맞은 뒤 누른 대시"까지 소급된다.
 */
namespace LNPHitGrace
{
	/** LNP.HitGrace.Enable이 켜져 있는가. */
	LOOTNPOP_API bool IsEnabled();

	/** 서버가 본 그 폰의 업링크 추정 U(초). 로컬 조종 폰이거나 유예가 꺼져 있으면 0. Game Thread 전용. */
	LOOTNPOP_API float EstimateUplink(const APawn* Pawn);

	/** 업링크 추정에서 보류 길이 H(초)를 낸다. U가 0이면 0이다. */
	LOOTNPOP_API float ComputeHoldSeconds(float Uplink);

	/**
	 * 플레이어 피격 효과 묶음을 즉시 적용하거나 보류한다. 판정 프로세서(Worker Thread)의 Hit·Guarded 분기가 부른다.
	 * PS.HitGraceSeconds가 0이면 예전과 똑같이 경직을 그 자리에서 쌓고 각 커맨드를 넣는다 — 호스트·유예 꺼짐의 동작이 바뀌지 않는다.
	 * @param Poise  피격자 경직 프래그먼트(없으면 null). 즉시 분기에서만 쓴다 — 보류 해제는 Defender로 다시 찾는다.
	 */
	LOOTNPOP_API void Dispatch(FMassExecutionContext& Ctx, const FLNPParryStateFragment& PS, FLNPPoiseFragment* Poise,
		FMassEntityHandle Defender, AActor* Victim, const TCHAR* Path, bool bJustEligible, FLNPPlayerHitEffects&& Effects, double Now);

	/** 보류 묶음 하나를 지금 적용한다. 피격자가 사라졌거나 이미 죽었으면 버린다. Game Thread 전용. */
	LOOTNPOP_API void ApplyHeld(FMassEntityManager& EntityManager, UMassActorSubsystem* ActorSub, const FLNPHeldPlayerHit& Hit, double Now);
}

/**
 * 피격 유예의 서버 쪽 주인.
 * - 판정 프로세서(Worker Thread)가 PlayerState를 읽지 않도록 플레이어마다 보류 길이 H를
 *   FLNPParryStateFragment::HitGraceSeconds로 미러링한다.
 * - 보류된 피격을 들고 있다가 ReleaseTime이 지나면 적용한다.
 */
UCLASS()
class LOOTNPOP_API ULNPHitGraceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 보류 묶음을 받는다. FLNPHoldPlayerHitCommand(Game Thread)가 부른다. */
	void Enqueue(FLNPHeldPlayerHit&& Hit);

	/**
	 * 서버가 원격 게스트의 대시를 실행했다 — 그 게스트의 보류된 피격을 누름 추정 시각 기준으로 소급 판정한다
	 * (LNPGuardParry::JudgeRetro). 소급된 묶음은 버리고, 저스트 회피면 성공 처리를 바로 한다. 나머지는 해제 시각까지 남는다.
	 * ULNPCharacterMoverComponent::ExecuteDash의 권위·비리시뮬레이션 분기가 창을 기록한 직후 부른다.
	 */
	void ResolveDash(const APawn* Pawn, double PressTime, float IFrameDuration, float JustWindow);

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void MirrorHoldSeconds(UWorld& World);
	void ReleaseDue(UWorld& World);

	TArray<FLNPHeldPlayerHit> Held;

	/** 마지막으로 로그에 남긴 보류 길이(ms) — 핑이 조금씩 흔들릴 때마다 찍지 않으려고 둔다. */
	TMap<TWeakObjectPtr<const APawn>, int32> LoggedHoldMs;
};
