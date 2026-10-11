// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPFlameConeProcessors.h"
#include "HitDetection/LNPFlameConeMassTypes.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "HitDetection/LNPProjectileMotion.h"
#include "HitDetection/LNPPositionHistoryFragment.h"
#include "GAS/LNPPoiseTypes.h"
#include "GAS/LNPDamageFormula.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "LootNPop.h"
#include "LNPMassUtils.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "MassCommandBuffer.h"
#include "MassCommands.h"

ULNPFlameConeHitDetectionProcessor::ULNPFlameConeHitDetectionProcessor()
	: ConeQuery(*this), EnemyQuery(*this)
{
	// 원뿔 엔티티는 서버에서만 생긴다 — 클라이언트에는 판정할 것도 파괴할 것도 없다.
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::StartPhysics;
}

void ULNPFlameConeHitDetectionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ConeQuery.AddRequirement<FLNPFlameConeFragment>(EMassFragmentAccess::ReadOnly);
	ConeQuery.RegisterWithProcessor(*this);

	// 근접 판정(ULNPWeaponTraceHitDetectionProcessor)의 적 쿼리와 같은 구성 — 넉백 속도만 쓰지 않는다.
	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FLNPPositionHistoryFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPPoiseFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	EnemyQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<ULNPMassWorldCollisionSubsystem>(EMassFragmentAccess::ReadOnly);
}

void ULNPFlameConeHitDetectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	struct FCone
	{
		FLNPFlameConeFragment Data;
		FMassEntityHandle     Handle;
	};
	TArray<FCone> Cones;

	ConeQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FLNPFlameConeFragment> Frags = Ctx.GetFragmentView<FLNPFlameConeFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			Cones.Add({ Frags[i], Ctx.GetEntity(i) });
	});

	if (Cones.IsEmpty())
		return;

	// 판정 결과와 무관하게 이번 실행에서 모두 파괴한다 — 원뿔 엔티티의 유일한 파괴 지점.
	{
		TArray<FMassEntityHandle> ToDestroy;
		ToDestroy.Reserve(Cones.Num());
		for (const FCone& Cone : Cones)
			ToDestroy.Add(Cone.Handle);
		Context.Defer().PushCommand<FMassCommandDestroyEntities>(TConstArrayView<FMassEntityHandle>(ToDestroy));
	}

	UWorld* World = EntityManager.GetWorld();
	if (!World)
		return;

	const ULNPMassWorldCollisionSubsystem* WorldCollision = Context.GetSubsystem<ULNPMassWorldCollisionSubsystem>();
	const auto* RewindSurfaces = World->GetSubsystem<ULNPSurfaceDataSubsystem>();
	const auto RewindSurface = RewindSurfaces ? RewindSurfaces->TakeSnapshot() : nullptr;
	const double Now = World->GetTimeSeconds();

	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPEnemySharedFragment& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
		const ULNPEnemyConfig* Config = Shared.Config;
		if (!Config)
			return;

		const float HalfH     = Config->GetHitCapsuleHalfHeight();
		const float CapsuleR  = Config->GetHitCapsuleRadius();
		const float HitOffset = Config->HitCenterUpOffset;
		const float CylHalfLen = FMath::Max(0.f, HalfH - CapsuleR);

		const TConstArrayView<FTransformFragment>          Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FLNPEnemyFragment>                       EnemyFrags = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		TArrayView<FMassActorFragment>                      ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPPositionHistoryFragment>  Histories  = Ctx.GetFragmentView<FLNPPositionHistoryFragment>();
		TArrayView<FLNPPoiseFragment>                       PoiseFrags = Ctx.GetMutableFragmentView<FLNPPoiseFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector RawLoc = Transforms[i].GetTransform().GetLocation();
			const FVector UpDir  = LNPMass::GetEntityUp(Transforms[i].GetTransform());
			AActor*       Actor  = ActorFrags[i].GetMutable();
			const FVector Center = LNPHitDetection::ResolveEnemyCapsuleCenter(RawLoc, UpDir, HitOffset, Actor);
			const FLNPPositionHistoryFragment& History = Histories[i];
			FLNPEnemyFragment& Enemy = EnemyFrags[i];

			for (const FCone& Cone : Cones)
			{
				const FLNPFlameConeFragment& C = Cone.Data;
				if (Ctx.GetEntity(i) == C.InstigatorEntity)
					continue;

				// 공격자 RTT/2만큼 과거의 대상 위치로 판정한다 — 근접과 같은 규약(보간 지연 가산 없음, TechDesign_HitDetection.md §5).
				// History가 비어 있으면 GetInterpolatedLocation이 행성 중심을 돌려주므로 되감지 않는다.
				const float RewindSeconds = FMath::Clamp(C.CachedRewindSeconds, 0.f, LNPHitDetection::MaxPingRewindSeconds);
				const FVector JudgedCenter = (RewindSeconds <= 0.f || History.Count == 0)
					? Center
					: Center + (History.GetInterpolatedLocation(Now - RewindSeconds, RewindSurface.Get()) - RawLoc);

				// 1차: 원뿔 전체를 감싸는 구 밖이면 버린다.
				const float BoundR = FMath::Max(C.NearRadius, C.FarRadius) + C.Range + HalfH;
				if (FVector::DistSquared(JudgedCenter, C.Origin) > FMath::Square(BoundR))
					continue;

				// 캡슐 축 선분에서 원뿔 축에 가장 가까운 점을 대표점으로 쓴다 — 키 큰 적의 머리·발만 들어와도 맞는다.
				FVector OnConeAxis, OnCapsuleAxis;
				FMath::SegmentDistToSegment(C.Origin, C.Origin + C.Axis * C.Range,
					JudgedCenter - UpDir * CylHalfLen, JudgedCenter + UpDir * CylHalfLen, OnConeAxis, OnCapsuleAxis);

				const FVector ToPoint = OnCapsuleAxis - C.Origin;
				const float   T       = FVector::DotProduct(ToPoint, C.Axis);
				if (T < -CapsuleR || T > C.Range + CapsuleR)
					continue;

				const float Alpha   = C.ConeRange > 0.f ? FMath::Clamp(T / C.ConeRange, 0.f, 1.f) : 0.f;
				const float Allowed = FMath::Lerp(C.NearRadius, C.FarRadius, Alpha) + CapsuleR;
				if ((ToPoint - C.Axis * T).SizeSquared() > FMath::Square(Allowed))
					continue;

				// 축 트레이스는 정면 벽만 자른다. 축 옆 기둥 뒤에 숨은 대상은 총구→대표점 사선으로 거른다.
				FLNPWorldHit WallHit;
				if (WorldCollision && LNPProjectileMotion::TraceWorld(*WorldCollision, C.Origin, OnCapsuleAxis, WallHit))
					continue;

				const FVector HitFromDir = (C.Origin - Center).GetSafeNormal(); // 피격자 → 공격자

				LNPPoise::Accumulate(PoiseFrags.IsValidIndex(i) ? &PoiseFrags[i] : nullptr, C.PoiseDamage, Now);
				Enemy.bActivityWakeRequested = true;
				// 배회 중이면 불이 날아온 쪽으로 돌아선다. 플린치는 걸지 않는다 — 0.1초마다 갱신되면 움찔이 끊기지 않는다.
				Enemy.HitReactTimer     = Config->TargetingConfig.HitReactLookTime;
				Enemy.HitReactDirection = HitFromDir;

				const TSubclassOf<UGameplayEffect> EffectClass(C.DamageEffectClass);
				if (Actor && EffectClass)
				{
					// 틱 피해라 HitReact 몽타주를 억제한다 — 근접·원거리와 달리 0.1초마다 들어온다.
					Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Actor, EffectClass, C.Damage, HitFromDir,
						OnCapsuleAxis, /*KnockbackStrength*/ 0.f, /*bSuppressHitReact*/ true);
				}
				else
				{
					const float HpBefore = Enemy.Health;
					Enemy.Health = FMath::Max(0.f, HpBefore - LNPDamage::ApplyDefense(C.Damage, Enemy.Defense));
					UE_LOG(LogLootNPop, Log, TEXT("[FlameCone][Entity] HP: %.1f -> %.1f (damage=%.1f t=%.0f)"),
						HpBefore, Enemy.Health, C.Damage, T);
				}
			}
		}
	});
}
