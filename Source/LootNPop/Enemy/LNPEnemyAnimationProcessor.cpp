// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyAnimationProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"

#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationTypes.h"
#include "MassRepresentationAnimationProcessor.h"
#include "Animation/AnimSequenceTransformProviderData.h"   // FAnimSequenceTrackAutoPlayData · EAnimSequenceTrackLoopMode

ULNPEnemyAnimationProcessor::ULNPEnemyAnimationProcessor()
	: AnimQuery(*this)
{
	// 서버는 그리지 않는다. 리슨 서버 호스트는 Client | Server 플래그를 받으므로 이 조합으로 함께 돈다
	// (엔진의 소비 프로세서도 정확히 같은 플래그다).
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	bAutoRegisterWithProcessingPhases = true;

	// ⚠️ 페이즈를 명시한다. 소비 프로세서가 ProcessingPhase를 설정하지 않아 기본값 PrePhysics로 돌기
	//    때문이며, 페이즈가 어긋나면 아래 ExecuteBefore/After가 조용히 무시된다.
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Representation;
	// CurrentRepresentation이 확정된 뒤여야 Actor가 그리는 개체를 건너뛸 수 있다.
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::VisualizationProcessing);
	ExecutionOrder.ExecuteBefore.Add(UMassConsumeInstancedSkinnedMeshAnimationProcessor::StaticClass()->GetFName());
}

void ULNPEnemyAnimationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	AnimQuery.AddRequirement<FMassRepresentationAnimationFragment>(EMassFragmentAccess::ReadWrite);
	AnimQuery.AddRequirement<FLNPEnemyActionFragment>(EMassFragmentAccess::ReadOnly);
	AnimQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	AnimQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	AnimQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	// ⚠️ FLNPEnemyDyingTag를 None으로 걸지 않는다 — 죽는 순간 쿼리에서 빠지면 Death 시퀀스를 아무도 못 건다.
	AnimQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyAnimationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	AnimQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Ctx)
	{
		const ULNPEnemyConfig* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config;
		if (Config == nullptr || Config->ActionSequences.IsEmpty())
			return;

		const TArrayView<FMassRepresentationAnimationFragment> AnimFrags   = Ctx.GetMutableFragmentView<FMassRepresentationAnimationFragment>();
		const TConstArrayView<FLNPEnemyActionFragment> Actions             = Ctx.GetFragmentView<FLNPEnemyActionFragment>();
		const TConstArrayView<FMassRepresentationFragment> Representations = Ctx.GetFragmentView<FMassRepresentationFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			// Actor로 그리는 동안은 손대지 않는다 — 그쪽 포즈의 주인은 ABP다.
			if (UE::Mass::Representation::IsValidActorRepresentation(Representations[i].CurrentRepresentation))
				continue;

			const FLNPEnemyActionFragment& ActionFrag = Actions[i];
			const FLNPEnemyActionSequences* Entry = Config->ActionSequences.Find(ActionFrag.Action);
			if (Entry == nullptr || Entry->Indices.IsEmpty())
				continue;   // 매핑이 없는 행동은 직전 시퀀스를 그대로 유지한다.

			// 변형은 전이 카운터로 고른다. 별도 장부를 두지 않는 이유는 두 가지다 —
			// Seq가 이미 전이마다 바뀌므로 연속 공격에서 인덱스가 **반드시** 달라지고(재생 보장),
			// 서버와 게스트가 같은 Seq를 보므로 두 화면이 같은 변형을 고른다.
			const int32 SequenceIndex = Entry->Indices[ActionFrag.Seq % Entry->Indices.Num()];

			FAnimSequenceTrackAutoPlayData& AnimData = AnimFrags[i].AnimData;
			AnimData.SequenceIndex = SequenceIndex;
			AnimData.Position      = 0.f;
			AnimData.PlayRate      = 1.f;
			AnimData.BlendTime     = Config->AnimBlendTime;
			// 루프 여부는 파생값이다 — "일회성 연출인가"의 단일 원본이 이미 있다.
			AnimData.LoopMode = FLNPEnemyActionFragment::IsOneShot(ActionFrag.Action)
				? EAnimSequenceTrackLoopMode::Clamp
				: EAnimSequenceTrackLoopMode::Loop;
		}
	});
}
