// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyReplication.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "Net/UnrealNetwork.h"

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FLNPEnemyClientBubbleHandler::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	auto AddRequirementsForSpawnQuery = [this](FMassEntityQuery& InQuery)
	{
		TransformHandler.AddRequirementsForSpawnQuery(InQuery);
		InQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	};

	auto CacheFragmentViewsForSpawnQuery = [this](FMassExecutionContext& InExecContext)
	{
		TransformHandler.CacheFragmentViewsForSpawnQuery(InExecContext);
		EnemyFragmentList = InExecContext.GetMutableFragmentView<FLNPEnemyFragment>();
	};

	auto SetSpawnedEntityData = [this](const FMassEntityView&, const FLNPReplicatedEnemyAgent& ReplicatedEntity, const int32 EntityIdx)
	{
		TransformHandler.SetSpawnedEntityData(EntityIdx, ReplicatedEntity.GetReplicatedPositionYawData());
		EnemyFragmentList[EntityIdx].EnemyTypeTag = ReplicatedEntity.GetEnemyTypeTag();
	};

	auto SetModifiedEntityData = [](const FMassEntityView& EntityView, const FLNPReplicatedEnemyAgent& Item)
	{
		FMassClientBubbleTransformHandler::SetModifiedEntityData(EntityView, Item.GetReplicatedPositionYawData());
	};

	PostReplicatedAddHelper(AddedIndices, AddRequirementsForSpawnQuery, CacheFragmentViewsForSpawnQuery, SetSpawnedEntityData, SetModifiedEntityData);

	TransformHandler.ClearFragmentViewsForSpawnQuery();
	EnemyFragmentList = TArrayView<FLNPEnemyFragment>();
}

void FLNPEnemyClientBubbleHandler::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	auto SetModifiedEntityData = [](const FMassEntityView& EntityView, const FLNPReplicatedEnemyAgent& Item)
	{
		FMassClientBubbleTransformHandler::SetModifiedEntityData(EntityView, Item.GetReplicatedPositionYawData());
	};

	PostReplicatedChangeHelper(ChangedIndices, SetModifiedEntityData);
}
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE

ALNPEnemyClientBubbleInfo::ALNPEnemyClientBubbleInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Serializers.Add(&EnemySerializer);
}

void ALNPEnemyClientBubbleInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	// FastArray 자체는 PushModel 대상이 아니지만 관례상 그대로 설정한다.
	DOREPLIFETIME_WITH_PARAMS_FAST(ALNPEnemyClientBubbleInfo, EnemySerializer, SharedParams);
}
