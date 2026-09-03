// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Replication/LNPMassReplication.h"
#include "Replication/LNPMassReplicator.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassReplicationFragments.h"
#include "MassCommonFragments.h"
#include "MassRepresentationFragments.h"
#include "MassCommonTypes.h"
#include "Net/UnrealNetwork.h"

void LNP::Replication::ConfigureParams(FMassReplicationParameters& Params,
	const TSubclassOf<UMassReplicatorBase> ReplicatorClass, const float CullDistance)
{
	checkf(ReplicatorClass && ReplicatorClass != ULNPMassReplicator::StaticClass(),
		TEXT("Each replicated Mass type needs its own ULNPMassReplicator subclass -- see the header comment."));

	Params.BubbleInfoClass = ALNPMassClientBubbleInfo::StaticClass();
	Params.ReplicatorClass = ReplicatorClass;

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
namespace
{
	/**
	 * 보간 시간의 하한·상한(초). 실측 수신 간격을 그대로 쓰되 양극단만 잘라낸다.
	 * - 하한: 한 프레임에 몰린 두 수신이 0에 가까운 구간을 만들어 순간이동으로 보이는 것을 막는다.
	 * - 상한: 오래 정지해 있던(= Dirty가 안 걸린) 엔티티가 다시 움직일 때 실측 간격이 수십 초로
	 *   잡히는데, 그 값을 쓰면 기어가듯 움직인다. 복제 LOD의 최장 주기(Off = 0.5초)로 자른다.
	 */
	constexpr float MinSmoothingDuration = 0.05f;
	constexpr float MaxSmoothingDuration = 0.5f;
}

void FLNPMassClientBubbleHandler::ApplyReplicatedTransform(FTransformFragment& TransformFragment, const FReplicatedAgentPositionYawData& PositionYaw)
{
	FTransform& Transform = TransformFragment.GetMutableTransform();
	Transform.SetLocation(PositionYaw.GetPosition());
	Transform.SetRotation(LNP::Replication::DecodeSphereRotation(PositionYaw.GetPosition(), PositionYaw.GetYaw()));
}

void FLNPMassClientBubbleHandler::PushSmoothingTarget(const FMassEntityView& EntityView, const FReplicatedAgentPositionYawData& PositionYaw)
{
	FTransformFragment& TransformFragment = EntityView.GetFragmentData<FTransformFragment>();
	FLNPReplicatedMovementFragment* Movement = EntityView.GetFragmentDataPtr<FLNPReplicatedMovementFragment>();
	if (Movement == nullptr)
	{
		// 보간 대상이 아닌 아키타입(Player·LootPod) — 애초에 갱신을 싣지 않으므로 여기 오지 않지만,
		// 페이로드 구성이 바뀌어도 위치가 유실되지 않도록 즉시 반영으로 폴백한다.
		ApplyReplicatedTransform(TransformFragment, PositionYaw);
		return;
	}

	const FTransform& Current = TransformFragment.GetTransform();
	Movement->SourcePosition = Current.GetLocation();
	Movement->SourceRotation = Current.GetRotation();
	Movement->TargetPosition = PositionYaw.GetPosition();
	Movement->TargetRotation = LNP::Replication::DecodeSphereRotation(PositionYaw.GetPosition(), PositionYaw.GetYaw());
	Movement->BlendDuration = FMath::Clamp(Movement->TimeSinceUpdate, MinSmoothingDuration, MaxSmoothingDuration);
	Movement->TimeSinceUpdate = 0.f;
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
		FTransformFragment& TransformFragment = SpawnTransformList[EntityIdx];
		ApplyReplicatedTransform(TransformFragment, ReplicatedEntity.GetReplicatedPositionYawData());

		// 첫 프레임은 보간 없이 스폰 위치에 고정한다 (BlendDuration = 0 → Alpha = 1).
		// 여기서 초기화하지 않으면 목표가 원점이라 스폰 직후 월드 중심으로 끌려간다.
		if (FLNPReplicatedMovementFragment* Movement = EntityView.GetFragmentDataPtr<FLNPReplicatedMovementFragment>())
		{
			const FTransform& Spawned = TransformFragment.GetTransform();
			Movement->SourcePosition = Movement->TargetPosition = Spawned.GetLocation();
			Movement->SourceRotation = Movement->TargetRotation = Spawned.GetRotation();
			Movement->TimeSinceUpdate = 0.f;
			Movement->BlendDuration = 0.f;
		}

		// Enemy 아키타입만 타입 태그 기록 — 청크 필터링을 피하기 위해 EntityView로 조건부 접근한다.
		if (FLNPEnemyFragment* EnemyFragment = EntityView.GetFragmentDataPtr<FLNPEnemyFragment>())
		{
			EnemyFragment->EnemyTypeTag = ReplicatedEntity.GetEnemyTypeTag();
		}
	};

	auto SetModifiedEntityData = [](const FMassEntityView& EntityView, const FLNPReplicatedAgent& Item)
	{
		PushSmoothingTarget(EntityView, Item.GetReplicatedPositionYawData());
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
		PushSmoothingTarget(EntityView, Item.GetReplicatedPositionYawData());
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

// --- Smoothing Processor (수신 사이 보간) ---

ULNPMassSmoothingProcessor::ULNPMassSmoothingProcessor()
	: SmoothingQuery(*this)
{
	// 수신 자체가 없는 서버·Standalone에는 이 Fragment가 아키타입에 들어가지도 않는다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::Client;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::SyncWorldToMass;

	// ActorToMass 캡슐 번역기가 같은 그룹에 있다 — 순서를 못 박지 않으면 그쪽이 나중에 돌아
	// 복제 위치를 프록시 캡슐로 덮어쓴다. 반드시 뒤에 실행돼야 복제본이 이긴다.
	ExecutionOrder.ExecuteAfter.Add(TEXT("MassCapsuleTransformToMassTranslator"));
}

void ULNPMassSmoothingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	SmoothingQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	SmoothingQuery.AddRequirement<FLNPReplicatedMovementFragment>(EMassFragmentAccess::ReadWrite);
	SmoothingQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	SmoothingQuery.RegisterWithProcessor(*this);
}

void ULNPMassSmoothingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	SmoothingQuery.ForEachEntityChunk(Context, [DeltaTime](FMassExecutionContext& Ctx)
	{
		const TArrayView<FTransformFragment> Transforms = Ctx.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FLNPReplicatedMovementFragment> Movements = Ctx.GetMutableFragmentView<FLNPReplicatedMovementFragment>();
		// Optional — 시각화 트레잇이 없는 아키타입은 빈 뷰다(그 경우 Actor로 그려질 일도 없다).
		const TConstArrayView<FMassRepresentationFragment> Representations = Ctx.GetFragmentView<FMassRepresentationFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPReplicatedMovementFragment& Movement = Movements[i];

			// 다음 수신 때 실측 간격으로 쓰이므로 아래 분기와 무관하게 항상 누적한다.
			Movement.TimeSinceUpdate += DeltaTime;

			// Transform 권한을 Actor에 넘기는 조건은 "Actor가 **그려지고 있는가**"다.
			// "Actor가 붙어 있는가"로 판단하면 안 된다 — 적 Actor는 복제 대상이라(bReplicates=true)
			// 호스트가 승격시킨 순간 게스트에도 시뮬레이티드 프록시로 내려오고, 게스트가 멀어서
			// ISM으로 그리는 동안에도 FMassActorFragment는 채워져 있다. 그때 권한을 넘기면
			// ActorToMass 번역기가 프록시 캡슐을 Transform에 되써서 ISM이 프록시를 따라
			// 지면에 파묻힌다.
			if (!Representations.IsEmpty())
			{
				const EMassRepresentationType Representation = Representations[i].CurrentRepresentation;
				if (Representation == EMassRepresentationType::HighResSpawnedActor ||
					Representation == EMassRepresentationType::LowResSpawnedActor)
				{
					continue;
				}
			}

			const float Alpha = (Movement.BlendDuration > 0.f)
				? FMath::Clamp(Movement.TimeSinceUpdate / Movement.BlendDuration, 0.f, 1.f)
				: 1.f;

			// 스케일은 보존한다 (ApplyReplicatedTransform과 같은 규약).
			FTransform& Transform = Transforms[i].GetMutableTransform();
			Transform.SetLocation(FMath::Lerp(Movement.SourcePosition, Movement.TargetPosition, Alpha));
			Transform.SetRotation(FQuat::Slerp(Movement.SourceRotation, Movement.TargetRotation, Alpha).GetNormalized());
		}
	});
}
