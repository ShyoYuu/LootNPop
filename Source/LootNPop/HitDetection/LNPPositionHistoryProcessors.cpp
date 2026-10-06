// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPPositionHistoryProcessors.h"
#include "HitDetection/LNPPositionHistoryFragment.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Replication/LNPMassReplication.h"

ULNPPositionHistoryRecordProcessor::ULNPPositionHistoryRecordProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(TEXT("LNPEnemyMovementProcessor"));
	ExecutionOrder.ExecuteAfter.Add(TEXT("LNPEnemyFlightMovementProcessor"));
	ExecutionOrder.ExecuteAfter.Add(TEXT("LNPEnemyCrawlMovementProcessor"));
}

void ULNPPositionHistoryRecordProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FLNPPositionHistoryFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FLNPEnemyIdleFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::Optional);
	EntityQuery.RegisterWithProcessor(*this);
}

void ULNPPositionHistoryRecordProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World || World->GetNetMode() >= NM_Client)
		return; // 서버 전용 — Lag Compensation은 서버 판정에서만 사용한다.

	const double Now = World->GetTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FLNPPositionHistoryFragment>   Histories  = Ctx.GetMutableFragmentView<FLNPPositionHistoryFragment>();
		const auto Idle = Ctx.GetFragmentView<FLNPEnemyIdleFragment>();
		const auto Enemies = Ctx.GetFragmentView<FLNPEnemyFragment>();
		const auto* Shared = Ctx.GetConstSharedFragmentPtr<FLNPEnemySharedFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPPositionHistoryFragment& History = Histories[i];
			if (Now - History.LastRecordTime < RecordInterval)
				continue;

			History.LastRecordTime = Now;
			History.Samples[History.NextWriteIdx] = { Now, Transforms[i].GetTransform().GetLocation() };
			if (Idle.IsValidIndex(i) && Enemies.IsValidIndex(i) && Idle[i].bWanderHandled && Shared && Shared->Config)
			{
				auto& Sample = History.Samples[History.NextWriteIdx];
				Sample.Wander = FLNPReplicatedWanderData::Encode(Idle[i].WanderSegment, Shared->Config->IsFlying(),
					Enemies[i].SurfaceHandle, Idle[i].WanderSegment.LocalYaw);
				Sample.SurfaceGeneration = Idle[i].WanderSegment.Generation;
				Sample.CapsuleHalfHeight = Shared->Config->CapsuleHalfHeight;
				Sample.GravityOrigin = Shared->Config->MovementConfig.GravityOrigin;
			}
			History.NextWriteIdx = (History.NextWriteIdx + 1) % FLNPPositionHistoryFragment::MaxSamples;
			History.Count = FMath::Min(History.Count + 1, FLNPPositionHistoryFragment::MaxSamples);
		}
	});
}
