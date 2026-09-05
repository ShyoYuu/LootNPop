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

	// ⚠️ **반드시 Optional이다.** 이 함수는 세 리플리케이터가 공유하므로 하드 요구로 넣으면
	// 이 프래그먼트가 없는 Player·LootPod 청크가 복제 쿼리에서 통째로 빠진다.
	EntityQuery.AddRequirement<FLNPEnemyActionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void ULNPMassReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE
	FMassReplicationSharedFragment* RepSharedFrag = nullptr;
	const FMassReplicationParameters* RepParams = nullptr;
	TConstArrayView<FTransformFragment> TransformFragments;
	TConstArrayView<FLNPEnemyFragment> EnemyFragments;
	TConstArrayView<FLNPEnemyActionFragment> ActionFragments;

	auto CacheViewsCallback = [&RepSharedFrag, &RepParams, &TransformFragments, &EnemyFragments, &ActionFragments](FMassExecutionContext& InContext)
	{
		TransformFragments = InContext.GetFragmentView<FTransformFragment>();
		// Optional Fragment — Enemy가 아닌 청크는 빈 뷰가 반환된다 (청크 단위 타입 판별에 사용).
		EnemyFragments = InContext.GetFragmentView<FLNPEnemyFragment>();
		ActionFragments = InContext.GetFragmentView<FLNPEnemyActionFragment>();
		RepSharedFrag = &InContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
		check(RepSharedFrag);
		// UMassReplicationProcessor가 쿼리에 이미 걸어둔 Const Shared 요구사항이라 항상 존재한다.
		RepParams = &InContext.GetConstSharedFragment<FMassReplicationParameters>();
	};

	auto AddEntityCallback = [&RepSharedFrag, &TransformFragments, &ActionFragments](FMassExecutionContext& InContext, const int32 EntityIdx, FLNPReplicatedAgent& InReplicatedAgent, const FMassClientHandle ClientHandle) -> FMassReplicatedAgentHandle
	{
		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);

		// 스폰 시점 위치/자세 1회 — 퍼펫 링크 타입은 이 Transform으로 Actor 위치가 초기화된다.
		const FTransform& Transform = TransformFragments[EntityIdx].GetTransform();
		FLNPReplicatedPositionYawData& PositionYaw = InReplicatedAgent.GetReplicatedPositionYawDataMutable();
		PositionYaw.SetPosition(Transform.GetLocation());
		PositionYaw.SetYaw(LNP::Replication::EncodeSphereLocalYaw(Transform));

		// 행동도 여기서 시드한다. 빼먹으면 버블에 새로 들어온 적이 실제 상태와 무관하게 Idle(0)로
		// 시작하고, 다음 전이가 올 때까지 게스트에서 굳어 보인다.
		if (ActionFragments.IsValidIndex(EntityIdx))
		{
			InReplicatedAgent.SetActionAndSeq(FLNPReplicatedAgent::EncodeActionAndSeq(
				ActionFragments[EntityIdx].Action, ActionFragments[EntityIdx].Seq));
			InReplicatedAgent.SetAimPitch(ActionFragments[EntityIdx].AimPitch);
		}

		return BubbleInfo.GetAgentSerializer().Bubble.AddAgent(InContext.GetEntity(EntityIdx), InReplicatedAgent);
	};

	auto ModifyEntityCallback = [&RepSharedFrag, &RepParams, &TransformFragments, &EnemyFragments, &ActionFragments](FMassExecutionContext& InContext, const int32 EntityIdx, const EMassLOD::Type LOD, const double Time, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		// Enemy만 매 갱신 위치/자세를 반영한다 — 서버 AI가 매 틱 이동시키기 때문.
		// Player·LootPod은 스폰 이후 갱신 없음(지속 위치는 Actor 복제 채널 담당) — no-op으로 bubble 갱신 트래픽 없음.
		if (EnemyFragments.IsEmpty())
			return;

		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);
		FLNPMassClientBubbleHandler& Bubble = BubbleInfo.GetAgentSerializer().Bubble;

		FLNPMassFastArrayItem& Item = Bubble.GetAgentItem(Handle);

		// --- ① 행동 바이트를 **게이트보다 먼저** 계산한다 ---
		// 게이트를 우회할 자격이 있는지 판단해야 하므로 순서가 뒤바뀌면 안 된다. 값은 아직 쓰지 않는다.
		const bool bHasAction = ActionFragments.IsValidIndex(EntityIdx);
		const uint8 NewActionAndSeq = bHasAction
			? FLNPReplicatedAgent::EncodeActionAndSeq(ActionFragments[EntityIdx].Action, ActionFragments[EntityIdx].Seq)
			: Item.Agent.GetActionAndSeq();
		const int8 NewAimPitch = bHasAction ? ActionFragments[EntityIdx].AimPitch : Item.Agent.GetAimPitch();
		// 두 필드를 한 덩어리로 본다 — 발사 순간에 카운터와 조준각이 **함께** 확정되므로
		// 따로 판정하면 한쪽만 실린 중간 상태가 게스트에 도착할 수 있다.
		const bool bActionChanged =
			(NewActionAndSeq != Item.Agent.GetActionAndSeq() || NewAimPitch != Item.Agent.GetAimPitch());

		// --- ② 일회성 연출의 **시작만** 갱신 주기 게이트를 우회한다 ---
		// 복제 LOD가 Low(0.3초)인 거리에서 짧은 공격(총 1.0초)은 시작과 끝이 두 갱신 사이에 들어가
		// 통째로 스킵될 수 있다. 전이 카운터는 "전이가 있었다"만 알려주므로 그때는 이미 늦다.
		//
		// ⚠️ **Idle<->Move는 우회시키지 않는다.** 루프 상태는 늦게 도착해도 그림이 같은 반면,
		// 멈췄다 걷기를 반복하는 배회 개체는 전이를 초당 여러 번 만들어 갱신 수를 통제 없이
		// 밀어올린다. 이 구분 하나가 스킵과 플랩을 동시에 막는다.
		const bool bBypassGate = bActionChanged && bHasAction
			&& FLNPEnemyActionFragment::IsOneShot(ActionFragments[EntityIdx].Action);

		// 복제 LOD별 갱신 주기 게이트. 엔진은 FMassReplicationParameters::UpdateInterval을
		// 초기화만 하고 어디서도 읽지 않으므로(엔진 기본 High 0.1 / Medium 0.2 / Low 0.3초)
		// 여기서 직접 강제한다. 이 게이트가 없으면 거리와 무관하게 넷 틱마다(30Hz) Dirty가 걸린다.
		// 클라이언트는 ULNPMassSmoothingProcessor가 실측 수신 간격만큼 보간하고 그 상한이 0.5초라
		// 최장 주기(Low 0.3초)까지는 그대로 흡수된다.
		if (!bBypassGate && Time - Item.LastDirtyTime < RepParams->UpdateInterval[LOD])
			return;

		// 엔진 핸들러(SetBubblePositionYawFromTransform)와 동일한 허용 오차 기반 Dirty 마킹이되,
		// Yaw만 접평면 로컬 기준으로 인코딩한다.
		FLNPReplicatedPositionYawData& PositionYaw = Item.Agent.GetReplicatedPositionYawDataMutable();
		const FTransform& Transform = TransformFragments[EntityIdx].GetTransform();
		bool bPositionYawChanged = false;

		const FVector Position = Transform.GetLocation();
		if (!Position.Equals(PositionYaw.GetPosition(), UE::Mass::Replication::PositionReplicateTolerance))
		{
			PositionYaw.SetPosition(Position);
			bPositionYawChanged = true;
		}

		const float LocalYaw = LNP::Replication::EncodeSphereLocalYaw(Transform);
		if (FMath::Abs(FMath::FindDeltaAngleRadians(LocalYaw, PositionYaw.GetYaw())) > UE::Mass::Replication::YawReplicateTolerance)
		{
			PositionYaw.SetYaw(LocalYaw);
			bPositionYawChanged = true;
		}

		// --- ③ 이제 값을 쓴다 ---
		// ⚠️ FLNPMassFastArrayItem의 규약 — **멤버가 바뀌면 반드시 Dirty 표시할 것.** 스폰 1회인
		// TemplateID와 달리 이 필드는 매 갱신 대상이라, 여기를 빠뜨리면 "서버에선 공격하는데
		// 게스트는 가만히 서 있는" 증상이 그대로 나온다.
		if (bActionChanged)
		{
			Item.Agent.SetActionAndSeq(NewActionAndSeq);
			Item.Agent.SetAimPitch(NewAimPitch);
		}

		// 게이트 통과 시각이 아니라 **실제로 보낸 시각**을 기록한다 — 정지한 엔티티가 다시 움직이기
		// 시작할 때 한 주기를 기다리지 않고 즉시 반영되게 하려는 것이다.
		if (bPositionYawChanged || bActionChanged)
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
