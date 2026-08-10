// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Replication/LNPMassReplication.h"
#include "Replication/LNPMassReplicator.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassReplicationFragments.h"
#include "Net/UnrealNetwork.h"

void LNP::Replication::ConfigureParams(FMassReplicationParameters& Params, const float CullDistance)
{
	Params.BubbleInfoClass = ALNPMassClientBubbleInfo::StaticClass();
	Params.ReplicatorClass = ULNPMassReplicator::StaticClass();

	// High/Medium/Low 경계는 엔진 기본값 유지 — 이 세 티어는 갱신 주기(UpdateInterval)만 좌우한다.
	// Off 경계만 시각화 거리에 맞춰 밀어낸다: 엔진 기본 5,000cm는 반지름 25,000cm 월드에서 너무 좁아
	// 클라이언트가 서버보다 훨씬 가까이 가야만 엔티티(빛기둥·NPC)를 받는 원인이었다.
	Params.LODDistance[EMassLOD::High]   = 0.f;
	Params.LODDistance[EMassLOD::Medium] = 1000.f;
	Params.LODDistance[EMassLOD::Low]    = 2500.f;
	Params.LODDistance[EMassLOD::Off]    = CullDistance;

	// 개수 캡은 거리 컬링을 무력화하지 않도록 넉넉히 열어둔다 (엔진 기본 Low=300은 거리보다 먼저 걸린다).
	// 가시 범위 제어는 CullDistance 하나로 일원화하고, 이 값들은 폭주 방지 안전망으로만 남긴다.
	Params.LODMaxCountPerViewer[EMassLOD::High]   = 500;
	Params.LODMaxCountPerViewer[EMassLOD::Medium] = 2000;
	Params.LODMaxCountPerViewer[EMassLOD::Low]    = 10000;
	Params.LODMaxCountPerViewer[EMassLOD::Off]    = 0;
}

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FLNPMassClientBubbleHandler::ApplyReplicatedTransform(FTransformFragment& TransformFragment, const FReplicatedAgentPositionYawData& PositionYaw)
{
	FTransform& Transform = TransformFragment.GetMutableTransform();
	Transform.SetLocation(PositionYaw.GetPosition());
	Transform.SetRotation(LNP::Replication::DecodeSphereRotation(PositionYaw.GetPosition(), PositionYaw.GetYaw()));
}

void FLNPMassClientBubbleHandler::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	// 스폰 쿼리에는 공통 Fragment(Transform)만 요구한다. 타입 전용 Fragment를 하드 요구사항으로
	// 넣으면 그 Fragment가 없는 아키타입 청크가 순회에서 통째로 빠져 NetID 기록이 누락되고
	// 에이전트-엔티티 대응(AgentsSpawnIdx)이 어긋난다 (EngineAnalysis_MassReplication.md §7.6).
	auto AddRequirementsForSpawnQuery = [](FMassEntityQuery& InQuery)
	{
		InQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	};

	auto CacheFragmentViewsForSpawnQuery = [this](FMassExecutionContext& InExecContext)
	{
		SpawnTransformList = InExecContext.GetMutableFragmentView<FTransformFragment>();
	};

	auto SetSpawnedEntityData = [this](const FMassEntityView& EntityView, const FLNPReplicatedAgent& ReplicatedEntity, const int32 EntityIdx)
	{
		// 스폰 시점 위치/자세 반영 — 퍼펫 링크 타입은 이 Transform으로 Actor 위치가 초기화된다.
		ApplyReplicatedTransform(SpawnTransformList[EntityIdx], ReplicatedEntity.GetReplicatedPositionYawData());

		// Enemy 아키타입만 타입 태그 기록 — 청크 필터링을 피하기 위해 EntityView로 조건부 접근한다.
		if (FLNPEnemyFragment* EnemyFragment = EntityView.GetFragmentDataPtr<FLNPEnemyFragment>())
		{
			EnemyFragment->EnemyTypeTag = ReplicatedEntity.GetEnemyTypeTag();
		}
	};

	auto SetModifiedEntityData = [](const FMassEntityView& EntityView, const FLNPReplicatedAgent& Item)
	{
		ApplyReplicatedTransform(EntityView.GetFragmentData<FTransformFragment>(), Item.GetReplicatedPositionYawData());
	};

	PostReplicatedAddHelper(AddedIndices, AddRequirementsForSpawnQuery, CacheFragmentViewsForSpawnQuery, SetSpawnedEntityData, SetModifiedEntityData);

	SpawnTransformList = TArrayView<FTransformFragment>();
}

void FLNPMassClientBubbleHandler::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	// 변경 이벤트는 서버가 Dirty 마킹한 항목만 도착한다 — 매 갱신 위치를 싣는 Enemy만 해당
	// (Player·LootPod은 스폰 이후 갱신이 없으므로 자연히 이 경로에 들어오지 않는다).
	auto SetModifiedEntityData = [](const FMassEntityView& EntityView, const FLNPReplicatedAgent& Item)
	{
		ApplyReplicatedTransform(EntityView.GetFragmentData<FTransformFragment>(), Item.GetReplicatedPositionYawData());
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
