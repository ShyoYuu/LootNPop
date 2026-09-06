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
	// 서버·클라이언트 모두에서 돈다. 조준점은 소유 클라이언트가 만들어 InputCmd로 올리므로 클라이언트가
	// 주 소비처이고, 서버도 장차 자동 탐색 폴백(근접 보정)에서 같은 창구를 쓸 수 있다.
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

		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPEnemyFragment>  EnemyFrags = Ctx.GetFragmentView<FLNPEnemyFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			// 사망 판정을 엔티티 HP로 한다. Actor의 ASC를 보던 옛 경로는 순수 엔티티에서 성립하지 않았다.
			if (EnemyFrags[i].Health <= 0.f)
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
