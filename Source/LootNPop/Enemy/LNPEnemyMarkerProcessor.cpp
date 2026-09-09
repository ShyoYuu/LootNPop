// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyMarkerProcessor.h"
#include "Enemy/LNPEnemyMarkerSubsystem.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "HitDetection/LNPHitDetectionShared.h"

#include "Engine/World.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"

ULNPEnemyMarkerProcessor::ULNPEnemyMarkerProcessor()
	: EnemyQuery(*this)
{
	// 리슨 호스트도 HP 바를 봐야 하므로 클라이언트 전용이 아니다.
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;

	// 같은 프레임의 HUD Tick이 결과를 읽는다 — 표현 체인과 같은 페이즈에 둔다.
	// ⚠️ 순서 선언은 페이즈를 건너지 못하므로, 순서 제약이 필요해지면 반드시 같은 페이즈 안에서 건다.
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void ULNPEnemyMarkerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyActionFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyHealthDisplayFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<ULNPEnemyMarkerSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPEnemyMarkerProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ULNPEnemyMarkerSubsystem& MarkerSub = Context.GetMutableSubsystemChecked<ULNPEnemyMarkerSubsystem>();

	FLNPEnemyMarkerParams Params;
	if (!MarkerSub.SnapshotParams(Params))
		return;

	const UWorld* World = EntityManager.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const float CosMaxAngle = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Params.MaxAngleDeg, 0.f, 180.f)));

	/** 정렬 전 후보. 최근 피해 여부가 거리보다 먼저 걸린다. */
	struct FCandidate
	{
		FLNPEnemyMarkerEntry Entry;
		bool bRecentlyDamaged = false;
	};
	TArray<FCandidate> Candidates;

	EnemyQuery.ForEachEntityChunk(Context, [&Params, &Candidates, Now, CosMaxAngle](FMassExecutionContext& Ctx)
	{
		const FLNPEnemySharedFragment& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
		if (nullptr == Shared.Config)
			return;

		const float HalfHeight = Shared.Config->CapsuleHalfHeight;

		const TConstArrayView<FTransformFragment>       Transforms  = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPEnemyFragment>        EnemyFrags  = Ctx.GetFragmentView<FLNPEnemyFragment>();
		const TConstArrayView<FLNPEnemyActionFragment>  ActionFrags = Ctx.GetFragmentView<FLNPEnemyActionFragment>();
		const TArrayView<FLNPEnemyHealthDisplayFragment> DisplayFrags = Ctx.GetMutableFragmentView<FLNPEnemyHealthDisplayFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			// ① 관측 갱신은 **게이트보다 먼저**, 표시 여부와 무관하게 한다.
			//    화면 밖에서 맞은 적이 돌아섰을 때 "최근 피해"로 잡혀야 하기 때문이다.
			const uint8 Pct = FLNPEnemyHealthDisplayFragment::EncodePct(EnemyFrags[i].Health, EnemyFrags[i].MaxHealth);
			FLNPEnemyHealthDisplayFragment& Display = DisplayFrags[i];
			if (Pct != Display.LastSeenPct)
			{
				Display.LastSeenPct   = Pct;
				Display.LastChangeTime = Now;
			}
			const bool bRecentlyDamaged = (Now - Display.LastChangeTime) <= Params.RecentDamageWindow;

			// ② 시체는 표시하지 않는다. 서버에서는 HP가 먼저 0이 되고 Dying 전이가 한 틱 늦을 수 있어 둘 다 본다.
			if (EnemyFrags[i].Health <= 0.f || ELNPEnemyAction::Dying == ActionFrags[i].Action)
				continue;

			// 만피는 그리지 않는다 — 방금 회복해 만피가 된 경우만 잠깐 남는다.
			if (Pct >= MAX_uint8 && !bRecentlyDamaged)
				continue;

			// 구 내벽이라 Up은 월드 중심의 반대 방향이다 (판정·락온과 같은 규약).
			const FVector Location   = Transforms[i].GetTransform().GetLocation();
			const FVector EnemyUpDir = (-Location).GetSafeNormal();
			const FVector Center     = LNPHitDetection::ResolveEnemyCapsuleCenter(Location, EnemyUpDir, HalfHeight, nullptr);

			// ⚠️ 여기의 거리는 **카메라 기준 직선 거리**다. 접평면 규약은 지면 위 거리를 잴 때의 것이라
			//    해당하지 않는다 — 화면에서 얼마나 작게 보이는지가 이 값으로 정해진다.
			const FVector ToTarget = Center - Params.Origin;
			const float   Distance = ToTarget.Size();
			if (Distance > Params.MaxDistance || Distance <= KINDA_SMALL_NUMBER)
				continue;

			if (FVector::DotProduct(Params.Direction, ToTarget / Distance) < CosMaxAngle)
				continue;

			FCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.Entry.Entity   = Ctx.GetEntity(i);
			Candidate.Entry.Location = Center;
			Candidate.Entry.Ratio    = static_cast<float>(Pct) / static_cast<float>(MAX_uint8);
			Candidate.Entry.Distance = Distance;
			Candidate.bRecentlyDamaged = bRecentlyDamaged;
		}
	});

	// 최근 피해가 상한보다 우선한다 — 거리순 단독이면 라이플로 저격한 먼 적이 근처 잡몹에 밀려 안 보인다.
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.bRecentlyDamaged != B.bRecentlyDamaged)
			return A.bRecentlyDamaged;

		return A.Entry.Distance < B.Entry.Distance;
	});

	const int32 Num = FMath::Min(Candidates.Num(), Params.MaxCount);
	TArray<FLNPEnemyMarkerEntry> Entries;
	Entries.Reserve(Num);
	for (int32 i = 0; i < Num; ++i)
		Entries.Add(Candidates[i].Entry);

	MarkerSub.SubmitEntries(MoveTemp(Entries));
}
