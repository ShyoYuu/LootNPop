// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPUpdateISMProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"

#include "MassUpdateISMProcessor.h"
#include "MassVisualizationComponent.h"
#include "MassRepresentationSubsystem.h"
#include "MassRepresentationFragments.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassLODFragments.h"

ULNPUpdateISMProcessor::ULNPUpdateISMProcessor()
	: EntityQuery(*this)
{
	// 엔진 UMassUpdateISMProcessor와 같은 실행 조건·순서다.
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Representation);
	bRequiresGameThreadExecution = true;
}

void ULNPUpdateISMProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FLNPEnemyActionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional); // 센서 발광
	EntityQuery.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.SetChunkFilter(&FMassVisualizationChunkFragment::AreAnyEntitiesVisibleInChunk);
	EntityQuery.AddSharedRequirement<FMassRepresentationSubsystemSharedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UMassRepresentationSubsystem>(EMassFragmentAccess::ReadWrite);
	// 표현이 정적으로 설정된 개체는 엔진과 같이 건너뛴다.
	EntityQuery.AddTagRequirement<FMassStaticRepresentationTag>(EMassFragmentPresence::None);
}

void ULNPUpdateISMProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		UMassRepresentationSubsystem* RepresentationSubsystem = Context.GetSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
		check(RepresentationSubsystem);
		FMassInstancedStaticMeshInfoArrayView ISMInfo = RepresentationSubsystem->GetMutableInstancedStaticMeshInfos();

		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassRepresentationFragment> RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();
		const TConstArrayView<FMassRepresentationLODFragment> RepresentationLODList = Context.GetFragmentView<FMassRepresentationLODFragment>();
		const TConstArrayView<FLNPEnemyActionFragment> ActionList = Context.GetFragmentView<FLNPEnemyActionFragment>();

		// 커스텀 데이터는 ISM 단위로 전부 붙이거나 전부 빼야 한다. 비행 적의 ISM(드론 메시)에는 비행 적만 들어간다.
		const bool bSensorGlowData = Context.DoesArchetypeHaveTag<FLNPEnemyFlyingTag>() && ActionList.Num() > 0;

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTransformFragment& TransformFragment = TransformList[EntityIt];
			const FMassRepresentationLODFragment& RepresentationLOD = RepresentationLODList[EntityIt];
			FMassRepresentationFragment& Representation = RepresentationList[EntityIt];
			if (Representation.CurrentRepresentation != EMassRepresentationType::StaticMeshInstance)
				continue;

			const int32 ISMInfoIndex = Representation.StaticMeshDescHandle.ToIndex();
			if (ensureMsgf(ISMInfo.IsValidIndex(ISMInfoIndex), TEXT("Invalid handle index %u for ISMInfosView"), ISMInfoIndex))
			{
				FMassInstancedStaticMeshInfo& Info = ISMInfo[ISMInfoIndex];
				UMassUpdateISMProcessor::UpdateISMTransform(Context.GetEntity(EntityIt), Info, TransformFragment.GetTransform(),
					Representation.PrevTransform, RepresentationLOD.LODSignificance, Representation.PrevLODSignificance);

				if (bSensorGlowData)
				{
					const float SensorGlow = ActionList[EntityIt].Action == ELNPEnemyAction::Dying ? 0.f : 1.f;
					Info.AddBatchedCustomData(SensorGlow, RepresentationLOD.LODSignificance, Representation.PrevLODSignificance);
				}
			}
			Representation.PrevTransform = TransformFragment.GetTransform();
			Representation.PrevLODSignificance = RepresentationLOD.LODSignificance;
		}
	});
}
