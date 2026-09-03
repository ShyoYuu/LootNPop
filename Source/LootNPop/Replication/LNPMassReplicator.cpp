// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Replication/LNPMassReplicator.h"
#include "Replication/LNPMassReplication.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassExecutionContext.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// [I-009] 조사용 임시 절제(ablation) 스위치 — 조사 종료 시 제거한다.
	// 1이면 버블의 위치/자세 갱신을 통째로 멈춘다. 켠 구간과 끈 구간의 송신량 차이가
	// 곧 "Mass 버블이 쓰는 실제 바이트"다. 추정 대신 뺄셈으로 얻는다.
	TAutoConsoleVariable<int32> CVarFreezeBubble(
		TEXT("LNP.Net.FreezeBubble"),
		0,
		TEXT("Ablation switch for bandwidth investigation. 1 = stop dirtying Mass bubble transforms.\n")
		TEXT("Entities will stop moving on clients. Server-side simulation is unaffected."),
		ECVF_Cheat);
}

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
	TConstArrayView<FTransformFragment> TransformFragments;
	TConstArrayView<FLNPEnemyFragment> EnemyFragments;

	auto CacheViewsCallback = [&RepSharedFrag, &TransformFragments, &EnemyFragments](FMassExecutionContext& InContext)
	{
		TransformFragments = InContext.GetFragmentView<FTransformFragment>();
		// Optional Fragment — Enemy가 아닌 청크는 빈 뷰가 반환된다 (청크 단위 타입 판별에 사용).
		EnemyFragments = InContext.GetFragmentView<FLNPEnemyFragment>();
		RepSharedFrag = &InContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
		check(RepSharedFrag);
	};

	auto AddEntityCallback = [&RepSharedFrag, &TransformFragments, &EnemyFragments](FMassExecutionContext& InContext, const int32 EntityIdx, FLNPReplicatedAgent& InReplicatedAgent, const FMassClientHandle ClientHandle) -> FMassReplicatedAgentHandle
	{
		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);

		// 스폰 시점 위치/자세 1회 — 퍼펫 링크 타입은 이 Transform으로 Actor 위치가 초기화된다.
		const FTransform& Transform = TransformFragments[EntityIdx].GetTransform();
		FReplicatedAgentPositionYawData& PositionYaw = InReplicatedAgent.GetReplicatedPositionYawDataMutable();
		PositionYaw.SetPosition(Transform.GetLocation());
		PositionYaw.SetYaw(LNP::Replication::EncodeSphereLocalYaw(Transform));

		if (!EnemyFragments.IsEmpty())
		{
			InReplicatedAgent.SetEnemyTypeTag(EnemyFragments[EntityIdx].EnemyTypeTag);
		}

		return BubbleInfo.GetAgentSerializer().Bubble.AddAgent(InContext.GetEntity(EntityIdx), InReplicatedAgent);
	};

	auto ModifyEntityCallback = [&RepSharedFrag, &TransformFragments, &EnemyFragments](FMassExecutionContext& InContext, const int32 EntityIdx, const EMassLOD::Type LOD, const double Time, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		// Enemy만 매 갱신 위치/자세를 반영한다 — 서버 AI가 매 틱 이동시키기 때문.
		// Player·LootPod은 스폰 이후 갱신 없음(지속 위치는 Actor 복제 채널 담당) — no-op으로 bubble 갱신 트래픽 없음.
		if (EnemyFragments.IsEmpty())
			return;

		// [I-009] 조사용 절제 스위치 — 조사 종료 시 이 두 줄을 제거한다.
		if (0 != CVarFreezeBubble.GetValueOnAnyThread())
			return;

		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);
		FLNPMassClientBubbleHandler& Bubble = BubbleInfo.GetAgentSerializer().Bubble;

		// 엔진 핸들러(SetBubblePositionYawFromTransform)와 동일한 허용 오차 기반 Dirty 마킹이되,
		// Yaw만 접평면 로컬 기준으로 인코딩한다.
		FLNPMassFastArrayItem& Item = Bubble.GetAgentItem(Handle);
		FReplicatedAgentPositionYawData& PositionYaw = Item.Agent.GetReplicatedPositionYawDataMutable();
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

		if (bMarkDirty)
		{
			Bubble.MarkItemDirty(Item);
		}
	};

	auto RemoveEntityCallback = [&RepSharedFrag](FMassExecutionContext& InContext, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);

		// 단일 버블이므로 파괴 루프(CalculateClientReplication의 AgentsData 순회)가 넘겨주는 핸들은
		// 항상 이 버블이 발급한 것 — RemoveAgentChecked가 안전하다.
		BubbleInfo.GetAgentSerializer().Bubble.RemoveAgentChecked(Handle);
	};

	CalculateClientReplication<FLNPMassFastArrayItem>(Context, ReplicationContext, CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);
#endif // UE_REPLICATION_COMPILE_SERVER_CODE
}
