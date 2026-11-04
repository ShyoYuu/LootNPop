// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPHitGraceSubsystem.h"

#include "LootNPop.h"
#include "Config/LNPSettings.h"
#include "HitDetection/LNPGuardParryTypes.h"
#include "Character/LNPPlayerCharacter.h"
#include "GAS/LNPPoiseTypes.h"

#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarHitGraceEnable(
		TEXT("LNP.HitGrace.Enable"), 1,
		TEXT("Hit grace buffer. Holds hit effects on remote-client players briefly so a late-arriving dash can dodge them retroactively. 0: off (apply immediately), 1: on"),
		ECVF_Cheat);

	/** 이만큼 바뀌어야 다시 로그를 남긴다(ms). */
	constexpr int32 GLNPHitGraceLogStepMs = 10;
}

bool LNPHitGrace::IsEnabled()
{
	return CVarHitGraceEnable.GetValueOnAnyThread() != 0;
}

float LNPHitGrace::EstimateUplink(const APawn* Pawn)
{
	if (!IsEnabled() || !Pawn || Pawn->IsLocallyControlled())
		return 0.f;

	const APlayerState* PS = Pawn->GetPlayerState();
	if (!PS)
		return 0.f;

	// 패링 RTT 역보정과 같은 핑을 쓴다(GetPingInMilliseconds = RTT). 입력 버퍼 여유는 지연이 없어도 남는 업링크다.
	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	const float HalfRTT = PS->GetPingInMilliseconds() * 0.0005f;
	return FMath::Clamp(HalfRTT + Settings->HitGraceInputAllowance, 0.f, Settings->HitGraceMax);
}

float LNPHitGrace::ComputeHoldSeconds(const float Uplink)
{
	if (Uplink <= 0.f)
		return 0.f;

	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	return FMath::Min(Uplink + Settings->HitGraceJitterMargin, Settings->HitGraceMax);
}

void LNPHitGrace::Dispatch(FMassExecutionContext& Ctx, const FLNPParryStateFragment& PS, FLNPPoiseFragment* Poise,
	const FMassEntityHandle Defender, AActor* Victim, const TCHAR* Path, const bool bJustEligible, FLNPPlayerHitEffects&& Effects, const double Now)
{
	if (PS.HitGraceSeconds <= 0.f)
	{
		// 유예 전과 같은 일을 같은 자리에서 한다 — 경직은 Worker Thread에서 바로 쌓고, 나머지는 지연 커맨드다.
		LNPPoise::Accumulate(Poise, Effects.PoiseDamage, Now, Effects.PoiseMultiplier);
		if (Effects.GuardBlock)
			Ctx.Defer().PushCommand<FLNPGuardBlockCommand>(*Effects.GuardBlock);
		if (Effects.ImpactCue)
			Ctx.Defer().PushCommand<FLNPImpactCueCommand>(*Effects.ImpactCue);
		if (Effects.EntityHitStopAttacker.IsSet())
			Ctx.Defer().PushCommand<FLNPEntityHitStopCommand>(Effects.EntityHitStopAttacker);
		if (Effects.Damage)
			Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(*Effects.Damage);
		if (Effects.Burn)
			Ctx.Defer().PushCommand<FLNPApplyPlayerBurnGECommand>(*Effects.Burn);
		return;
	}

	FLNPHeldPlayerHit Hit;
	Hit.Defender      = Defender;
	Hit.Victim        = Victim;
	Hit.Path          = Path;
	Hit.bJustEligible = bJustEligible;
	Hit.HitTime       = Now;
	Hit.ReleaseTime   = Now + PS.HitGraceSeconds;
	Hit.Effects       = MoveTemp(Effects);
	Ctx.Defer().PushCommand<FLNPHoldPlayerHitCommand>(MoveTemp(Hit));
}

void LNPHitGrace::ApplyHeld(FMassEntityManager& EntityManager, UMassActorSubsystem* ActorSub, const FLNPHeldPlayerHit& Hit, const double Now)
{
	// 보류 중 리스폰했거나(새 엔티티) 먼저 풀린 피격으로 죽었으면 버린다 — 즉시 경로에서는 판정 쿼리가 사체를 뺀다.
	if (!EntityManager.IsEntityActive(Hit.Defender))
		return;
	const ALNPPlayerCharacter* VictimPawn = Cast<ALNPPlayerCharacter>(Hit.Victim.Get());
	if (!VictimPawn || VictimPawn->IsDead())
		return;

	if (LNPGuardParry::IsDodgeDebugEnabled())
		UE_LOG(LogLootNPop, Log, TEXT("[Dodge] Grace release path=%s player=%s held=%.0f"),
			Hit.Path, *VictimPawn->GetName(), (Now - Hit.HitTime) * 1000.0);

	// 경직의 LastHitTime은 실제로 쌓이는 지금이다 — 회복 유예가 보이는 피격 시점과 맞는다.
	const FLNPPlayerHitEffects& E = Hit.Effects;
	LNPPoise::Accumulate(EntityManager.GetFragmentDataPtr<FLNPPoiseFragment>(Hit.Defender), E.PoiseDamage, Now, E.PoiseMultiplier);
	if (E.GuardBlock)
		FLNPGuardBlockCommand::ApplyEntry(*E.GuardBlock);
	if (E.ImpactCue)
		FLNPImpactCueCommand::ApplyEntry(EntityManager, ActorSub, *E.ImpactCue);
	if (E.EntityHitStopAttacker.IsSet())
		FLNPEntityHitStopCommand::Apply(EntityManager, E.EntityHitStopAttacker);
	if (E.Damage)
		FLNPApplyDamageGECommand::ApplyEntry(*E.Damage);
	if (E.Burn)
		FLNPApplyPlayerBurnGECommand::ApplyEntry(*E.Burn);
}

void FLNPHoldPlayerHitCommand::Run(FMassEntityManager& EntityManager)
{
	UWorld* World = EntityManager.GetWorld();
	ULNPHitGraceSubsystem* Sub = World ? World->GetSubsystem<ULNPHitGraceSubsystem>() : nullptr;
	for (FLNPHeldPlayerHit& Hit : Entries)
	{
		// 서브시스템이 없는 월드 유형이면 피격을 잃지 않게 바로 적용한다.
		if (Sub)
			Sub->Enqueue(MoveTemp(Hit));
		else
			LNPHitGrace::ApplyHeld(EntityManager, World ? World->GetSubsystem<UMassActorSubsystem>() : nullptr, Hit, World ? World->GetTimeSeconds() : Hit.HitTime);
	}
}

void ULNPHitGraceSubsystem::Enqueue(FLNPHeldPlayerHit&& Hit)
{
	Held.Add(MoveTemp(Hit));
}

void ULNPHitGraceSubsystem::ResolveDash(const APawn* Pawn, const double PressTime, const float IFrameDuration, const float JustWindow)
{
	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSub = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!Pawn || !MassSub || Held.IsEmpty())
		return;

	FMassEntityManager& EntityManager = MassSub->GetMutableEntityManager();
	const double Now = World->GetTimeSeconds();

	// 들어온 순서(= 판정 순서)대로 본다 — 저스트 회피는 대시당 한 번이라 가장 이른 대상 피격이 가져간다(J-011).
	for (int32 i = 0; i < Held.Num();)
	{
		const FLNPHeldPlayerHit& Hit = Held[i];
		if (Hit.Victim.Get() != Pawn || !EntityManager.IsEntityActive(Hit.Defender))
		{
			++i;
			continue;
		}

		// 소비 플래그는 매번 다시 읽는다 — 바로 앞 묶음의 성공 처리가 세웠을 수 있다.
		const FLNPParryStateFragment* PS = EntityManager.GetFragmentDataPtr<FLNPParryStateFragment>(Hit.Defender);
		const ELNPPlayerDefenseResult Result = LNPGuardParry::JudgeRetro(Hit.HitTime, PressTime, IFrameDuration, JustWindow,
			Hit.bJustEligible, PS && PS->bJustDodgeConsumed);
		if (Result == ELNPPlayerDefenseResult::None)
		{
			++i;
			continue;
		}

		if (Result == ELNPPlayerDefenseResult::JustDodged)
		{
			FLNPJustDodgeCommand::ApplyEntry(EntityManager, { Hit.Defender, Hit.Victim, Hit.Path });
			LNPGuardParry::ReportRetroDodge(TEXT("JustDodged"), Hit.Path, Pawn, Now - Hit.HitTime);
		}
		else
		{
			LNPGuardParry::ReportRetroDodge(TEXT("Dodged"), Hit.Path, Pawn, Now - Hit.HitTime);
		}
		Held.RemoveAt(i, EAllowShrinking::No);
	}
}

void ULNPHitGraceSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	MirrorHoldSeconds(*World);
	ReleaseDue(*World);
}

void ULNPHitGraceSubsystem::MirrorHoldSeconds(UWorld& World)
{
	for (FConstPlayerControllerIterator It = World.GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC   = It->Get();
		const APawn*             Pawn = PC ? PC->GetPawn() : nullptr;
		FLNPParryStateFragment*  PF   = LNPGuardParry::FindFragment(Pawn);
		if (!PF)
			continue;

		// 유예가 꺼졌거나 로컬 조종이면 0을 쓴다 — CVar를 끄면 다음 틱부터 지금까지의 즉시 처리로 돌아간다.
		const float Uplink = LNPHitGrace::EstimateUplink(Pawn);
		PF->HitGraceSeconds = LNPHitGrace::ComputeHoldSeconds(Uplink);

		if (LNPGuardParry::IsDodgeDebugEnabled())
		{
			const int32 HoldMs = FMath::RoundToInt32(PF->HitGraceSeconds * 1000.f);
			const int32* Logged = LoggedHoldMs.Find(Pawn);
			if (!Logged || FMath::Abs(*Logged - HoldMs) >= GLNPHitGraceLogStepMs || (HoldMs == 0) != (*Logged == 0))
			{
				LoggedHoldMs.Add(Pawn, HoldMs);
				const APlayerState* PS = Pawn->GetPlayerState();
				UE_LOG(LogLootNPop, Log, TEXT("[Dodge] Grace player=%s rtt=%.0f uplink=%.0f hold=%d"),
					*Pawn->GetName(), PS ? PS->GetPingInMilliseconds() : 0.f, Uplink * 1000.f, HoldMs);
			}
		}
	}
}

void ULNPHitGraceSubsystem::ReleaseDue(UWorld& World)
{
	if (Held.IsEmpty())
		return;

	UMassEntitySubsystem* MassSub = World.GetSubsystem<UMassEntitySubsystem>();
	if (!MassSub)
		return;

	// 적용이 다시 보류를 부르지는 않지만(보류는 판정 커맨드로만 들어온다), 순회 중 배열을 고치지 않도록 먼저 떼어 낸다.
	// 들어온 순서를 지켜 같은 피격자의 경직·피해가 판정 순서대로 쌓이게 한다.
	const double Now = World.GetTimeSeconds();
	TArray<FLNPHeldPlayerHit> Due;
	for (int32 i = 0; i < Held.Num();)
	{
		if (Held[i].ReleaseTime <= Now)
		{
			Due.Add(MoveTemp(Held[i]));
			Held.RemoveAt(i, EAllowShrinking::No);
		}
		else
		{
			++i;
		}
	}

	FMassEntityManager&  EntityManager = MassSub->GetMutableEntityManager();
	UMassActorSubsystem* ActorSub      = World.GetSubsystem<UMassActorSubsystem>();
	for (const FLNPHeldPlayerHit& Hit : Due)
		LNPHitGrace::ApplyHeld(EntityManager, ActorSub, Hit, Now);
}

bool ULNPHitGraceSubsystem::IsTickable() const
{
	// 판정은 서버에만 있다. 클라이언트의 프래그먼트 값은 쓰이지 않는다.
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() < NM_Client;
}

TStatId ULNPHitGraceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPHitGraceSubsystem, STATGROUP_Tickables);
}

bool ULNPHitGraceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
