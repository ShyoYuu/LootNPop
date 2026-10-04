// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Replication/LNPMassReplicator.h"
#include "Replication/LNPMassReplication.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassExecutionContext.h"
#include "MassReplicationFragments.h"
#include "SurfaceNavigation/LNPLoadBaseline.h"

namespace
{
	LNP::Replication::FEnemyReplicationStats EnemyReplicationStats;
}

void LNP::Replication::ResetEnemyReplicationStats()
{
	EnemyReplicationStats = {};
}

LNP::Replication::FEnemyReplicationStats LNP::Replication::GetEnemyReplicationStats()
{
	return EnemyReplicationStats;
}

bool LNP::Replication::UpdateEnemyAgent(FLNPMassFastArrayItem& Item, const FTransform& Transform,
	const FLNPEnemyFragment& Enemy, const FLNPEnemyActionFragment* Action,
	const FLNPReplicatedWanderData& Segment, const double Time, const float UpdateInterval, const bool bReplicateWander)
{
	const FLNPReplicatedWanderData Empty;
	const FLNPReplicatedWanderData& Wander = bReplicateWander ? Segment : Empty;
	const uint8 NewAction = Action ? FLNPReplicatedAgent::EncodeActionAndSeq(Action->Action, Action->Seq) : Item.Agent.GetActionAndSeq();
	const int8 NewPitch = Action ? Action->AimPitch : Item.Agent.GetAimPitch();
	const uint8 NewHitStop = Action ? Action->HitStopSeq : Item.Agent.GetHitStopSeq();
	const uint8 NewHealth = FLNPReplicatedAgent::EncodeHealthPct(Enemy.Health, Enemy.MaxHealth);
	const bool bActionChanged = NewAction != Item.Agent.GetActionAndSeq() || NewPitch != Item.Agent.GetAimPitch();
	const bool bHitChanged = NewHitStop != Item.Agent.GetHitStopSeq();
	const bool bHealthChanged = NewHealth != Item.Agent.GetHealthPct();
	const bool bWanderChanged = Wander != Item.Agent.GetWander();
	// 구간 시작·취소와 일회성 공격/피격은 즉시 보낸다. HP·Idle/Move는 기존 주기 게이트를 유지한다.
	const bool bBypass = bWanderChanged || (Action && ((bActionChanged && FLNPEnemyActionFragment::IsOneShot(Action->Action)) || bHitChanged));
	if (!bBypass && Time - Item.LastDirtyTime < UpdateInterval)
		return false;
	bool bPoseChanged = false;
	if (!Wander.IsEnabled() || bWanderChanged)
	{
		auto& Pose = Item.Agent.GetReplicatedPositionYawDataMutable();
		const FVector Position = Transform.GetLocation();
		const float Yaw = EncodeSphereLocalYaw(Transform);
		if (!Position.Equals(Pose.GetPosition(), UE::Mass::Replication::PositionReplicateTolerance))
		{
			Pose.SetPosition(Position);
			bPoseChanged = true;
		}
		if (FMath::Abs(FMath::FindDeltaAngleRadians(Yaw, Pose.GetYaw())) > UE::Mass::Replication::YawReplicateTolerance)
		{
			Pose.SetYaw(Yaw);
			bPoseChanged = true;
		}
	}
	if (!bPoseChanged && !bActionChanged && !bHitChanged && !bHealthChanged && !bWanderChanged)
		return false;
	if (bWanderChanged)
		Item.Agent.SetWander(Wander);
	if (bActionChanged)
	{
		Item.Agent.SetActionAndSeq(NewAction);
		Item.Agent.SetAimPitch(NewPitch);
	}
	if (bHitChanged)
		Item.Agent.SetHitStopSeq(NewHitStop);
	if (bHealthChanged)
		Item.Agent.SetHealthPct(NewHealth);
	Item.LastDirtyTime = Time;
	if (LNPLoadBaseline::IsActive())
	{
		++EnemyReplicationStats.Dirty;
		EnemyReplicationStats.Pose += bPoseChanged;
		EnemyReplicationStats.Action += bActionChanged;
		EnemyReplicationStats.Hit += bHitChanged;
		EnemyReplicationStats.Health += bHealthChanged;
		EnemyReplicationStats.Wander += bWanderChanged;
	}
	return true;
}

void ULNPMassReplicator::AddRequirements(FMassEntityQuery& EntityQuery)
{
	// 위치/자세는 직접 인코딩하므로 엔진 핸들러(FMassReplicationProcessorPositionYawHandler) 대신
	// Transform Fragment를 직접 요구한다 — 월드 Yaw가 아니라 접평면 로컬 Yaw를 실어야 하기 때문.
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FLNPEnemyIdleFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

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
	TConstArrayView<FLNPEnemyIdleFragment> IdleFragments;

	auto CacheViewsCallback = [&RepSharedFrag, &RepParams, &TransformFragments, &EnemyFragments, &ActionFragments, &IdleFragments](FMassExecutionContext& InContext)
	{
		TransformFragments = InContext.GetFragmentView<FTransformFragment>();
		// Optional Fragment — Enemy가 아닌 청크는 빈 뷰가 반환된다 (청크 단위 타입 판별에 사용).
		EnemyFragments = InContext.GetFragmentView<FLNPEnemyFragment>();
		ActionFragments = InContext.GetFragmentView<FLNPEnemyActionFragment>();
		IdleFragments = InContext.GetFragmentView<FLNPEnemyIdleFragment>();
		RepSharedFrag = &InContext.GetMutableSharedFragment<FMassReplicationSharedFragment>();
		check(RepSharedFrag);
		// UMassReplicationProcessor가 쿼리에 이미 걸어둔 Const Shared 요구사항이라 항상 존재한다.
		RepParams = &InContext.GetConstSharedFragment<FMassReplicationParameters>();
	};

	auto AddEntityCallback = [&RepSharedFrag, &TransformFragments, &EnemyFragments, &ActionFragments, &IdleFragments](FMassExecutionContext& InContext, const int32 EntityIdx, FLNPReplicatedAgent& InReplicatedAgent, const FMassClientHandle ClientHandle) -> FMassReplicatedAgentHandle
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
			InReplicatedAgent.SetHitStopSeq(ActionFragments[EntityIdx].HitStopSeq);
		}

		// HP도 시드한다. 빼먹으면 이미 다친 적이 버블에 들어올 때 만피로 시작해 HP 바가 뜨지 않는다.
		if (EnemyFragments.IsValidIndex(EntityIdx))
		{
			InReplicatedAgent.SetHealthPct(FLNPReplicatedAgent::EncodeHealthPct(
				EnemyFragments[EntityIdx].Health, EnemyFragments[EntityIdx].MaxHealth));
		}

		if (!LNPLoadBaseline::IsWanderPoseControl() && EnemyFragments.IsValidIndex(EntityIdx)
			&& IdleFragments.IsValidIndex(EntityIdx) && IdleFragments[EntityIdx].bWanderHandled)
			InReplicatedAgent.SetWander(FLNPReplicatedWanderData::Encode(IdleFragments[EntityIdx].WanderSegment,
				InContext.DoesArchetypeHaveTag<FLNPEnemyFlyingTag>(), EnemyFragments[EntityIdx].SurfaceHandle,
				IdleFragments[EntityIdx].WanderSegment.LocalYaw));
		if (LNPLoadBaseline::IsActive() && !EnemyFragments.IsEmpty())
			++EnemyReplicationStats.Adds;
		return BubbleInfo.GetAgentSerializer().Bubble.AddAgent(InContext.GetEntity(EntityIdx), InReplicatedAgent);
	};

	auto ModifyEntityCallback = [&RepSharedFrag, &RepParams, &TransformFragments, &EnemyFragments, &ActionFragments, &IdleFragments](FMassExecutionContext& InContext, const int32 EntityIdx, const EMassLOD::Type LOD, const double Time, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		// Enemy만 매 갱신 위치/자세를 반영한다 — 서버 AI가 매 틱 이동시키기 때문.
		// Player·LootPod은 스폰 이후 갱신 없음(지속 위치는 Actor 복제 채널 담당) — no-op으로 bubble 갱신 트래픽 없음.
		if (EnemyFragments.IsEmpty())
			return;

		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);
		FLNPMassClientBubbleHandler& Bubble = BubbleInfo.GetAgentSerializer().Bubble;

		FLNPMassFastArrayItem& Item = Bubble.GetAgentItem(Handle);

		const FLNPReplicatedWanderData Wander = IdleFragments.IsValidIndex(EntityIdx) && IdleFragments[EntityIdx].bWanderHandled
			? FLNPReplicatedWanderData::Encode(IdleFragments[EntityIdx].WanderSegment,
				InContext.DoesArchetypeHaveTag<FLNPEnemyFlyingTag>(), EnemyFragments[EntityIdx].SurfaceHandle,
				IdleFragments[EntityIdx].WanderSegment.LocalYaw) : FLNPReplicatedWanderData();
		if (LNP::Replication::UpdateEnemyAgent(Item, TransformFragments[EntityIdx].GetTransform(), EnemyFragments[EntityIdx],
			ActionFragments.IsValidIndex(EntityIdx) ? &ActionFragments[EntityIdx] : nullptr, Wander, Time, RepParams->UpdateInterval[LOD],
			!LNPLoadBaseline::IsWanderPoseControl()))
			Bubble.MarkItemDirty(Item);
	};

	auto RemoveEntityCallback = [&RepSharedFrag, &EnemyFragments](FMassExecutionContext& InContext, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		ALNPMassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<ALNPMassClientBubbleInfo>(ClientHandle);

		// 핸들 발급자(버블 핸들러)가 하나뿐이므로, 파괴 루프(CalculateClientReplication의 AgentsData
		// 순회)가 넘겨주는 핸들은 항상 이 핸들러가 발급한 것 — RemoveAgentChecked가 안전하다.
		// 리플리케이터가 여럿인 것과는 무관하다(제약은 발급자 수다). 근거: LNPMassReplication.h 주석.
		if (LNPLoadBaseline::IsActive() && !EnemyFragments.IsEmpty())
			++EnemyReplicationStats.Removes;
		BubbleInfo.GetAgentSerializer().Bubble.RemoveAgentChecked(Handle);
	};

	CalculateClientReplication<FLNPMassFastArrayItem>(Context, ReplicationContext, CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);
#endif // UE_REPLICATION_COMPILE_SERVER_CODE
}
