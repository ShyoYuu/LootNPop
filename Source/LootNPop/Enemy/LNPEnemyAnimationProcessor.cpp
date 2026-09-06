// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyAnimationProcessor.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"

#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationTypes.h"
#include "MassRepresentationAnimationProcessor.h"
#include "MassRepresentationSubsystem.h"                    // FMassInstancedSkinnedMeshInfoArrayView
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
