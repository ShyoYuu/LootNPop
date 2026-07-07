// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LootPod/LNPLootPodReplicator.h"
#include "LootPod/LNPLootPodReplication.h"

void ULNPLootPodReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
{
	ProcessSpawnOnly<ALNPLootPodClientBubbleInfo, FLNPLootPodFastArrayItem>(Context, ReplicationContext,
		[](ALNPLootPodClientBubbleInfo& BubbleInfo) -> FLNPLootPodClientBubbleHandler&
		{
			return BubbleInfo.GetLootPodSerializer().Bubble;
		});
}
