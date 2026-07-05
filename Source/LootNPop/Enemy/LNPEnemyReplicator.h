// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassReplicationProcessor.h"
#include "LNPEnemyReplicator.generated.h"

/** 서버 전용: Enemy Mass 엔티티 Fragment를 읽어 클라이언트별 Bubble(FLNPEnemyClientBubbleSerializer)에 반영한다. */
UCLASS()
class LOOTNPOP_API ULNPEnemyReplicator : public UMassReplicatorBase
{
	GENERATED_BODY()

public:
	virtual void AddRequirements(FMassEntityQuery& EntityQuery) override;
	virtual void ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext) override;
};
