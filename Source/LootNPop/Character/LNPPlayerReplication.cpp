// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Character/LNPPlayerReplication.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "Net/UnrealNetwork.h"

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FLNPPlayerClientBubbleHandler::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	auto AddRequirementsForSpawnQuery = [this](FMassEntityQuery& InQuery)
	{
		TransformHandler.AddRequirementsForSpawnQuery(InQuery);
	};

	auto CacheFragmentViewsForSpawnQuery = [this](FMassExecutionContext& InExecContext)
	{
		TransformHandler.CacheFragmentViewsForSpawnQuery(InExecContext);
	};

	// 스폰 시점 위치/Yaw 1회 반영 — 퍼펫 링크 시 엔진이 이 Transform으로 폰 위치를 초기화한다.
	// 이후 갱신은 없다 (위치는 Mover Actor 복제 채널이 담당, 엔티티는 클라 로컬에서 폰을 추종).
	auto SetSpawnedEntityData = [this](const FMassEntityView&, const FLNPReplicatedPlayerAgent& ReplicatedEntity, const int32 EntityIdx)
	{
		TransformHandler.SetSpawnedEntityData(EntityIdx, ReplicatedEntity.GetReplicatedPositionYawData());
	};

	auto SetModifiedEntityData = [](const FMassEntityView&, const FLNPReplicatedPlayerAgent&)
	{
		// 스폰 이후 서버발 갱신 없음 — no-op.
	};

	PostReplicatedAddHelper(AddedIndices, AddRequirementsForSpawnQuery, CacheFragmentViewsForSpawnQuery, SetSpawnedEntityData, SetModifiedEntityData);

	TransformHandler.ClearFragmentViewsForSpawnQuery();
}

void FLNPPlayerClientBubbleHandler::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	// 스폰 이후 Agent 데이터를 갱신하지 않으므로 변경 이벤트는 발생하지 않는다 — no-op.
}
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE

ALNPPlayerClientBubbleInfo::ALNPPlayerClientBubbleInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Serializers.Add(&PlayerSerializer);
}

void ALNPPlayerClientBubbleInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	// FastArray 자체는 PushModel 대상이 아니지만 관례상 그대로 설정한다.
	DOREPLIFETIME_WITH_PARAMS_FAST(ALNPPlayerClientBubbleInfo, PlayerSerializer, SharedParams);
}
