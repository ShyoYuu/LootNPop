// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Character/LNPPlayerReplicator.h"
#include "Character/LNPPlayerReplication.h"
#include "MassExecutionContext.h"

void ULNPPlayerReplicator::AddRequirements(FMassEntityQuery& EntityQuery)
{
	FMassReplicationProcessorPositionYawHandler::AddRequirements(EntityQuery);
}

void ULNPPlayerReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE
	FMassReplicationProcessorPositionYawHandler PositionYawHandler;
	FMassReplicationSharedFragment* RepSharedFrag = nullptr;

	auto CacheViewsCallback = [&RepSharedFrag, &PositionYawHandler](FMassExecutionContext& InContext)
	{
		PositionYawHandler.CacheFragmentViews(InContext);
		RepSharedFrag = &InContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
		check(RepSharedFrag);
	};

	auto AddEntityCallback = [&RepSharedFrag, &PositionYawHandler](FMassExecutionContext& InContext, const int32 EntityIdx, FLNPReplicatedPlayerAgent& InReplicatedAgent, const FMassClientHandle ClientHandle) -> FMassReplicatedAgentHandle
	{
		ALNPPlayerClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPPlayerClientBubbleInfo>(ClientHandle);

		// 스폰 시점 위치/Yaw 1회 — 퍼펫 링크 시 폰 위치 초기화용 (이후 갱신 없음)
		PositionYawHandler.AddEntity(EntityIdx, InReplicatedAgent.GetReplicatedPositionYawDataMutable());

		return BubbleInfo.GetPlayerSerializer().Bubble.AddAgent(InContext.GetEntity(EntityIdx), InReplicatedAgent);
	};

	auto ModifyEntityCallback = [](FMassExecutionContext&, const int32, const EMassLOD::Type, const double, const FMassReplicatedAgentHandle, const FMassClientHandle)
	{
		// 위치는 Mover Actor 복제가 담당 — bubble 갱신 트래픽 없음 (Phase 6.5 설계).
	};

	auto RemoveEntityCallback = [&RepSharedFrag](FMassExecutionContext& InContext, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		ALNPPlayerClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPPlayerClientBubbleInfo>(ClientHandle);
		BubbleInfo.GetPlayerSerializer().Bubble.RemoveAgentChecked(Handle);
	};

	CalculateClientReplication<FLNPPlayerFastArrayItem>(Context, ReplicationContext, CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);
#endif // UE_REPLICATION_COMPILE_SERVER_CODE
}
