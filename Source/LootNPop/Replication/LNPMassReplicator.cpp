// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Replication/LNPMassReplicator.h"
#include "Replication/LNPMassReplication.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassExecutionContext.h"
#include "MassReplicationFragments.h"

void ULNPMassReplicator::AddRequirements(FMassEntityQuery& EntityQuery)
{
	// 위치/자세는 직접 인코딩하므로 엔진 핸들러(FMassReplicationProcessorPositionYawHandler) 대신
	// Transform Fragment를 직접 요구한다 — 월드 Yaw가 아니라 접평면 로컬 Yaw를 실어야 하기 때문.
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

	// 타입 판별용 — Optional이므로 Enemy가 아닌 아키타입(Player·LootPod)도 쿼리에 매칭된다.
	EntityQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void ULNPMassReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE
	FMassReplicationSharedFragment* RepSharedFrag = nullptr;
	const FMassReplicationParameters* RepParams = nullptr;
	TConstArrayView<FTransformFragment> TransformFragments;
	TConstArrayView<FLNPEnemyFragment> EnemyFragments;

	auto CacheViewsCallback = [&RepSharedFrag, &RepParams, &TransformFragments, &EnemyFragments](FMassExecutionContext& InContext)
	{
		TransformFragments = InContext.GetFragmentView<FTransformFragment>();
		// Optional Fragment — Enemy가 아닌 청크는 빈 뷰가 반환된다 (청크 단위 타입 판별에 사용).
		EnemyFragments = InContext.GetFragmentView<FLNPEnemyFragment>();
		RepSharedFrag = &InContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
		check(RepSharedFrag);
		// UMassReplicationProcessor가 쿼리에 이미 걸어둔 Const Shared 요구사항이라 항상 존재한다.
		RepParams = &InContext.GetConstSharedFragment<FMassReplicationParameters>();
	};

	auto AddEntityCallback = [&RepSharedFrag, &TransformFragments](FMassExecutionContext& InContext, const int32 EntityIdx, FLNPReplicatedAgent& InReplicatedAgent, const FMassClientHandle ClientHandle) -> FMassReplicatedAgentHandle
	{
		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);

		// 스폰 시점 위치/자세 1회 — 퍼펫 링크 타입은 이 Transform으로 Actor 위치가 초기화된다.
		const FTransform& Transform = TransformFragments[EntityIdx].GetTransform();
		FLNPReplicatedPositionYawData& PositionYaw = InReplicatedAgent.GetReplicatedPositionYawDataMutable();
		PositionYaw.SetPosition(Transform.GetLocation());
		PositionYaw.SetYaw(LNP::Replication::EncodeSphereLocalYaw(Transform));

		return BubbleInfo.GetAgentSerializer().Bubble.AddAgent(InContext.GetEntity(EntityIdx), InReplicatedAgent);
	};

	auto ModifyEntityCallback = [&RepSharedFrag, &RepParams, &TransformFragments, &EnemyFragments](FMassExecutionContext& InContext, const int32 EntityIdx, const EMassLOD::Type LOD, const double Time, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		// Enemy만 매 갱신 위치/자세를 반영한다 — 서버 AI가 매 틱 이동시키기 때문.
		// Player·LootPod은 스폰 이후 갱신 없음(지속 위치는 Actor 복제 채널 담당) — no-op으로 bubble 갱신 트래픽 없음.
		if (EnemyFragments.IsEmpty())
			return;

		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);
		FLNPMassClientBubbleHandler& Bubble = BubbleInfo.GetAgentSerializer().Bubble;

		FLNPMassFastArrayItem& Item = Bubble.GetAgentItem(Handle);

		// 복제 LOD별 갱신 주기 게이트. 엔진은 FMassReplicationParameters::UpdateInterval을
		// 초기화만 하고 어디서도 읽지 않으므로(엔진 기본 High 0.1 / Medium 0.2 / Low 0.3초)
		// 여기서 직접 강제한다. 이 게이트가 없으면 거리와 무관하게 넷 틱마다(30Hz) Dirty가 걸린다.
		// 클라이언트는 ULNPMassSmoothingProcessor가 실측 수신 간격만큼 보간하고 그 상한이 0.5초라
		// 최장 주기(Low 0.3초)까지는 그대로 흡수된다.
		if (Time - Item.LastDirtyTime < RepParams->UpdateInterval[LOD])
			return;

		// 엔진 핸들러(SetBubblePositionYawFromTransform)와 동일한 허용 오차 기반 Dirty 마킹이되,
		// Yaw만 접평면 로컬 기준으로 인코딩한다.
		FLNPReplicatedPositionYawData& PositionYaw = Item.Agent.GetReplicatedPositionYawDataMutable();
		const FTransform& Transform = TransformFragments[EntityIdx].GetTransform();
		bool bMarkDirty = false;

		const FVector Position = Transform.GetLocation();
		if (!Position.Equals(PositionYaw.GetPosition(), UE::Mass::Replication::PositionReplicateTolerance))
		{
			PositionYaw.SetPosition(Position);
			bMarkDirty = true;
		}

		const float LocalYaw = LNP::Replication::EncodeSphereLocalYaw(Transform);
		if (FMath::Abs(FMath::FindDeltaAngleRadians(LocalYaw, PositionYaw.GetYaw())) > UE::Mass::Replication::YawReplicateTolerance)
		{
			PositionYaw.SetYaw(LocalYaw);
			bMarkDirty = true;
		}

		// 게이트 통과 시각이 아니라 **실제로 보낸 시각**을 기록한다 — 정지한 엔티티가 다시 움직이기
		// 시작할 때 한 주기를 기다리지 않고 즉시 반영되게 하려는 것이다.
		if (bMarkDirty)
		{
			Bubble.MarkItemDirty(Item);
			Item.LastDirtyTime = Time;
		}
	};

	auto RemoveEntityCallback = [&RepSharedFrag](FMassExecutionContext& InContext, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);

		// 핸들 발급자(버블 핸들러)가 하나뿐이므로, 파괴 루프(CalculateClientReplication의 AgentsData
		// 순회)가 넘겨주는 핸들은 항상 이 핸들러가 발급한 것 — RemoveAgentChecked가 안전하다.
		// 리플리케이터가 여럿인 것과는 무관하다(제약은 발급자 수다). 근거: LNPMassReplication.h 주석.
		BubbleInfo.GetAgentSerializer().Bubble.RemoveAgentChecked(Handle);
	};

	CalculateClientReplication<FLNPMassFastArrayItem>(Context, ReplicationContext, CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);
#endif // UE_REPLICATION_COMPILE_SERVER_CODE
}
