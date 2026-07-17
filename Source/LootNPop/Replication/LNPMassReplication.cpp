// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Replication/LNPMassReplication.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "Net/UnrealNetwork.h"

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FLNPMassClientBubbleHandler::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	// 스폰 쿼리에는 공통 Fragment(Transform)만 요구한다. 타입 전용 Fragment를 하드 요구사항으로
	// 넣으면 그 Fragment가 없는 아키타입 청크가 순회에서 통째로 빠져 NetID 기록이 누락되고
	// 에이전트-엔티티 대응(AgentsSpawnIdx)이 어긋난다 (EngineAnalysis_MassReplication.md §7.6).
	auto AddRequirementsForSpawnQuery = [this](FMassEntityQuery& InQuery)
	{
		TransformHandler.AddRequirementsForSpawnQuery(InQuery);
	};

	auto CacheFragmentViewsForSpawnQuery = [this](FMassExecutionContext& InExecContext)
	{
		TransformHandler.CacheFragmentViewsForSpawnQuery(InExecContext);
	};

	auto SetSpawnedEntityData = [this](const FMassEntityView& EntityView, const FLNPReplicatedAgent& ReplicatedEntity, const int32 EntityIdx)
	{
		// 스폰 시점 위치/Yaw 반영 — 퍼펫 링크 타입은 이 Transform으로 Actor 위치가 초기화된다.
		TransformHandler.SetSpawnedEntityData(EntityIdx, ReplicatedEntity.GetReplicatedPositionYawData());

		// Enemy 아키타입만 타입 태그 기록 — 청크 필터링을 피하기 위해 EntityView로 조건부 접근한다.
		if (FLNPEnemyFragment* EnemyFragment = EntityView.GetFragmentDataPtr<FLNPEnemyFragment>())
		{
			EnemyFragment->EnemyTypeTag = ReplicatedEntity.GetEnemyTypeTag();
		}
	};

	auto SetModifiedEntityData = [](const FMassEntityView& EntityView, const FLNPReplicatedAgent& Item)
	{
		FMassClientBubbleTransformHandler::SetModifiedEntityData(EntityView, Item.GetReplicatedPositionYawData());
	};

	PostReplicatedAddHelper(AddedIndices, AddRequirementsForSpawnQuery, CacheFragmentViewsForSpawnQuery, SetSpawnedEntityData, SetModifiedEntityData);

	TransformHandler.ClearFragmentViewsForSpawnQuery();
}

void FLNPMassClientBubbleHandler::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	// 변경 이벤트는 서버가 Dirty 마킹한 항목만 도착한다 — 매 갱신 위치를 싣는 Enemy만 해당
	// (Player·LootPod은 스폰 이후 갱신이 없으므로 자연히 이 경로에 들어오지 않는다).
	auto SetModifiedEntityData = [](const FMassEntityView& EntityView, const FLNPReplicatedAgent& Item)
	{
		FMassClientBubbleTransformHandler::SetModifiedEntityData(EntityView, Item.GetReplicatedPositionYawData());
	};

	PostReplicatedChangeHelper(ChangedIndices, SetModifiedEntityData);
}
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE

ALNPMassClientBubbleInfo::ALNPMassClientBubbleInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Serializers.Add(&AgentSerializer);
}

void ALNPMassClientBubbleInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	// FastArray 자체는 PushModel 대상이 아니지만 관례상 그대로 설정한다.
	DOREPLIFETIME_WITH_PARAMS_FAST(ALNPMassClientBubbleInfo, AgentSerializer, SharedParams);
}
