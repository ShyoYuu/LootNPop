// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/LNPPoiseProcessor.h"

#include "GAS/LNPPoiseTypes.h"
#include "Config/LNPSettings.h"
#include "LNPMassUtils.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "LNPGameplayTags.h"

#include "MassExecutionContext.h"
#include "Engine/World.h"

ULNPPoiseProcessor::ULNPPoiseProcessor()
	: PoiseQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	// 히트 판정(StartPhysics)이 이번 프레임에 쌓은 경직도를 같은 프레임에 평가한다.
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

void ULNPPoiseProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	PoiseQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadWrite);
	// 사망 중인 적은 굳힐 대상이 아니다 — 랙돌 위에 경직 몽타주를 얹게 된다.
	// 플레이어 아키타입에는 이 태그가 없으므로 함께 걸러지지 않는다.
	PoiseQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	PoiseQuery.RegisterWithProcessor(*this);
}

void ULNPPoiseProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// 경직도는 서버 권위 누적값이다 — 클라이언트는 자신에게 들어오는 모든 히트를 알 수 없어 예측이 어긋난다.
	if (LNPMass::IsClientWorld(EntityManager))
		return;

	const UWorld* World = EntityManager.GetWorld();
	if (!World)
		return;

	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	const float  DeltaTime = Context.GetDeltaTimeSeconds();
	const double Now       = World->GetTimeSeconds();

	PoiseQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		TArrayView<FLNPPoiseFragment> Poises = Ctx.GetMutableFragmentView<FLNPPoiseFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPPoiseFragment& Poise = Poises[i];

			// 다운 직후 면역 — 누적만 막고(Accumulate), 나머지 흐름은 그대로 돈다.
			if (Poise.ImmunityTimeRemaining > 0.f)
				Poise.ImmunityTimeRemaining = FMath::Max(0.f, Poise.ImmunityTimeRemaining - DeltaTime);

			// 경직도를 줄이는 수단은 이 자연회복 하나뿐이다. 마지막 피격 직후 잠깐은 멈춰
			// 짧은 시간에 몰아친 연타가 실제로 쌓이게 한다.
			if (Poise.Current > 0.f
				&& (Poise.LastHitTime < 0.0 || (Now - Poise.LastHitTime) > Settings->PoiseDecayDelaySeconds))
			{
				Poise.Current = FMath::Max(0.f, Poise.Current - Settings->PoiseDecayPerSecond * DeltaTime);
			}

			const bool bWasGroggy = Poise.bIsGroggy != 0;
			const bool bNowGroggy = Poise.Current >= Poise.StaggerThreshold;

			// 그로기가 이어진 시간을 잰다 — LNPPoise::Accumulate가 이 값으로 유입 보너스를 키운다.
			// 시계로 강제 다운시키지 않는 이유는 ULNPSettings::PoiseGroggyBonusPerSecond 주석 참조.
			Poise.GroggyElapsed = bNowGroggy ? (Poise.GroggyElapsed + DeltaTime) : 0.f;

			const bool bDown = bNowGroggy && Poise.Current >= Poise.DownThreshold;

			if (bDown)
			{
				// 게이지 리셋과 면역은 **다운에만** 붙는다 — 자연회복 외의 유일한 예외다.
				Poise.Current               = 0.f;
				Poise.GroggyElapsed         = 0.f;
				Poise.bIsGroggy             = 0;
				Poise.bParryBreakPending    = 0;   // 다운은 다운 연출이다
				Poise.ImmunityTimeRemaining = Settings->PoiseDownImmunitySeconds;

				Ctx.Defer().PushCommand<FLNPStaggerCommand>(Ctx.GetEntity(i), ELNPStaggerTier::Down);
			}
			else if (bNowGroggy != bWasGroggy)
			{
				// 그로기는 고정 시간이 아니라 게이지 값에 종속된 상태다 — 진입·이탈 에지에서만 GA를 켜고 끈다.
				Poise.bIsGroggy = bNowGroggy ? 1 : 0;

				// 패링으로 유발된 그로기만 몽타주를 갈라 쓴다. 행동은 일반 그로기와 완전히 같다.
				const FGameplayTag MontageValueTag = (bNowGroggy && Poise.bParryBreakPending)
					? FGameplayTag(TAG_Montage_Value_Stagger_Parried)
					: FGameplayTag();
				Poise.bParryBreakPending = 0;   // 진입·이탈 어느 쪽이든 소비한다

				Ctx.Defer().PushCommand<FLNPStaggerCommand>(Ctx.GetEntity(i),
					bNowGroggy ? ELNPStaggerTier::Groggy : ELNPStaggerTier::None, MontageValueTag);
			}
		}
	});
}
