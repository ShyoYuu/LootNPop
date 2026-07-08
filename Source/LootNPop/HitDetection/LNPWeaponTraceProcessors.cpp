// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPWeaponTraceProcessors.h"
#include "HitDetection/LNPWeaponTraceMassTypes.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "HitDetection/LNPGuardParryTypes.h"
#include "HitDetection/LNPPositionHistoryFragment.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "Character/LNPPlayerCharacter.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GAS/Effects/LNPGameplayEffect_Damage.h"
#include "GAS/LNPDamageFormula.h"
#include "Config/LNPSettings.h"
#include "LootNPop.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerState.h"
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

	/** 클라이언트 예측 전용: ApplyLocalHitFeedback()(내부적으로 GetWorldTimerManager().SetTimer 호출)은 게임 스레드 전용이라
	 *  Mass Execute()(워커 스레드에서 돌 수 있음)에서 직접 호출할 수 없다. Command Buffer flush(게임 스레드 보장)로 위탁한다. */
	struct FLNPLocalHitFeedbackCommand : public FMassBatchedCommand
	{
		struct FEntry
		{
			FMassEntityHandle                 AttackerEntity;
			TWeakObjectPtr<ALNPCharacterBase>  AttackerActor;
		};

		FLNPLocalHitFeedbackCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

		void Add(FMassEntityHandle InAttackerEntity, ALNPCharacterBase* InAttackerActor)
		{
			Entries.Add({ InAttackerEntity, InAttackerActor });
			bHasWork = true;
		}

		virtual void Run(FMassEntityManager& EntityManager) override
		{
			for (const FEntry& Entry : Entries)
			{
				FLNPWeaponTraceFragment* Frag = EntityManager.GetFragmentDataPtr<FLNPWeaponTraceFragment>(Entry.AttackerEntity);
				if (!Frag || Frag->bLocalFeedbackFired)
					continue;

				if (ALNPCharacterBase* AttackerChar = Entry.AttackerActor.Get())
					AttackerChar->ApplyLocalHitFeedback();

				Frag->bLocalFeedbackFired = true;
			}
		}

		virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
		virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
		virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

	private:
		TArray<FEntry> Entries;
	};
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
	EnemyQuery.AddRequirement<FLNPPositionHistoryFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyDyingTag>(EMassFragmentPresence::None);
	EnemyQuery.RegisterWithProcessor(*this);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	PlayerQuery.AddRequirement<FLNPParryStateFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FLNPPositionHistoryFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerQuery.RegisterWithProcessor(*this);

}

void ULNPWeaponTraceHitDetectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	const bool bIsServer = World && World->GetNetMode() < NM_Client;

	// 클라이언트: 로컬 컨트롤 공격자에 한해 Mass 엔티티 쿼리 기반 예측 판정 (코스메틱 HitStop만, GE 미적용).
	// 서버 판정(Mass 엔티티 쿼리 + GE 적용)과 완전히 분리된 경로다.
	if (!bIsServer)
	{
		// 클라이언트 예측 전용 타겟 캡슐 수집 — Enemy MassReplication(Phase 6) 이후 EnemyQuery/PlayerQuery가
		// 클라이언트에도 유효한 엔티티를 반환하므로, 게임 스레드 전용인 TActorIterator 없이 서버 Pass 1/2와 동일하게 조회한다.
		struct FClientCapsuleTarget
		{
			AActor* Actor;
			FVector CapsuleCenter;
			FVector UpDir;
			float   CapsuleHalfHeight;
			float   CapsuleRadius;
			bool    bIsEnemy;
		};
		TArray<FClientCapsuleTarget> ClientTargets;

		EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
		{
			const FLNPEnemySharedFragment& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
			if (!Shared.Config)
				return;

			const float HalfH  = Shared.Config->CapsuleHalfHeight;
			const float Radius = Shared.Config->CapsuleRadius;

			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FMassActorFragment>            ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				const FVector Loc   = Transforms[i].GetTransform().GetLocation();
				const FVector UpDir = (-Loc).GetSafeNormal();
				AActor*       Actor = ActorFrags[i].GetMutable();
				const FVector Center = Cast<ALNPCharacterBase>(Actor) ? Loc : Loc + UpDir * HalfH;
				ClientTargets.Add({ Actor, Center, UpDir, HalfH, Radius, true });
			}
		});

		PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FMassActorFragment>            ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				AActor* Actor = ActorFrags[i].GetMutable();
				float HalfH, Radius;
				LNPHitDetection::GetCapsuleSize(Actor ? Actor->FindComponentByClass<UCapsuleComponent>() : nullptr, HalfH, Radius);
				const FVector Loc = Transforms[i].GetTransform().GetLocation();
				ClientTargets.Add({ Actor, Loc, (-Loc).GetSafeNormal(), HalfH, Radius, false });
			}
		});

		if (ClientTargets.IsEmpty())
			return;

		const bool bFriendlyFireClient = GetDefault<ULNPSettings>()->bFriendlyFire;

		AttackerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
		{
			TArrayView<FLNPWeaponTraceFragment> Attackers = Ctx.GetMutableFragmentView<FLNPWeaponTraceFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				FLNPWeaponTraceFragment& Frag = Attackers[i];
				if (!Frag.bIsLocalInstigator || Frag.bLocalFeedbackFired)
					continue;

				ALNPCharacterBase* AttackerChar = Frag.InstigatorActor.Get();
				if (!AttackerChar)
					continue;

				const bool  bDegenerate  = FVector::DistSquared(Frag.SwordTipPrev, Frag.SwordTipCurr) < 1.f;
				const float SwordRadius  = Frag.HitRadius;

				for (const FClientCapsuleTarget& Target : ClientTargets)
				{
					if (!Target.Actor || Target.Actor == AttackerChar)
						continue;

					if (Frag.InstigatorTeam == ELNPInstigatorTeam::Player && !Target.bIsEnemy && !bFriendlyFireClient)
						continue;
					if (Frag.InstigatorTeam == ELNPInstigatorTeam::Enemy && Target.bIsEnemy)
						continue;

					FVector CapsuleBot, CapsuleTop;
					MakeCapsuleSeg(Target.CapsuleCenter, Target.UpDir, Target.CapsuleHalfHeight, Target.CapsuleRadius, CapsuleBot, CapsuleTop);

					float DistSq;
					if (bDegenerate)
					{
						FVector P1, P2;
						FMath::SegmentDistToSegment(Frag.SwordRootCurr, Frag.SwordTipCurr, CapsuleBot, CapsuleTop, P1, P2);
						DistSq = FVector::DistSquared(P1, P2);
					}
					else
					{
						DistSq = SweptQuadCapsuleDistSq(
							Frag.SwordRootPrev, Frag.SwordTipPrev,
							Frag.SwordRootCurr, Frag.SwordTipCurr,
							CapsuleBot, CapsuleTop);
					}

					if (DistSq > FMath::Square(SwordRadius + Target.CapsuleRadius))
						continue;

					Ctx.Defer().PushCommand<FLNPLocalHitFeedbackCommand>(Ctx.GetEntity(i), AttackerChar);
					break;  // 스윙당 예측 피드백 1회 — 서버 확정 결과가 최종
				}
			}
		});
		return;
	}

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
		FVector            RawLocation;      // Lag Compensation 되감기 기준 원점
		const FLNPPositionHistoryFragment* History;
	};
	TArray<FCollectedEnemy> Enemies;

	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPEnemySharedFragment& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
		if (!Shared.Config)
			return;

		const float HalfH  = Shared.Config->CapsuleHalfHeight;
		const float Radius = Shared.Config->CapsuleRadius;

		const TConstArrayView<FTransformFragment>           Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FLNPEnemyFragment>                       EnemyFrags = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		TArrayView<FMassActorFragment>                      ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPPositionHistoryFragment>  Histories  = Ctx.GetFragmentView<FLNPPositionHistoryFragment>();

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
			Enemies.Add({ Center, UpDir, HalfH, Radius, &EnemyFrags[i], Ctx.GetEntity(i), Actor, Loc, &Histories[i] });
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
		FVector                RawLocation;    // Lag Compensation 되감기 기준 원점
		const FLNPPositionHistoryFragment* History;
	};
	TArray<FCollectedPlayer> Players;

	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment>          Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FMassActorFragment>                     ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPParryStateFragment>      ParryFrags = Ctx.GetFragmentView<FLNPParryStateFragment>();
		const TConstArrayView<FLNPPositionHistoryFragment> Histories  = Ctx.GetFragmentView<FLNPPositionHistoryFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			AActor* Actor = ActorFrags[i].GetMutable();
			float HalfH, Radius;
			LNPHitDetection::GetCapsuleSize(Actor ? Actor->FindComponentByClass<UCapsuleComponent>() : nullptr, HalfH, Radius);
			const FTransform& T   = Transforms[i].GetTransform();
			const FVector     Loc = T.GetLocation();
			const FVector     Up  = (-Loc).GetSafeNormal();
			Players.Add({ Loc, Up, T.GetRotation().GetForwardVector(), HalfH, Radius, Ctx.GetEntity(i), Actor, ParryFrags[i], Loc, &Histories[i] });
		}
	});

	if (Enemies.IsEmpty() && Players.IsEmpty())
		return;

	const bool bFriendlyFire = GetDefault<ULNPSettings>()->bFriendlyFire;

	// ── Pass 3: Swept Volume 피격 판정 (Lag Compensation 포함) ─────────────────
	UMassActorSubsystem* ActorSubForRewind = World->GetSubsystem<UMassActorSubsystem>();
	constexpr float MaxRewindSeconds = 0.2f; // 섹션 5.0 — 되감기 클램프 상한 200ms
	const double NowForRewind = World->GetTimeSeconds();

	AttackerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		TArrayView<FLNPWeaponTraceFragment> Attackers = Ctx.GetMutableFragmentView<FLNPWeaponTraceFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPWeaponTraceFragment& Frag = Attackers[i];

			// 공격자 RTT/2만큼 과거 시점의 피격 대상 위치로 판정한다 (섹션 5.0).
			// 공격자가 Player가 아니면(Enemy AI) PlayerController가 없어 RewindSeconds=0 → 보정 없음.
			float RewindSeconds = 0.f;
			if (ActorSubForRewind && Frag.InstigatorEntity.IsSet())
			{
				if (const ALNPCharacterBase* AttackerChar = Cast<ALNPCharacterBase>(ActorSubForRewind->GetActorFromHandle(Frag.InstigatorEntity)))
				{
					if (const APlayerState* AttackerPS = AttackerChar->GetPlayerState<APlayerState>())
						RewindSeconds = FMath::Clamp(AttackerPS->GetPingInMilliseconds() * 0.0005f, 0.f, MaxRewindSeconds);
				}
			}
			const double RewindQueryTime = NowForRewind - RewindSeconds;

			// 되감긴 캡슐 중심 = 현재 캡슐 중심 + (과거 원점 - 현재 원점). RewindSeconds가 0이면 원본 그대로 반환.
			auto RewoundCenter = [&](const FVector& CapsuleCenter, const FVector& RawLoc, const FLNPPositionHistoryFragment* History) -> FVector
			{
				if (RewindSeconds <= 0.f || !History)
					return CapsuleCenter;
				return CapsuleCenter + (History->GetInterpolatedLocation(RewindQueryTime) - RawLoc);
			};

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

			// Player 타겟 공용 2단계 판정 (1단계 패링 → 2단계 가드/피격).
			// 근접 PvP(아군 사격)와 Enemy→Player가 반드시 동일한 로직을 타야 한다 —
			// 과거 두 분기가 복제되어 있던 시절 PvP 쪽에만 패링 체크가 누락된 버그가 있었다 (Networking Phase 3).
			const FVector AttackerLoc      = (Frag.SwordRootCurr + Frag.SwordTipCurr) * 0.5f;
			const float   SwordParryRadius = Frag.ParryRadius;

			auto JudgePlayerTarget = [&](FCollectedPlayer& Player)
			{
				if (Player.Handle == Frag.InstigatorEntity || IsAlreadyHit(Frag, Player.Handle))
					return;

				const FLNPParryStateFragment& PS          = Player.ParryState;
				const FVector                 AttackerDir = (AttackerLoc - Player.CapsuleCenter).GetSafeNormal();
				const float                   Dot         = FVector::DotProduct(Player.ForwardVector, AttackerDir);
				const float                   DistSq      = CalcDistSq(RewoundCenter(Player.CapsuleCenter, Player.RawLocation, Player.History), Player.UpDir, Player.CapsuleHalfHeight, Player.CapsuleRadius);

				// 1단계: 패링 체크 (ParryRadius — 피격보다 큰 반경, 서버 만료 시각으로 RTT 보정)
				if (PS.bIsParrying && (PS.ParryWindowExpiryTime < 0.0 || NowForRewind <= PS.ParryWindowExpiryTime) && Dot >= PS.ParryAngleCos
					&& DistSq <= FMath::Square(SwordParryRadius + Player.CapsuleRadius))
				{
					MarkHit(Frag, Player.Handle);
					if (Player.Actor)
						Ctx.Defer().PushCommand<FLNPMeleeParryCommand>(Player.Actor, Frag.InstigatorEntity);
					return;
				}

				// 2단계: 피격 체크 (HitRadius — 정상 반경)
				if (DistSq > FMath::Square(SwordRadius + Player.CapsuleRadius))
					return;

				MarkHit(Frag, Player.Handle);
				if (!Player.Actor)
					return;

				if (PS.bIsGuarding && Dot >= PS.GuardAngleCos)
				{
					Ctx.Defer().PushCommand<FLNPGuardBlockCommand>(Player.Actor);
					return;
				}

				const TSubclassOf<UGameplayEffect> EffectClass(Frag.DamageEffectClass);
				if (EffectClass)
					Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Player.Actor, Frag.InstigatorEntity, EffectClass, Frag.Damage, AttackerDir, Frag.KnockbackStrength, true);
			};

			if (Frag.InstigatorTeam == ELNPInstigatorTeam::Player)
			{
				// Player 공격 → Enemy 타겟
				for (FCollectedEnemy& Enemy : Enemies)
				{
					if (Enemy.Handle == Frag.InstigatorEntity || IsAlreadyHit(Frag, Enemy.Handle))
						continue;

					if (CalcDistSq(RewoundCenter(Enemy.CapsuleCenter, Enemy.RawLocation, Enemy.History), Enemy.UpDir, Enemy.CapsuleHalfHeight, Enemy.CapsuleRadius)
						> FMath::Square(SwordRadius + Enemy.CapsuleRadius))
						continue;

					MarkHit(Frag, Enemy.Handle);

					const TSubclassOf<UGameplayEffect> EffectClass(Frag.DamageEffectClass);
					if (Enemy.Actor && EffectClass)
					{
						const FVector HitFromDir = (AttackerLoc - Enemy.CapsuleCenter).GetSafeNormal();
						Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Enemy.Actor, Frag.InstigatorEntity, EffectClass, Frag.Damage, HitFromDir, Frag.KnockbackStrength, true);
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

				// Player 공격 → Player 타겟 (아군 사격이 켜진 경우만)
				if (bFriendlyFire)
				{
					for (FCollectedPlayer& Player : Players)
						JudgePlayerTarget(Player);
				}
			}
			else
			{
				// Enemy 공격 → Player 타겟
				for (FCollectedPlayer& Player : Players)
					JudgePlayerTarget(Player);
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
