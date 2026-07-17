// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassReplicationProcessor.h"
#include "LNPMassReplicator.generated.h"

/**
 * 통합 서버 Replicator — 월드의 모든 복제 대상 Mass 엔티티 타입을 단일 스트림으로 처리한다.
 * 타입 분기는 FLNPEnemyFragment의 Optional 요구로 청크 단위 판별:
 * - Enemy 청크: Add 시 타입 태그 + 매 갱신 위치/Yaw 반영 (서버 AI가 매 틱 이동시키므로)
 * - 그 외(Player·LootPod) 청크: Add 시 위치 1회만, 이후 갱신 없음 (구 SpawnOnly 패턴)
 * 통합 배경: LNPMassReplication.h 및 EngineAnalysis_MassReplication.md §7.1 참조.
 */
UCLASS()
class LOOTNPOP_API ULNPMassReplicator : public UMassReplicatorBase
{
	GENERATED_BODY()

public:
	virtual void AddRequirements(FMassEntityQuery& EntityQuery) override;
	virtual void ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext) override;
};
