// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Replication/LNPSpawnOnlyReplication.h"
#include "LNPLootPodReplicator.generated.h"

/**
 * LootPod 엔티티의 클라이언트별 bubble 등록을 담당한다 (Phase 7).
 * LootPod은 정적이므로 Add 시 스폰 위치 1회만 싣는다 — PodState·게이지는
 * ALNPLootPod Actor 복제 채널이 담당한다 (LNPLootPodReplication.h 설계 주석 참조).
 */
UCLASS()
class LOOTNPOP_API ULNPLootPodReplicator : public ULNPSpawnOnlyReplicatorBase
{
	GENERATED_BODY()

public:
	virtual void ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext) override;
};
