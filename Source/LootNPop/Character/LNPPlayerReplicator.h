// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Replication/LNPSpawnOnlyReplication.h"
#include "LNPPlayerReplicator.generated.h"

/**
 * Player 엔티티의 클라이언트별 bubble 등록을 담당한다 (Phase 6.5).
 * Enemy와 달리 위치를 지속 복제하지 않는다 — Add 시 스폰 위치 1회만 싣고,
 * 이후 위치는 Mover Actor 복제 채널이 담당한다 (LNPPlayerReplication.h 설계 주석 참조).
 */
UCLASS()
class LOOTNPOP_API ULNPPlayerReplicator : public ULNPSpawnOnlyReplicatorBase
{
	GENERATED_BODY()

public:
	virtual void ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext) override;
};
