// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassReplicationTransformHandlers.h"
#include "MassReplicationTypes.h"
#include "MassClientBubbleHandler.h"
#include "MassReplicationProcessor.h"
#include "MassExecutionContext.h"
#include "MassEntityView.h"
#include "LNPSpawnOnlyReplication.generated.h"

/**
 * "스폰 1회 복제" 공용 구현 — Player(Phase 6.5)·LootPod(Phase 7)이 공유하는 패턴.
 *
 * 이 패턴의 bubble은 "클라이언트 월드에 엔티티를 존재하게 만들고, NetID 퍼펫 핸드셰이크로
 * 복제된 Actor와 자동 연결시키는 것"만 담당한다. 위치/Yaw는 Add 시 1회만 싣고 이후 갱신하지
 * 않는다 — 지속적인 위치 전달은 각 타입의 Actor 복제 채널(Mover, LootPod Actor)이 담당하거나
 * 아예 불필요(정적)하기 때문이다.
 *
 * 스폰 위치를 1회 싣는 이유: 퍼펫 링크 시 엔진(UMassAgentComponent::SetEntityHandleInternal)이
 * 엔티티 Transform으로 Actor 위치를 초기화하므로, 비워두면 원점으로 튄다.
 *
 * 개체별 추가 데이터나 지속 위치 갱신이 필요한 타입(Enemy — 타입 태그 + 매 갱신 위치 반영)은
 * 이 헬퍼를 쓰지 않고 직접 구현한다 (LNPEnemyReplication.h 참조).
 */

/** 스폰 1회 복제 타입 공용 Client Bubble 핸들러. Add 시 위치/Yaw 1회 반영, 이후 no-op. */
template<typename TItem>
class TLNPSpawnOnlyBubbleHandler : public TClientBubbleHandlerBase<TItem>
{
public:
	typedef TClientBubbleHandlerBase<TItem> Super;
	typedef TMassClientBubbleTransformHandler<TItem> FTransformHandlerType;
	typedef typename TItem::FReplicatedAgentType FAgentType;

	TLNPSpawnOnlyBubbleHandler()
		: TransformHandler(*this)
	{}

#if UE_REPLICATION_COMPILE_SERVER_CODE
	const FTransformHandlerType& GetTransformHandler() const { return TransformHandler; }
	FTransformHandlerType& GetTransformHandlerMutable() { return TransformHandler; }
#endif // UE_REPLICATION_COMPILE_SERVER_CODE

protected:
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	virtual void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) override
	{
		auto AddRequirementsForSpawnQuery = [this](FMassEntityQuery& InQuery)
		{
			TransformHandler.AddRequirementsForSpawnQuery(InQuery);
		};

		auto CacheFragmentViewsForSpawnQuery = [this](FMassExecutionContext& InExecContext)
		{
			TransformHandler.CacheFragmentViewsForSpawnQuery(InExecContext);
		};

		// 스폰 시점 위치/Yaw 1회 반영 — 퍼펫 링크 시 엔진이 이 Transform으로 Actor 위치를 초기화한다.
		auto SetSpawnedEntityData = [this](const FMassEntityView&, const FAgentType& ReplicatedEntity, const int32 EntityIdx)
		{
			TransformHandler.SetSpawnedEntityData(EntityIdx, ReplicatedEntity.GetReplicatedPositionYawData());
		};

		auto SetModifiedEntityData = [](const FMassEntityView&, const FAgentType&)
		{
			// 스폰 이후 서버발 갱신 없음 — no-op.
		};

		this->PostReplicatedAddHelper(AddedIndices, AddRequirementsForSpawnQuery, CacheFragmentViewsForSpawnQuery, SetSpawnedEntityData, SetModifiedEntityData);

		TransformHandler.ClearFragmentViewsForSpawnQuery();
	}

	virtual void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) override
	{
		// 스폰 이후 Agent 데이터를 갱신하지 않으므로 변경 이벤트는 발생하지 않는다 — no-op.
	}
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE

	FTransformHandlerType TransformHandler;
};

/**
 * 스폰 1회 복제 타입 공용 서버 Replicator 베이스.
 * AddRequirements(위치/Yaw만)와 ProcessSpawnOnly 템플릿을 제공한다.
 * 파생 클래스는 ProcessClientReplication에서 자기 BubbleInfo 타입으로 ProcessSpawnOnly를 호출만 하면 된다.
 */
UCLASS(Abstract)
class LOOTNPOP_API ULNPSpawnOnlyReplicatorBase : public UMassReplicatorBase
{
	GENERATED_BODY()

public:
	virtual void AddRequirements(FMassEntityQuery& EntityQuery) override;

protected:
	/**
	 * 스폰 1회 복제의 공용 ProcessClientReplication 구현.
	 * @param GetBubble BubbleInfo에서 해당 타입의 bubble 핸들러를 꺼내는 접근자 (예: Info.GetPlayerSerializer().Bubble)
	 */
	template<typename TBubbleInfo, typename TItem, typename TGetBubble>
	void ProcessSpawnOnly(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext, TGetBubble&& GetBubble)
	{
#if UE_REPLICATION_COMPILE_SERVER_CODE
		using FAgentType = typename TItem::FReplicatedAgentType;

		FMassReplicationProcessorPositionYawHandler PositionYawHandler;
		FMassReplicationSharedFragment* RepSharedFrag = nullptr;

		auto CacheViewsCallback = [&RepSharedFrag, &PositionYawHandler](FMassExecutionContext& InContext)
		{
			PositionYawHandler.CacheFragmentViews(InContext);
			RepSharedFrag = &InContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
			check(RepSharedFrag);
		};

		auto AddEntityCallback = [&RepSharedFrag, &PositionYawHandler, &GetBubble](FMassExecutionContext& InContext, const int32 EntityIdx, FAgentType& InReplicatedAgent, const FMassClientHandle ClientHandle) -> FMassReplicatedAgentHandle
		{
			TBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<TBubbleInfo>(ClientHandle);

			// 스폰 시점 위치/Yaw 1회 — 퍼펫 링크 시 Actor 위치 초기화용 (이후 갱신 없음)
			PositionYawHandler.AddEntity(EntityIdx, InReplicatedAgent.GetReplicatedPositionYawDataMutable());

			return GetBubble(BubbleInfo).AddAgent(InContext.GetEntity(EntityIdx), InReplicatedAgent);
		};

		auto ModifyEntityCallback = [](FMassExecutionContext&, const int32, const EMassLOD::Type, const double, const FMassReplicatedAgentHandle, const FMassClientHandle)
		{
			// 스폰 이후 갱신 없음 — bubble 갱신 트래픽 없음.
		};

		auto RemoveEntityCallback = [&RepSharedFrag, &GetBubble](FMassExecutionContext& InContext, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
		{
			TBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<TBubbleInfo>(ClientHandle);
			GetBubble(BubbleInfo).RemoveAgentChecked(Handle);
		};

		CalculateClientReplication<TItem>(Context, ReplicationContext, CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);
#endif // UE_REPLICATION_COMPILE_SERVER_CODE
	}
};
