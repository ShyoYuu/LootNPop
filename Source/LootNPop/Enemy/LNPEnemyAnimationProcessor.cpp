// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyAnimationProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"

#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationTypes.h"
#include "MassRepresentationAnimationProcessor.h"
#include "MassRepresentationSubsystem.h"                    // FMassInstancedSkinnedMeshInfoArrayView
#include "MassVisualizationComponent.h"                    // GetSharedDataForDescriptionIndex — 트랙 ID 조회
#include "Components/InstancedSkinnedMeshComponent.h"      // GetTransformProvider
#include "Animation/AnimSequenceTransformProviderData.h"   // FAnimSequenceTrackAutoPlayData · EAnimSequenceTrackLoopMode

namespace
{
	/**
	 * 시퀀스 길이를 **엔진이 가진 데이터에서 읽는다.** Config에 손으로 적어 두면 애니를 갈아 끼울 때
	 * 조용히 어긋나고, 어긋난 것을 알려 줄 사람이 없다.
	 *
	 * 프로바이더까지의 경로는 표현 프래그먼트가 들고 있는 핸들 하나로 닿는다:
	 * SkinnedMeshDescHandle -> FMassInstancedSkinnedMeshInfo -> Desc.Meshes[n].TransformProvider.
	 * 무기처럼 항목이 여럿이면 첫 프로바이더가 몸통이다 — 모든 항목이 같은 애니 데이터를 받는다.
	 */
	float GetSequencePlayLength(const FMassInstancedSkinnedMeshInfoArrayView& MeshInfos,
		const FMassRepresentationFragment& Representation, const int32 SequenceIndex)
	{
		const int32 MeshInfoIndex = Representation.SkinnedMeshDescHandle.ToIndex();
		if (!MeshInfos.IsValidIndex(MeshInfoIndex))
			return 0.f;

		for (const FMassSkinnedMeshInstanceVisualizationMeshDesc& MeshDesc : MeshInfos[MeshInfoIndex].GetDesc().Meshes)
		{
			if (const UAnimSequenceTransformProviderData* Provider = Cast<UAnimSequenceTransformProviderData>(MeshDesc.TransformProvider))
				return Provider->GetSequencePlayLength(SequenceIndex);
		}
		return 0.f;
	}

	/**
	 * 엔티티가 그려지고 있는 ISKM 트랙에 재생 속도를 **직접** 건다. 하나라도 반영했으면 true.
	 *
	 * ⚠️ **프래그먼트의 PlayRate만 바꿔서는 아무 일도 일어나지 않는다.** 엔진의 반영 지점
	 * (`UMassVisualizationComponent::EndVisualChanges`)은 **`SequenceIndex`가 달라졌을 때만**
	 * `SetAutoPlayData`를 호출한다 — 재생 속도만 바뀐 갱신은 경고 한 줄 없이 버려진다.
	 * 그래서 감속·복귀는 이 경로로 트랙에 직접 걸어야 한다.
	 *
	 * `SetPlayRate`는 **현재 위상을 보존한 채** 기준 타임스탬프를 다시 계산하므로 포즈가 튀지 않는다.
	 *
	 * ⚠️ **몸통과 무기는 서로 다른 ISKM이고 트랙도 따로다** (6.3장). 한쪽만 늦추면 칼만 앞서 나가므로
	 * 그 엔티티가 들어 있는 공유 데이터를 **전부** 훑는다. 항목 수는 (표현 종류 x 메시 수)라 한 자릿수고,
	 * 이 경로는 매 프레임이 아니라 속도가 **바뀌는 순간**에만 돈다.
	 */
	bool SetTrackPlayRate(UMassRepresentationSubsystem& RepresentationSubsystem,
		const FMassEntityHandle Entity, const float PlayRate)
	{
		const UMassVisualizationComponent* VisualizationComponent = RepresentationSubsystem.GetVisualizationComponent();
		if (VisualizationComponent == nullptr)
			return false;

		bool bApplied = false;
		for (int32 DataIndex = 0; ; ++DataIndex)
		{
			// 배열 끝에서 null이 나온다. 중간에 비워진 항목은 트랙 맵이 비어 있어 그냥 지나간다.
			const FMassInstancedSkinnedMeshComponentSharedData* SharedData =
				VisualizationComponent->GetSharedDataForDescriptionIndex(DataIndex);
			if (SharedData == nullptr)
				break;

			const int32* TrackId = SharedData->GetEntityToTrackMap().Find(Entity);
			if (TrackId == nullptr)
				continue;

			const UInstancedSkinnedMeshComponent* MeshComponent = SharedData->GetInstancedSkinnedMeshComponent();
			if (MeshComponent == nullptr)
				continue;

			// GetTransformProvider()는 const 메서드지만 비const 포인터를 돌려준다 (엔진 API).
			if (UAnimSequenceTransformProviderDataInstance* ProviderInstance =
				Cast<UAnimSequenceTransformProviderDataInstance>(MeshComponent->GetTransformProvider()))
			{
				bApplied |= ProviderInstance->SetPlayRate(*TrackId, /*LayerIndex*/ 0, PlayRate);
			}
		}
		return bApplied;
	}
}

ULNPEnemyAnimationProcessor::ULNPEnemyAnimationProcessor()
	: AnimQuery(*this)
{
	// 서버는 그리지 않는다. 리슨 서버 호스트는 Client | Server 플래그를 받으므로 이 조합으로 함께 돈다
	// (엔진의 소비 프로세서도 정확히 같은 플래그다).
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	bAutoRegisterWithProcessingPhases = true;

	// ⚠️ 페이즈를 명시한다. 소비 프로세서가 ProcessingPhase를 설정하지 않아 기본값 PrePhysics로 돌기
	//    때문이며, 페이즈가 어긋나면 아래 ExecuteBefore/After가 조용히 무시된다.
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Representation;
	// CurrentRepresentation이 확정된 뒤여야 Actor가 그리는 개체를 건너뛸 수 있다.
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::VisualizationProcessing);
	ExecutionOrder.ExecuteBefore.Add(UMassConsumeInstancedSkinnedMeshAnimationProcessor::StaticClass()->GetFName());

	// 시퀀스 길이를 읽으려면 표현 서브시스템의 메시 정보를 만져야 한다 — 소비 프로세서와 같은 조건이다.
	bRequiresGameThreadExecution = true;
}

void ULNPEnemyAnimationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	AnimQuery.AddRequirement<FMassRepresentationAnimationFragment>(EMassFragmentAccess::ReadWrite);
	AnimQuery.AddRequirement<FLNPEnemyActionFragment>(EMassFragmentAccess::ReadOnly);
	AnimQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	AnimQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	AnimQuery.AddSharedRequirement<FMassRepresentationSubsystemSharedFragment>(EMassFragmentAccess::ReadWrite);
	AnimQuery.AddSubsystemRequirement<UMassRepresentationSubsystem>(EMassFragmentAccess::ReadWrite);
	AnimQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	// ⚠️ FLNPEnemyDyingTag를 None으로 걸지 않는다 — 죽는 순간 쿼리에서 빠지면 Death 시퀀스를 아무도 못 건다.
	AnimQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyAnimationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	AnimQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Ctx)
	{
		const ULNPEnemyConfig* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config;
		if (Config == nullptr || Config->ActionSequences.IsEmpty())
			return;

		UMassRepresentationSubsystem* RepresentationSubsystem = Ctx.GetSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
		check(RepresentationSubsystem);
		FMassInstancedSkinnedMeshInfoArrayView MeshInfos = RepresentationSubsystem->GetMutableInstancedSkinnedMeshInfos();

		const TArrayView<FMassRepresentationAnimationFragment> AnimFrags   = Ctx.GetMutableFragmentView<FMassRepresentationAnimationFragment>();
		const TConstArrayView<FLNPEnemyActionFragment> Actions             = Ctx.GetFragmentView<FLNPEnemyActionFragment>();
		const TConstArrayView<FMassRepresentationFragment> Representations = Ctx.GetFragmentView<FMassRepresentationFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			// Actor로 그리는 동안은 손대지 않는다 — 그쪽 포즈의 주인은 ABP다.
			if (UE::Mass::Representation::IsValidActorRepresentation(Representations[i].CurrentRepresentation))
				continue;

			const FLNPEnemyActionFragment& ActionFrag = Actions[i];
			const FLNPEnemyActionSequences* Entry = Config->ActionSequences.Find(ActionFrag.Action);
			if (Entry == nullptr || Entry->Indices.IsEmpty())
				continue;   // 매핑이 없는 행동은 직전 시퀀스를 그대로 유지한다.

			// 변형은 전이 카운터에서 고른다 — 별도 장부가 없어도 서버와 게스트가 같은 Seq를 보므로
			// 두 화면이 같은 변형을 고른다.
			//
			// ⚠️ **Seq를 그대로 나머지 연산하면 안 된다.** 공격은 반드시 다른 상태를 경유해 재진입하므로
			// (Move -> Attack -> Move -> Attack) 공격 사이의 전이 수가 대체로 짝수이고, 그러면 변형이
			// 한쪽 패리티에 **고정된다** — 2026-09-06 실측에서 "1번만 반복하다 간격이 홀수인 순간
			// 2번으로 넘어가 다시 고정"으로 나타났다. 곱셈 후 상위 비트를 내려 패리티 상관을 끊는다
			// (간격 1·2·3·4 전부에서 교대하는 것을 확인했다).
			const uint32 Mixed = (static_cast<uint32>(ActionFrag.Seq) * 2654435761u) >> 13;
			const int32 SequenceIndex = Entry->Indices[Mixed % static_cast<uint32>(Entry->Indices.Num())];

			// 공격만 재생 속도를 위상 상수에서 파생시킨다 — 애니가 판정을 정하는 게 아니라
			// 판정이 애니 속도를 정한다. 이 관계는 데이터로만 묶여 있어 어긋나도 컴파일도 실행도
			// 실패하지 않으므로, 상수를 손으로 맞추지 않고 여기서 매번 계산한다.
			// 다른 행동은 1.0이다 — 이동 속도에 맞춘 보정은 넣지 않았다(게스트에 속도가 없다).
			float PlayRate = 1.f;
			if (ActionFrag.Action == ELNPEnemyAction::Attack)
			{
				const FLNPEntityAttackConfig& AttackConfig = Config->EntityAttackConfig;
				const float PhaseTotal = AttackConfig.WindupTime + AttackConfig.ActiveTime + AttackConfig.RecoveryTime;
				const float Length = GetSequencePlayLength(MeshInfos, Representations[i], SequenceIndex);
				if (Length > 0.f && PhaseTotal > UE_SMALL_NUMBER)
					PlayRate = Length / PhaseTotal;
			}

			FAnimSequenceTrackAutoPlayData& AnimData = AnimFrags[i].AnimData;
			AnimData.SequenceIndex = SequenceIndex;
			AnimData.Position      = 0.f;
			AnimData.PlayRate      = PlayRate;
			AnimData.BlendTime     = Config->AnimBlendTime;
			// 루프 여부는 파생값이다 — "일회성 연출인가"의 단일 원본이 이미 있다.
			AnimData.LoopMode = FLNPEnemyActionFragment::IsOneShot(ActionFrag.Action)
				? EAnimSequenceTrackLoopMode::Clamp
				: EAnimSequenceTrackLoopMode::Loop;
		}
	});
}

// ============================================================
// ULNPEnemyHitStopProcessor
// ============================================================

ULNPEnemyHitStopProcessor::ULNPEnemyHitStopProcessor()
	: HitStopQuery(*this)
{
	// 그리는 머신에서만 돈다 — 애니 프로세서와 같은 조건이다.
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	bAutoRegisterWithProcessingPhases = true;

	// ⚠️ **PostPhysics다. PrePhysics가 아니다.** 이유가 둘이고 각각 한 프레임을 만든다:
	//  ① 판정(StartPhysics)이 미는 커맨드는 **페이즈가 끝날 때** 플러시된다
	//     (FMassProcessingPhaseManager::OnPhaseEnd). PrePhysics에서 읽으면 이번 프레임의 적중을 못 보고
	//     다음 프레임에야 본다 — 스윙이 0.27초에 300도라 한 프레임이 약 19도다. 그만큼 칼이 지나간
	//     뒤에 멈칫하는 것으로 보인다.
	//  ② 엔진이 AnimData를 실제로 트랙에 반영하는 지점
	//     (UMassVisualizationComponent::EndVisualChanges)은 **PostPhysics 시작**이다
	//     (UMassRepresentationSubsystem::OnProcessingPhaseStarted). 그보다 먼저 걸면 시퀀스가 바뀐
	//     프레임에 우리가 건 값이 그대로 덮인다.
	ProcessingPhase = EMassProcessingPhase::PostPhysics;

	// 트랙 조회가 표현 컴포넌트를 만진다 — 애니 프로세서와 같은 조건이다.
	bRequiresGameThreadExecution = true;
}

void ULNPEnemyHitStopProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// 기본 재생 속도의 **단일 원본**이다 — 애니 프로세서가 이번 프레임에 써 둔 값을 그대로 읽는다.
	// 감속 전 값을 따로 들지 않으므로 두 프로세서가 어긋날 여지가 없다.
	HitStopQuery.AddRequirement<FMassRepresentationAnimationFragment>(EMassFragmentAccess::ReadOnly);
	HitStopQuery.AddRequirement<FLNPEnemyActionFragment>(EMassFragmentAccess::ReadWrite);
	HitStopQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	HitStopQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>();
	HitStopQuery.AddSharedRequirement<FMassRepresentationSubsystemSharedFragment>(EMassFragmentAccess::ReadWrite);
	HitStopQuery.AddSubsystemRequirement<UMassRepresentationSubsystem>(EMassFragmentAccess::ReadWrite);
	HitStopQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	HitStopQuery.RegisterWithProcessor(*this);
}

void ULNPEnemyHitStopProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	HitStopQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Ctx)
	{
		const ULNPEnemyConfig* Config = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>().Config;
		if (Config == nullptr)
			return;

		const FLNPEntityAttackConfig& AttackConfig = Config->EntityAttackConfig;

		UMassRepresentationSubsystem* RepresentationSubsystem = Ctx.GetSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
		check(RepresentationSubsystem);

		const TConstArrayView<FMassRepresentationAnimationFragment> AnimFrags = Ctx.GetFragmentView<FMassRepresentationAnimationFragment>();
		const TArrayView<FLNPEnemyActionFragment> Actions                     = Ctx.GetMutableFragmentView<FLNPEnemyActionFragment>();
		const TConstArrayView<FMassRepresentationFragment> Representations    = Ctx.GetFragmentView<FMassRepresentationFragment>();

		const float DeltaTime = Ctx.GetDeltaTimeSeconds();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPEnemyActionFragment& ActionFrag = Actions[i];

			// Actor로 그리는 동안은 손대지 않는다 — 그쪽은 몽타주와 CustomTimeDilation의 영역이다.
			if (UE::Mass::Representation::IsValidActorRepresentation(Representations[i].CurrentRepresentation))
			{
				// 장부를 무효로 되돌린다. 승격 중에 밀린 적중을 ISKM으로 내려올 때 뒤늦게 재생하면
				// 이미 끝난 공격의 감속이 엉뚱한 동작에 걸린다.
				ActionFrag.AppliedSequenceIndex = INDEX_NONE;
				ActionFrag.HitStopTimeRemaining = 0.f;
				ActionFrag.ConsumedHitStopSeq   = ActionFrag.HitStopSeq;
				continue;
			}

			// 서버가 올린 카운터 하나만 보고 각 머신이 스스로 시간을 잰다. 호스트는 자기 프래그먼트를,
			// 게스트는 복제된 같은 값을 읽으므로 **넷 모드 분기가 없다**(5.3장의 단일 소비 경로).
			if (ActionFrag.HitStopSeq != ActionFrag.ConsumedHitStopSeq)
			{
				ActionFrag.ConsumedHitStopSeq   = ActionFrag.HitStopSeq;
				ActionFrag.HitStopTimeRemaining = AttackConfig.HitStopDuration;
			}
			else if (ActionFrag.HitStopTimeRemaining > 0.f)
			{
				ActionFrag.HitStopTimeRemaining = FMath::Max(0.f, ActionFrag.HitStopTimeRemaining - DeltaTime);
			}

			const FAnimSequenceTrackAutoPlayData& AnimData = AnimFrags[i].AnimData;
			const float DesiredPlayRate = ActionFrag.HitStopTimeRemaining > 0.f
				? AnimData.PlayRate * AttackConfig.HitStopPlayRateScale
				: AnimData.PlayRate;

			// 시퀀스가 바뀐 프레임이면 엔진이 **이미** 이 페이즈 시작에서 트랙을 기본 속도로 다시
			// 앵커링했다. 장부만 그 값으로 맞춰 두면 아래 비교가 곧바로 성립해 같은 프레임에 누른다.
			if (ActionFrag.AppliedSequenceIndex != AnimData.SequenceIndex)
			{
				ActionFrag.AppliedSequenceIndex = AnimData.SequenceIndex;
				ActionFrag.AppliedPlayRate      = AnimData.PlayRate;
			}

			// 반영에 실패했으면(트랙이 아직 없는 등) 장부를 그대로 둬 다음 프레임에 다시 시도한다.
			if (!FMath::IsNearlyEqual(DesiredPlayRate, ActionFrag.AppliedPlayRate)
				&& SetTrackPlayRate(*RepresentationSubsystem, Ctx.GetEntity(i), DesiredPlayRate))
			{
				ActionFrag.AppliedPlayRate = DesiredPlayRate;
			}
		}
	});
}
