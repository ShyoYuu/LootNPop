// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Character/LNPPlayerReplicator.h"
#include "Character/LNPPlayerReplication.h"

void ULNPPlayerReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
{
	ProcessSpawnOnly<ALNPPlayerClientBubbleInfo, FLNPPlayerFastArrayItem>(Context, ReplicationContext,
		[](ALNPPlayerClientBubbleInfo& BubbleInfo) -> FLNPPlayerClientBubbleHandler&
		{
			return BubbleInfo.GetPlayerSerializer().Bubble;
		});
}
