// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyReplicator.h"
#include "Enemy/LNPEnemyReplication.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassExecutionContext.h"

void ULNPEnemyReplicator::AddRequirements(FMassEntityQuery& EntityQuery)
{
	FMassReplicationProcessorPositionYawHandler::AddRequirements(EntityQuery);
	EntityQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly);
}

void ULNPEnemyReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE
	FMassReplicationProcessorPositionYawHandler PositionYawHandler;
	FMassReplicationSharedFragment* RepSharedFrag = nullptr;
	TConstArrayView<FLNPEnemyFragment> EnemyFragments;

	auto CacheViewsCallback = [&RepSharedFrag, &PositionYawHandler, &EnemyFragments](FMassExecutionContext& InContext)
	{
		PositionYawHandler.CacheFragmentViews(InContext);
		EnemyFragments = InContext.GetFragmentView<FLNPEnemyFragment>();
		RepSharedFrag = &InContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
		check(RepSharedFrag);
	};

	auto AddEntityCallback = [&RepSharedFrag, &PositionYawHandler, &EnemyFragments](FMassExecutionContext& InContext, const int32 EntityIdx, FLNPReplicatedEnemyAgent& InReplicatedAgent, const FMassClientHandle ClientHandle) -> FMassReplicatedAgentHandle
	{
		ALNPEnemyClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPEnemyClientBubbleInfo>(ClientHandle);

		PositionYawHandler.AddEntity(EntityIdx, InReplicatedAgent.GetReplicatedPositionYawDataMutable());
		InReplicatedAgent.SetEnemyTypeTag(EnemyFragments[EntityIdx].EnemyTypeTag);

		return BubbleInfo.GetEnemySerializer().Bubble.AddAgent(InContext.GetEntity(EntityIdx), InReplicatedAgent);
	};

	auto ModifyEntityCallback = [&RepSharedFrag, &PositionYawHandler](FMassExecutionContext& InContext, const int32 EntityIdx, const EMassLOD::Type LOD, const double Time, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		ALNPEnemyClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPEnemyClientBubbleInfo>(ClientHandle);
		FLNPEnemyClientBubbleHandler& Bubble = BubbleInfo.GetEnemySerializer().Bubble;

		// Crowd와 달리 Enemy는 서버 AI가 매 틱 이동시키므로, 스폰 시 1회가 아니라 매 갱신마다 위치/Yaw를 반영해야 한다.
		PositionYawHandler.ModifyEntity<FLNPEnemyFastArrayItem>(Handle, EntityIdx, Bubble.GetTransformHandlerMutable());
	};

	auto RemoveEntityCallback = [&RepSharedFrag](FMassExecutionContext& InContext, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		ALNPEnemyClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPEnemyClientBubbleInfo>(ClientHandle);
		BubbleInfo.GetEnemySerializer().Bubble.RemoveAgentChecked(Handle);
	};

	CalculateClientReplication<FLNPEnemyFastArrayItem>(Context, ReplicationContext, CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);
#endif // UE_REPLICATION_COMPILE_SERVER_CODE
}
