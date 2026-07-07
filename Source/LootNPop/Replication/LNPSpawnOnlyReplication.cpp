// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Replication/LNPSpawnOnlyReplication.h"

void ULNPSpawnOnlyReplicatorBase::AddRequirements(FMassEntityQuery& EntityQuery)
{
	FMassReplicationProcessorPositionYawHandler::AddRequirements(EntityQuery);
}
