// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPWeaponTraceProcessors.h"
#include "HitDetection/LNPWeaponTraceMassTypes.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "HitDetection/LNPGuardParryTypes.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Character/LNPPlayerCharacter.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GAS/Effects/LNPGameplayEffect_Damage.h"
#include "GAS/LNPDamageFormula.h"
#include "Config/LNPSettings.h"
#include "LootNPop.h"

#include "Components/CapsuleComponent.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassCommandBuffer.h"
#include "AbilitySystemComponent.h"
#if WITH_EDITOR
#include "MassDebugDrawHelpers.h"
#endif

namespace
{
	/** Möller–Trumbore 알고리즘: 선분이 삼각형과 교차하면 true. */
	bool SegmentIntersectsTriangle(
		const FVector& SA, const FVector& SB,
		const FVector& V0, const FVector& V1, const FVector& V2)
	{
		const FVector Dir   = SB - SA;
		const FVector Edge1 = V1 - V0;
		const FVector Edge2 = V2 - V0;
		const FVector H     = FVector::CrossProduct(Dir, Edge2);
		const float   A     = FVector::DotProduct(Edge1, H);

		if (FMath::Abs(A) < SMALL_NUMBER)
			return false;

		const float   F = 1.f / A;
		const FVector S = SA - V0;
		const float   U = F * FVector::DotProduct(S, H);
		if (U < 0.f || U > 1.f)
			return false;

		const FVector Q = FVector::CrossProduct(S, Edge1);
		const float   V = F * FVector::DotProduct(Dir, Q);
		if (V < 0.f || U + V > 1.f)
			return false;

		const float T = F * FVector::DotProduct(Edge2, Q);
		return T >= 0.f && T <= 1.f;
	}

	/**
	 * 삼각형 면과 선분 사이의 최단 거리² 반환.
	 * 선분이 삼각형을 관통하면 0을 반환한다.
	 */
	float TriangleSegmentDistSq(
		const FVector& TA, const FVector& TB, const FVector& TC,
		const FVector& SA, const FVector& SB)
	{
		if (SegmentIntersectsTriangle(SA, SB, TA, TB, TC))
			return 0.f;

		float MinSq = MAX_FLT;
		FVector P1, P2;

		// 선분 끝점 → 삼각형
		FVector C = FMath::ClosestPointOnTriangleToPoint(SA, TA, TB, TC);
		MinSq = FMath::Min(MinSq, FVector::DistSquared(SA, C));

		C = FMath::ClosestPointOnTriangleToPoint(SB, TA, TB, TC);
		MinSq = FMath::Min(MinSq, FVector::DistSquared(SB, C));

		// 삼각형 각 변 → 선분
		FMath::SegmentDistToSegment(TA, TB, SA, SB, P1, P2);
		MinSq = FMath::Min(MinSq, FVector::DistSquared(P1, P2));

		FMath::SegmentDistToSegment(TB, TC, SA, SB, P1, P2);
		MinSq = FMath::Min(MinSq, FVector::DistSquared(P1, P2));

		FMath::SegmentDistToSegment(TC, TA, SA, SB, P1, P2);
		MinSq = FMath::Min(MinSq, FVector::DistSquared(P1, P2));

		return MinSq;
	}

	/**
	 * Swept Quad(삼각형 2개)와 캡슐 축 선분 사이의 최단 거리² 반환.
	 * T1 = {RootPrev, RootCurr, TipCurr}, T2 = {RootPrev, TipCurr, TipPrev}
	 */
	float SweptQuadCapsuleDistSq(
		const FVector& RootPrev, const FVector& TipPrev,
		const FVector& RootCurr, const FVector& TipCurr,
		const FVector& CapsuleBot, const FVector& CapsuleTop)
	{
		return FMath::Min(
			TriangleSegmentDistSq(RootPrev, RootCurr, TipCurr, CapsuleBot, CapsuleTop),
			TriangleSegmentDistSq(RootPrev, TipCurr,  TipPrev, CapsuleBot, CapsuleTop));
	}

	/**
	 * 캡슐 축 선분(Bot~Top)을 계산한다.
	 * CylHalfLen = HalfHeight - Radius (실린더 반절 길이; 0 미만 클램프).
	 */
	void MakeCapsuleSeg(const FVector& Center, const FVector& Up, float HalfH, float R,
		FVector& OutBot, FVector& OutTop)
	{
		const float CylHalfLen = FMath::Max(0.f, HalfH - R);
		OutBot = Center - Up * CylHalfLen;
		OutTop = Center + Up * CylHalfLen;
	}

	bool IsAlreadyHit(const FLNPWeaponTraceFragment& Frag, FMassEntityHandle Target)
	{
		for (int32 i = 0; i < Frag.AlreadyHitCount; ++i)
			if (Frag.AlreadyHit[i] == Target)
				return true;
		return false;
	}

	void MarkHit(FLNPWeaponTraceFragment& Frag, FMassEntityHandle Target)
	{
		if (Frag.AlreadyHitCount < FLNPWeaponTraceFragment::MaxAlreadyHit)
			Frag.AlreadyHit[Frag.AlreadyHitCount++] = Target;
	}
}

// ============================================================
// ULNPWeaponTraceHitDetectionProcessor
// ============================================================

ULNPWeaponTraceHitDetectionProcessor::ULNPWeaponTraceHitDetectionProcessor()
	: AttackerQuery(*this), EnemyQuery(*this), PlayerQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::StartPhysics;
}

void ULNPWeaponTraceHitDetectionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	AttackerQuery.AddRequirement<FLNPWeaponTraceFragment>(EMassFragmentAccess::ReadWrite);
	AttackerQuery.RegisterWithProcessor(*this);

	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	EnemyQuery.RegisterWithProcessor(*this);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	PlayerQuery.AddRequirement<FLNPParryStateFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerQuery.RegisterWithProcessor(*this);

}

void ULNPWeaponTraceHitDetectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// ── Pass 1: Enemy 캡슐 데이터 수집 ────────────────────────────────────────
	struct FCollectedEnemy
	{
		FVector            CapsuleCenter;    // 액터 위치 + UpDir * HalfHeight (캡슐 중심)
		FVector            UpDir;            // (-Location).GetSafeNormal() — 구형 세계 UP
		float              CapsuleHalfHeight;
		float              CapsuleRadius;
		FLNPEnemyFragment* Fragment;
		FMassEntityHandle  Handle;
		AActor*            Actor;
	};
	TArray<FCollectedEnemy> Enemies;

	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPEnemySharedFragment& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
		if (!Shared.Config)
			return;

		const float HalfH  = Shared.Config->CapsuleHalfHeight;
		const float Radius = Shared.Config->CapsuleRadius;

		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FLNPEnemyFragment>             EnemyFrags = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		TArrayView<FMassActorFragment>            ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector Loc   = Transforms[i].GetTransform().GetLocation();
			const FVector UpDir = (-Loc).GetSafeNormal();
			AActor*       Actor = ActorFrags[i].GetMutable();
			FVector Center;
			if (const ALNPCharacterBase* Enemy = Cast<ALNPCharacterBase>(Actor))
				Center = Loc;
			else
				Center = Loc + UpDir * HalfH;
			Enemies.Add({ Center, UpDir, HalfH, Radius, &EnemyFrags[i], Ctx.GetEntity(i), Actor });
		}
	});

	// ── Pass 2: Player 캡슐 데이터 수집 ───────────────────────────────────────
	struct FCollectedPlayer
	{
		FVector                CapsuleCenter;  // 액터 위치 + UpDir * HalfHeight (캡슐 중심)
		FVector                UpDir;          // (-Location).GetSafeNormal()
		FVector                ForwardVector;  // 패링 각도 계산용
		float                  CapsuleHalfHeight;
		float                  CapsuleRadius;
		FMassEntityHandle      Handle;
		AActor*                Actor;
		FLNPParryStateFragment ParryState;
	};
	TArray<FCollectedPlayer> Players;

	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment>     Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FMassActorFragment>                ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPParryStateFragment> ParryFrags = Ctx.GetFragmentView<FLNPParryStateFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			AActor*                  Actor  = ActorFrags[i].GetMutable();
			const UCapsuleComponent* Cap    = Actor ? Actor->FindComponentByClass<UCapsuleComponent>() : nullptr;
			const float              HalfH  = Cap ? Cap->GetScaledCapsuleHalfHeight() : 96.f;
			const float              Radius = Cap ? Cap->GetScaledCapsuleRadius()     : 42.f;
			const FTransform&        T      = Transforms[i].GetTransform();
			const FVector            Loc    = T.GetLocation();
			const FVector            Up     = (-Loc).GetSafeNormal();
			Players.Add({ Loc, Up, T.GetRotation().GetForwardVector(), HalfH, Radius, Ctx.GetEntity(i), Actor, ParryFrags[i] });
		}
	});

	if (Enemies.IsEmpty() && Players.IsEmpty())
		return;

	const bool bFriendlyFire = GetDefault<ULNPSettings>()->bFriendlyFire;

	// ── Pass 3: Swept Volume 피격 판정 ────────────────────────────────────────
	AttackerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		TArrayView<FLNPWeaponTraceFragment> Attackers = Ctx.GetMutableFragmentView<FLNPWeaponTraceFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPWeaponTraceFragment& Frag = Attackers[i];

			// Prev==Curr(첫 프레임): 칼날 이동 없음 → 선분 vs 선분으로 폴백
			// 그 외: Swept Quad(삼각형 2개) vs 캡슐 축 선분 최단 거리 계산
			const bool  bDegenerate = FVector::DistSquared(Frag.SwordTipPrev, Frag.SwordTipCurr) < 1.f;
			const float SwordRadius = Frag.HitRadius;

			// 캡슐 축 선분(Bot~Top)과 칼날의 최단 거리²를 반환하는 람다
			auto CalcDistSq = [&](const FVector& CapsuleCenter, const FVector& UpDir, float HalfH, float CapsuleR) -> float
			{
				FVector CapsuleBot, CapsuleTop;
				MakeCapsuleSeg(CapsuleCenter, UpDir, HalfH, CapsuleR, CapsuleBot, CapsuleTop);

				if (bDegenerate)
				{
					FVector P1, P2;
					FMath::SegmentDistToSegment(Frag.SwordRootCurr, Frag.SwordTipCurr, CapsuleBot, CapsuleTop, P1, P2);
					return FVector::DistSquared(P1, P2);
				}
				return SweptQuadCapsuleDistSq(
					Frag.SwordRootPrev, Frag.SwordTipPrev,
					Frag.SwordRootCurr, Frag.SwordTipCurr,
					CapsuleBot, CapsuleTop);
			};

			if (Frag.InstigatorTeam == ELNPInstigatorTeam::Player)
			{
				// Player 공격 → Enemy 타겟
				for (FCollectedEnemy& Enemy : Enemies)
				{
					if (Enemy.Handle == Frag.InstigatorEntity || IsAlreadyHit(Frag, Enemy.Handle))
						continue;

					if (CalcDistSq(Enemy.CapsuleCenter, Enemy.UpDir, Enemy.CapsuleHalfHeight, Enemy.CapsuleRadius)
						> FMath::Square(SwordRadius + Enemy.CapsuleRadius))
						continue;

					MarkHit(Frag, Enemy.Handle);

					const TSubclassOf<UGameplayEffect> EffectClass(Frag.DamageEffectClass);
					if (Enemy.Actor && EffectClass)
					{
						const FVector SwordMid   = (Frag.SwordRootCurr + Frag.SwordTipCurr) * 0.5f;
						const FVector HitFromDir = (SwordMid - Enemy.CapsuleCenter).GetSafeNormal();
						Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Enemy.Actor, Frag.InstigatorEntity, EffectClass, Frag.Damage, HitFromDir, Frag.KnockbackStrength);
					}
					else
					{
						const float HpBefore = Enemy.Fragment->Health;
						Enemy.Fragment->Health = FMath::Max(0.f,
							HpBefore - LNPDamage::ApplyDefense(Frag.Damage, Enemy.Fragment->Defense));
						UE_LOG(LogLootNPop, Log, TEXT("[WeaponTrace][Entity] HP: %.1f -> %.1f (damage=%.1f)"),
							HpBefore, Enemy.Fragment->Health, Frag.Damage);
					}
				}

				// Player 공격 → Player 타겟 (아군 사격 켜진 경우만)
				if (bFriendlyFire)
				{
					for (FCollectedPlayer& Player : Players)
					{
						if (Player.Handle == Frag.InstigatorEntity || IsAlreadyHit(Frag, Player.Handle))
							continue;

						if (CalcDistSq(Player.CapsuleCenter, Player.UpDir, Player.CapsuleHalfHeight, Player.CapsuleRadius)
							> FMath::Square(SwordRadius + Player.CapsuleRadius))
							continue;

						MarkHit(Frag, Player.Handle);

						const TSubclassOf<UGameplayEffect> EffectClass(Frag.DamageEffectClass);
						if (Player.Actor && EffectClass)
						{
							const FVector SwordMid   = (Frag.SwordRootCurr + Frag.SwordTipCurr) * 0.5f;
							const FVector HitFromDir = (SwordMid - Player.CapsuleCenter).GetSafeNormal();
							Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Player.Actor, Frag.InstigatorEntity, EffectClass, Frag.Damage, HitFromDir, Frag.KnockbackStrength);
						}
					}
				}
			}
			else
			{
				// Enemy 공격 → Player 타겟 (패링/가드/피격 분기)
				const FVector AttackerLoc      = (Frag.SwordRootCurr + Frag.SwordTipCurr) * 0.5f;
				const float   SwordParryRadius = Frag.ParryRadius;

				for (FCollectedPlayer& Player : Players)
				{
					if (Player.Handle == Frag.InstigatorEntity || IsAlreadyHit(Frag, Player.Handle))
						continue;

					const FLNPParryStateFragment& PS          = Player.ParryState;
					const FVector                 AttackerDir = (AttackerLoc - Player.CapsuleCenter).GetSafeNormal();
					const float                   Dot         = FVector::DotProduct(Player.ForwardVector, AttackerDir);
					const float                   DistSq      = CalcDistSq(Player.CapsuleCenter, Player.UpDir, Player.CapsuleHalfHeight, Player.CapsuleRadius);

					// 1단계: 패링 체크 (ParryRadius — 피격보다 큰 반경)
					if (PS.bIsParrying && Dot >= PS.ParryAngleCos
						&& DistSq <= FMath::Square(SwordParryRadius + Player.CapsuleRadius))
					{
						MarkHit(Frag, Player.Handle);
						if (Player.Actor)
							Ctx.Defer().PushCommand<FLNPMeleeParryCommand>(Player.Actor, Frag.InstigatorEntity);
						continue;
					}

					// 2단계: 피격 체크 (HitRadius — 정상 반경)
					if (DistSq > FMath::Square(SwordRadius + Player.CapsuleRadius))
						continue;

					MarkHit(Frag, Player.Handle);
					if (!Player.Actor) continue;

					if (PS.bIsGuarding && Dot >= PS.GuardAngleCos)
					{
						Ctx.Defer().PushCommand<FLNPGuardBlockCommand>(Player.Actor);
						continue;
					}

					const TSubclassOf<UGameplayEffect> EffectClass(Frag.DamageEffectClass);
					if (EffectClass)
						Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Player.Actor, Frag.InstigatorEntity, EffectClass, Frag.Damage, AttackerDir, Frag.KnockbackStrength);
				}
			}
		}
	});
}

// ============================================================
// ULNPWeaponTraceLifetimeProcessor
// ============================================================

ULNPWeaponTraceLifetimeProcessor::ULNPWeaponTraceLifetimeProcessor()
	: Query(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::StartPhysics;
	ExecutionOrder.ExecuteAfter.Add(ULNPWeaponTraceHitDetectionProcessor::StaticClass()->GetFName());
}

void ULNPWeaponTraceLifetimeProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Query.AddRequirement<FLNPWeaponTraceFragment>(EMassFragmentAccess::ReadWrite);
	Query.RegisterWithProcessor(*this);
}

void ULNPWeaponTraceLifetimeProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	Query.ForEachEntityChunk(Context, [DeltaTime](FMassExecutionContext& Ctx)
	{
		TArrayView<FLNPWeaponTraceFragment> Frags = Ctx.GetMutableFragmentView<FLNPWeaponTraceFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			Frags[i].TimeToLive -= DeltaTime;

			if (Frags[i].TimeToLive <= 0.f)
			{
				const FMassEntityHandle Handle = Ctx.GetEntity(i);
				UE_LOG(LogLootNPop, Log, TEXT("[WeaponTrace] Melee entity TTL expired, force destroying."));
				Ctx.Defer().PushCommand<FMassCommandDestroyEntities>(
					TConstArrayView<FMassEntityHandle>(&Handle, 1));
			}
		}
	});
}

// ============================================================
// ULNPWeaponTraceDebugDrawProcessor
// ============================================================

#if WITH_EDITOR

ULNPWeaponTraceDebugDrawProcessor::ULNPWeaponTraceDebugDrawProcessor()
	: AttackerQuery(*this), PlayerQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::StartPhysics;
	ExecutionOrder.ExecuteAfter.Add(ULNPWeaponTraceHitDetectionProcessor::StaticClass()->GetFName());
}

void ULNPWeaponTraceDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	AttackerQuery.AddRequirement<FLNPWeaponTraceFragment>(EMassFragmentAccess::ReadOnly);
	AttackerQuery.RegisterWithProcessor(*this);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerQuery.RegisterWithProcessor(*this);
}

void ULNPWeaponTraceDebugDrawProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
		return;

	// Player 위치 전체 수집
	TArray<FVector> PlayerLocations;
	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			PlayerLocations.Add(Transforms[i].GetTransform().GetLocation());
	});

	const float MeleeProximityDistSq = GetDefault<ULNPSettings>()->DebugDrawProximityDistSq;
	auto Batcher = UE::Mass::Debug::FLineBatcher::MakeLineBatcher(World);

	auto IsNearAnyPlayer = [&](const FVector& Pos) -> bool
	{
		for (const FVector& PL : PlayerLocations)
		{
			if (FVector::DistSquared(Pos, PL) < MeleeProximityDistSq)
				return true;
		}
		return false;
	};

	AttackerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FLNPWeaponTraceFragment> Attackers = Ctx.GetFragmentView<FLNPWeaponTraceFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FLNPWeaponTraceFragment& Frag     = Attackers[i];
			const FVector                  SwordMid = (Frag.SwordRootCurr + Frag.SwordTipCurr) * 0.5f;
			if (!IsNearAnyPlayer(SwordMid))
				continue;

			Batcher.DrawSphere(Frag.SwordRootCurr, Frag.HitRadius,   FLinearColor(FColor::Green));
			Batcher.DrawSphere(Frag.SwordTipCurr,  Frag.HitRadius,   FLinearColor(FColor::Green));
			Batcher.DrawSphere(Frag.SwordRootCurr, Frag.ParryRadius, FLinearColor(FColor::Emerald));
			Batcher.DrawSphere(Frag.SwordTipCurr,  Frag.ParryRadius, FLinearColor(FColor::Emerald));

			const FVector SwordDir = (Frag.SwordTipCurr - Frag.SwordRootCurr).GetSafeNormal();
			FVector Perp = FVector::CrossProduct(SwordDir, FVector::UpVector);
			if (Perp.IsNearlyZero())
				Perp = FVector::CrossProduct(SwordDir, FVector::ForwardVector);
			Perp.Normalize();
			const FVector Perp2 = FVector::CrossProduct(SwordDir, Perp);

			// 칼날 두께 시각화 (HitRadius)
			//const float   HR  = Frag.HitRadius;
			//const float   HR3 = HR / 3.f;
			//const FVector HitOff[8] = {
			//	 Perp * HR  + Perp2 * HR3,  Perp * HR  - Perp2 * HR3,
			//	-Perp * HR  + Perp2 * HR3, -Perp * HR  - Perp2 * HR3,
			//	 Perp * HR3 + Perp2 * HR,   Perp * HR3 - Perp2 * HR,
			//	-Perp * HR3 + Perp2 * HR,  -Perp * HR3 - Perp2 * HR,
			//};
			//for (const FVector& Off : HitOff)
			//	Batcher.LineBatcherInstance->DrawLine(Frag.SwordRootCurr + Off, Frag.SwordTipCurr + Off, FLinearColor(FColor::Green), 0, 0.5f, Batcher.LifeTime);

			// 패링 판정 반경 시각화 (ParryRadius)
			const float   PR  = Frag.ParryRadius;
			const float   PR3 = PR / 3.f;
			const FVector ParryOff[8] = {
				 Perp * PR  + Perp2 * PR3,  Perp * PR  - Perp2 * PR3,
				-Perp * PR  + Perp2 * PR3, -Perp * PR  - Perp2 * PR3,
				 Perp * PR3 + Perp2 * PR,   Perp * PR3 - Perp2 * PR,
				-Perp * PR3 + Perp2 * PR,  -Perp * PR3 - Perp2 * PR,
			};
			for (const FVector& Off : ParryOff)
				Batcher.LineBatcherInstance->DrawLine(Frag.SwordRootCurr + Off, Frag.SwordTipCurr + Off, FLinearColor(FColor::Emerald), 0, 0.5f, Batcher.LifeTime);
		}
	});
}

#else

ULNPWeaponTraceDebugDrawProcessor::ULNPWeaponTraceDebugDrawProcessor()
	: AttackerQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;
}
void ULNPWeaponTraceDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>&) {}
void ULNPWeaponTraceDebugDrawProcessor::Execute(FMassEntityManager&, FMassExecutionContext&) {}

#endif
