// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPPositionHistoryProcessors.h"
#include "HitDetection/LNPPositionHistoryFragment.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"

ULNPPositionHistoryRecordProcessor::ULNPPositionHistoryRecordProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void ULNPPositionHistoryRecordProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FLNPPositionHistoryFragment>(EMassFragmentAccess::ReadWrite);
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

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPPositionHistoryFragment& History = Histories[i];
			if (Now - History.LastRecordTime < RecordInterval)
				continue;

			History.LastRecordTime = Now;
			History.Samples[History.NextWriteIdx] = { Now, Transforms[i].GetTransform().GetLocation() };
			History.NextWriteIdx = (History.NextWriteIdx + 1) % FLNPPositionHistoryFragment::MaxSamples;
			History.Count = FMath::Min(History.Count + 1, FLNPPositionHistoryFragment::MaxSamples);
		}
	});
}
