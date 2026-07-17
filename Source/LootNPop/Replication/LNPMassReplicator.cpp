// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Replication/LNPMassReplicator.h"
#include "Replication/LNPMassReplication.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassExecutionContext.h"

void ULNPMassReplicator::AddRequirements(FMassEntityQuery& EntityQuery)
{
	FMassReplicationProcessorPositionYawHandler::AddRequirements(EntityQuery);

	// 타입 판별용 — Optional이므로 Enemy가 아닌 아키타입(Player·LootPod)도 쿼리에 매칭된다.
	EntityQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void ULNPMassReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE
	FMassReplicationProcessorPositionYawHandler PositionYawHandler;
	FMassReplicationSharedFragment* RepSharedFrag = nullptr;
	TConstArrayView<FLNPEnemyFragment> EnemyFragments;

	auto CacheViewsCallback = [&RepSharedFrag, &PositionYawHandler, &EnemyFragments](FMassExecutionContext& InContext)
	{
		PositionYawHandler.CacheFragmentViews(InContext);
		// Optional Fragment — Enemy가 아닌 청크는 빈 뷰가 반환된다 (청크 단위 타입 판별에 사용).
		EnemyFragments = InContext.GetFragmentView<FLNPEnemyFragment>();
		RepSharedFrag = &InContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
		check(RepSharedFrag);
	};

	auto AddEntityCallback = [&RepSharedFrag, &PositionYawHandler, &EnemyFragments](FMassExecutionContext& InContext, const int32 EntityIdx, FLNPReplicatedAgent& InReplicatedAgent, const FMassClientHandle ClientHandle) -> FMassReplicatedAgentHandle
	{
		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);

		// 스폰 시점 위치/Yaw 1회 — 퍼펫 링크 타입은 이 Transform으로 Actor 위치가 초기화된다.
		PositionYawHandler.AddEntity(EntityIdx, InReplicatedAgent.GetReplicatedPositionYawDataMutable());

		if (!EnemyFragments.IsEmpty())
		{
			InReplicatedAgent.SetEnemyTypeTag(EnemyFragments[EntityIdx].EnemyTypeTag);
		}

		return BubbleInfo.GetAgentSerializer().Bubble.AddAgent(InContext.GetEntity(EntityIdx), InReplicatedAgent);
	};

	auto ModifyEntityCallback = [&RepSharedFrag, &PositionYawHandler, &EnemyFragments](FMassExecutionContext& InContext, const int32 EntityIdx, const EMassLOD::Type LOD, const double Time, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		// Enemy만 매 갱신 위치/Yaw를 반영한다 — 서버 AI가 매 틱 이동시키기 때문.
		// Player·LootPod은 스폰 이후 갱신 없음(지속 위치는 Actor 복제 채널 담당) — no-op으로 bubble 갱신 트래픽 없음.
		if (EnemyFragments.IsEmpty())
			return;

		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);
		FLNPMassClientBubbleHandler& Bubble = BubbleInfo.GetAgentSerializer().Bubble;

		PositionYawHandler.ModifyEntity<FLNPMassFastArrayItem>(Handle, EntityIdx, Bubble.GetTransformHandlerMutable());
	};

	auto RemoveEntityCallback = [&RepSharedFrag](FMassExecutionContext& InContext, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);

		// 단일 버블이므로 파괴 루프(CalculateClientReplication의 AgentsData 순회)가 넘겨주는 핸들은
		// 항상 이 버블이 발급한 것 — RemoveAgentChecked가 안전하다.
		BubbleInfo.GetAgentSerializer().Bubble.RemoveAgentChecked(Handle);
	};

	CalculateClientReplication<FLNPMassFastArrayItem>(Context, ReplicationContext, CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);
#endif // UE_REPLICATION_COMPILE_SERVER_CODE
}
