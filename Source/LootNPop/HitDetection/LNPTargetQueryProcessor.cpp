// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPTargetQueryProcessor.h"
#include "HitDetection/LNPTargetQuerySubsystem.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"

#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"

ULNPTargetQueryProcessor::ULNPTargetQueryProcessor()
	: EnemyQuery(*this)
{
	// 서버·클라이언트 모두에서 돈다. 질의를 등록하는 쪽은 소유 클라이언트뿐이라(결과는 발동 요청에 실어 보낸다)
	// 클라이언트가 주 소비처이고, 서버도 장차 자동 탐색 폴백(근접 보정)에서 같은 창구를 쓸 수 있다.
	// 슬롯이 없는 머신에서는 SnapshotQueries가 비어 즉시 반환한다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;

	// 같은 프레임의 ProduceInput이 결과를 읽을 수 있도록 표현 체인과 같은 페이즈에 둔다.
	// ⚠️ 순서 선언은 페이즈를 건너지 못한다 — 다른 페이즈의 프로세서를 지목하면 조용히 무시된다.
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void ULNPTargetQueryProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyActionFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<ULNPTargetQuerySubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPTargetQueryProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ULNPTargetQuerySubsystem& QuerySub = Context.GetMutableSubsystemChecked<ULNPTargetQuerySubsystem>();

	TArray<int32>              SlotIndices;
	TArray<FLNPTargetQueryParams> Params;
	QuerySub.SnapshotQueries(SlotIndices, Params);
	if (Params.IsEmpty())
		return;

	TArray<FLNPTargetQueryResult> Results;
	Results.SetNum(Params.Num());

	// 점수는 "클수록 이긴다"로 통일한다 — 광선은 깊이의 부호를 뒤집어 최근접이 최고점이 되게 한다.
	TArray<float> BestScores;
	BestScores.Init(-MAX_FLT, Params.Num());

	EnemyQuery.ForEachEntityChunk(Context, [&Params, &Results, &BestScores](FMassExecutionContext& Ctx)
	{
		const FLNPEnemySharedFragment& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
		if (nullptr == Shared.Config)
			return;

		const float HalfHeight = Shared.Config->CapsuleHalfHeight;
		const float Radius     = Shared.Config->CapsuleRadius;

		const TConstArrayView<FTransformFragment>        Transforms  = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPEnemyFragment>        EnemyFrags  = Ctx.GetFragmentView<FLNPEnemyFragment>();
		const TConstArrayView<FLNPEnemyActionFragment>  ActionFrags = Ctx.GetFragmentView<FLNPEnemyActionFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			// 시체는 대상이 아니다. 소멸까지 기다리면 락온이 시체에 붙어 있게 된다.
			//
			// 둘 다 보는 이유는 **Dying 전이가 한 틱 늦을 수 있어서다** — 서버에서는 HP가 먼저 0이 된다.
			// (HP는 2026-09-09부터 비율이 복제되므로 클라이언트에서도 유효하다 — TechDesign_HUD.md §11.3.
			//  그전에는 클라에서 기본값에 머물러 행동 상태만이 유일한 신호였다.)
			if (EnemyFrags[i].Health <= 0.f || ELNPEnemyAction::Dying == ActionFrags[i].Action)
				continue;

			const FVector Location = Transforms[i].GetTransform().GetLocation();

			// 구 내벽이라 Up은 월드 중심의 반대 방향이다 (원거리 판정과 같은 규약).
			const FVector EnemyUpDir = (-Location).GetSafeNormal();
			const FVector Center     = LNPHitDetection::ResolveEnemyCapsuleCenter(Location, EnemyUpDir, HalfHeight, nullptr);

			for (int32 q = 0; q < Params.Num(); ++q)
			{
				const FLNPTargetQueryParams& P = Params[q];

				float   Score = 0.f;
				float   Distance = 0.f;
				FVector ResultLocation = Location;

				if (ELNPTargetQueryKind::Ray == P.Kind)
				{
					FVector HitPoint;
					if (!LNPHitDetection::SegmentHitsCapsule(P.Origin, P.Origin + P.Direction * P.MaxDistance,
						Center, EnemyUpDir, HalfHeight, Radius, HitPoint))
					{
						continue;
					}

					Distance       = FVector::DotProduct(HitPoint - P.Origin, P.Direction);
					ResultLocation = HitPoint;
					Score          = -Distance;   // 가까울수록 이긴다
				}
				else if (ELNPTargetQueryKind::Track == P.Kind)
				{
					if (Ctx.GetEntity(i) != P.TrackedEntity)
						continue;

					// 사망·소멸은 위쪽 HP 필터와 청크 순회가 이미 걸러낸다 — 여기 도달했다면 살아 있다.
					Distance = FVector::Dist(Center, P.Origin);
					if (Distance > P.MaxDistance)
						continue;

					ResultLocation = Center;
					Score          = 0.f;   // 후보가 하나뿐이라 경쟁이 없다
				}
				else
				{
					// 거리·각도는 질의자의 접평면에서 잰다 — 반지름 방향 성분이 섞이면
					// 높이가 다른 대상의 거리가 실제보다 멀게 나온다.
					FVector ToDir;
					if (!LNPTargetQuery::ProjectToTangent(P.UpDir, Center - P.Origin, ToDir, Distance)
						|| Distance > P.MaxDistance)
					{
						continue;
					}

					const float CosAngle = FMath::Clamp(FVector::DotProduct(P.Direction, ToDir), -1.f, 1.f);
					const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(CosAngle));
					if (AngleDeg > P.MaxAngleDeg)
						continue;

					Score = P.AngleWeight    * (1.f - AngleDeg / P.MaxAngleDeg)
					      + P.DistanceWeight * (1.f - Distance / P.MaxDistance);
				}

				if (Results[q].bHit && BestScores[q] >= Score)
					continue;

				BestScores[q]       = Score;
				Results[q].Entity   = Ctx.GetEntity(i);
				Results[q].Location = ResultLocation;
				Results[q].Distance = Distance;
				Results[q].bHit     = true;
			}
		}
	});

	QuerySub.SubmitResults(SlotIndices, Results);
}
