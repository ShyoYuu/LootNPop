// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/LNPBurnProcessor.h"

#include "GAS/LNPBurnTypes.h"
#include "GAS/LNPDamageFormula.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "LootNPop.h"

#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassCommandBuffer.h"

namespace LNPBurn
{
	static TAutoConsoleVariable<int32> CVarBurnLog(
		TEXT("LNP.Burn.Log"), 0,
		TEXT("Log enemy burn apply/refresh/tick/expire/death-stop on the server and replicated stack changes on guests. 0: off, 1: on"),
		ECVF_Cheat);

	bool IsLogEnabled()
	{
		return CVarBurnLog.GetValueOnAnyThread() != 0;
	}
}

ULNPBurnProcessor::ULNPBurnProcessor()
	: BurnQuery(*this)
{
	// 화상은 서버 권위 누적값이다 — 게스트는 버블로 받은 중첩 수만 가진다.
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	bAutoRegisterWithProcessingPhases = true;
	// 원뿔 판정(StartPhysics)이 이번 프레임에 건 중첩을 같은 프레임에 정산한다.
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

void ULNPBurnProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	BurnQuery.AddRequirement<FLNPBurnFragment>(EMassFragmentAccess::ReadWrite);
	BurnQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	BurnQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	BurnQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	// FLNPEnemyDyingTag는 거르지 않는다 — 사망 정지(Stacks = 0)를 여기서 한다.
	BurnQuery.RegisterWithProcessor(*this);
}

void ULNPBurnProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	const bool  bLog      = LNPBurn::IsLogEnabled();

	BurnQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		TArrayView<FLNPBurnFragment>   Burns      = Ctx.GetMutableFragmentView<FLNPBurnFragment>();
		TArrayView<FLNPEnemyFragment>  Enemies    = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		TArrayView<FMassActorFragment> ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const bool bDying = Ctx.DoesArchetypeHaveTag<FLNPEnemyDyingTag>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPBurnFragment& Burn = Burns[i];
			if (Burn.Stacks == 0)
				continue;

			if (bDying)
			{
				if (bLog)
					UE_LOG(LogLootNPop, Log, TEXT("[Burn] entity=%d stopped by death (stacks=%d)"), Ctx.GetEntity(i).Index, Burn.Stacks);
				Burn = FLNPBurnFragment();
				continue;
			}

			// 틱을 먼저 정산하고 만료를 본다 — 지속시간 끝에 걸린 마지막 틱도 터진다.
			Burn.TickAccum     += DeltaTime;
			Burn.TimeRemaining -= DeltaTime;

			FLNPEnemyFragment& Enemy = Enemies[i];
			AActor* Actor = ActorFrags[i].GetMutable();
			const TSubclassOf<UGameplayEffect> EffectClass(Burn.DamageEffectClass);

			while (Burn.TickAccum >= Burn.TickInterval && Enemy.Health > 0.f)
			{
				Burn.TickAccum -= Burn.TickInterval;
				const float Damage = Burn.Stacks * Burn.DamagePerStack;

				if (Actor && EffectClass)
				{
					// 틱 피해라 HitReact 몽타주를 억제한다 (원뿔 피격과 같은 이유).
					Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Actor, EffectClass, Damage, FVector::ZeroVector,
						Actor->GetActorLocation(), /*KnockbackStrength*/ 0.f, /*bSuppressHitReact*/ true);
					if (bLog)
						UE_LOG(LogLootNPop, Log, TEXT("[Burn] entity=%d tick GE stacks=%d damage=%.1f remaining=%.2f"),
							Ctx.GetEntity(i).Index, Burn.Stacks, Damage, Burn.TimeRemaining);
				}
				else
				{
					const float HpBefore = Enemy.Health;
					Enemy.Health = FMath::Max(0.f, HpBefore - LNPDamage::ApplyDefense(Damage, Enemy.Defense));
					if (bLog)
						UE_LOG(LogLootNPop, Log, TEXT("[Burn] entity=%d tick stacks=%d damage=%.1f HP: %.1f -> %.1f remaining=%.2f"),
							Ctx.GetEntity(i).Index, Burn.Stacks, Damage, HpBefore, Enemy.Health, Burn.TimeRemaining);
				}
			}

			if (Burn.TimeRemaining <= 0.f)
			{
				if (bLog)
					UE_LOG(LogLootNPop, Log, TEXT("[Burn] entity=%d expired (stacks=%d)"), Ctx.GetEntity(i).Index, Burn.Stacks);
				Burn = FLNPBurnFragment();
			}
		}
	});
}
