// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPProjectileProcessors.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "HitDetection/LNPProjectileVisualSubsystem.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "HitDetection/LNPGuardParryTypes.h"
#include "HitDetection/LNPGhostProjectileSubsystem.h"
#include "HitDetection/LNPProjectileImpactContext.h"
#include "HitDetection/LNPPositionHistoryFragment.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "Character/LNPPlayerCharacter.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
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
	/**
	 * 선분(A→B)이 Center/UpDir/HalfHeight/CombinedRadius의 캡슐과 교차하면 true.
	 * Center는 캡슐 중심(바닥이 아닌 실린더 중앙).
	 */
	bool SegmentHitsCapsule(
		FVector A, FVector B,
		FVector Center, FVector UpDir,
		float   CapsuleHalfHeight, float CombinedRadius,
		FVector& OutHitPoint)
	{
		const FVector Closest      = FMath::ClosestPointOnSegment(Center, A, B);
		const FVector Delta        = Closest - Center;
		const float   Axial        = FVector::DotProduct(Delta, UpDir);
		const FVector RadialVec    = Delta - UpDir * Axial;
		const float   RadialDistSq = RadialVec.SizeSquared();

		if (FMath::Abs(Axial) <= CapsuleHalfHeight && RadialDistSq <= FMath::Square(CombinedRadius))
		{
			OutHitPoint = Closest;
			return true;
		}
		return false;
	}

	/** 클라이언트 예측 전용: ULNPGhostProjectileSubsystem::Ghosts(TMap)는 RPC 콜백(게임 스레드)에서도 갱신되므로,
	 *  Mass Execute()(워커 스레드에서 돌 수 있음)에서 직접 건드리면 데이터 레이스다. Command Buffer flush로 위탁한다. */
	struct FLNPGhostSweepCommand : public FMassBatchedCommand
	{
		FLNPGhostSweepCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

		void Add() { bHasWork = true; }

		virtual void Run(FMassEntityManager& EntityManager) override
		{
			UWorld* World = EntityManager.GetWorld();
			if (ULNPGhostProjectileSubsystem* GhostSub = World ? World->GetSubsystem<ULNPGhostProjectileSubsystem>() : nullptr)
				GhostSub->SweepExpiredGhosts();
		}

		virtual void Reset() override { FMassBatchedCommand::Reset(); }
		virtual SIZE_T GetAllocatedSize()     const override { return 0; }
		virtual int32  GetNumOperationsStat() const override { return 1; }
	};

	/** 로컬 코스메틱 판정에 의한 Ghost 파괴 — 서버 확정 큐의 VFX 중복 재생을 막기 위해 키를 기록한다. */
	struct FLNPGhostDestroyCommand : public FMassBatchedCommand
	{
		FLNPGhostDestroyCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

		void Add(const FLNPGhostKey& InKey)
		{
			Entries.Add(InKey);
			bHasWork = true;
		}

		virtual void Run(FMassEntityManager& EntityManager) override
		{
			UWorld* World = EntityManager.GetWorld();
			ULNPGhostProjectileSubsystem* GhostSub = World ? World->GetSubsystem<ULNPGhostProjectileSubsystem>() : nullptr;
			if (!GhostSub)
				return;

			for (const FLNPGhostKey& Key : Entries)
				GhostSub->DestroyGhostFromLocalImpact(Key);
		}

		virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
		virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
		virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

	private:
		TArray<FLNPGhostKey> Entries;
	};
}

// ============================================================
// ULNPProjectileMovementProcessor
// ============================================================

ULNPProjectileMovementProcessor::ULNPProjectileMovementProcessor()
	: ProjectileQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void ULNPProjectileMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProjectileQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.AddRequirement<FLNPProjectileFragment>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.AddRequirement<FLNPProjectileVisualFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddConstSharedRequirement<FLNPProjectileSharedFragment>(EMassFragmentPresence::All);
	ProjectileQuery.RegisterWithProcessor(*this);
	ProcessorRequirements.AddSubsystemRequirement<ULNPSurfaceCacheSubsystem>(EMassFragmentAccess::ReadOnly);
	ProcessorRequirements.AddSubsystemRequirement<ULNPProjectileVisualSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPProjectileMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	const ULNPSurfaceCacheSubsystem& SurfaceCache = Context.GetSubsystemChecked<ULNPSurfaceCacheSubsystem>();
	ULNPProjectileVisualSubsystem& VisualSub      = Context.GetMutableSubsystemChecked<ULNPProjectileVisualSubsystem>();

	ProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPProjectileSharedFragment&                  Shared      = Ctx.GetConstSharedFragment<FLNPProjectileSharedFragment>();
		TArrayView<FTransformFragment>                       Transforms  = Ctx.GetMutableFragmentView<FTransformFragment>();
		TArrayView<FLNPProjectileFragment>                   Projectiles = Ctx.GetMutableFragmentView<FLNPProjectileFragment>();
		const TConstArrayView<FLNPProjectileVisualFragment>  Visuals     = Ctx.GetFragmentView<FLNPProjectileVisualFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPProjectileFragment& Proj       = Projectiles[i];
			FTransform&             Transform  = Transforms[i].GetMutableTransform();
			const FVector           CurrentPos = Transform.GetLocation();

			Proj.PreviousPos = CurrentPos;
			const FVector NewPos = CurrentPos + Proj.Velocity * DeltaTime;
			Transform.SetLocation(NewPos);
			Proj.LifetimeRemaining -= DeltaTime;

			bool    bShouldDestroy = Proj.LifetimeRemaining <= 0.0f;
			bool    bHitSurface    = false;
			FVector ImpactNormal   = -NewPos.GetSafeNormal();

			if (!bShouldDestroy)
			{
				FVector SurfacePoint;
				if (SurfaceCache.GetSurfacePoint(NewPos.GetSafeNormal(), SurfacePoint)
					&& NewPos.SizeSquared() >= SurfacePoint.SizeSquared())
				{
					bShouldDestroy = true;
					bHitSurface    = true;
				}
			}

			if (bShouldDestroy)
			{
				const FMassEntityHandle Entity = Ctx.GetEntity(i);
				if (Visuals[i].bInitialized)
					VisualSub.EnqueueTrailRelease(Entity);
				VisualSub.EnqueueImpact(Shared.VFXData, NewPos, ImpactNormal);

				Ctx.Defer().AddTag<FLNPProjectileDeadTag>(Entity);
			}
		}
	});
}

// ============================================================
// ULNPProjectileHitDetectionProcessor
// ============================================================

ULNPProjectileHitDetectionProcessor::ULNPProjectileHitDetectionProcessor()
	: ProjectileQuery(*this), EnemyQuery(*this), PlayerQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::StartPhysics;
}

void ULNPProjectileHitDetectionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProjectileQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddRequirement<FLNPProjectileFragment>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.AddRequirement<FLNPProjectileVisualFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddConstSharedRequirement<FLNPProjectileSharedFragment>(EMassFragmentPresence::All);
	ProjectileQuery.AddTagRequirement<FLNPProjectileDeadTag>(EMassFragmentPresence::None);
	ProjectileQuery.RegisterWithProcessor(*this);

	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FLNPPositionHistoryFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.RegisterWithProcessor(*this);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	PlayerQuery.AddRequirement<FLNPParryStateFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FLNPPositionHistoryFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<ULNPProjectileVisualSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPProjectileHitDetectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	const bool bIsServer = World && World->GetNetMode() < NM_Client;

	ULNPProjectileVisualSubsystem& VisualSub = Context.GetMutableSubsystemChecked<ULNPProjectileVisualSubsystem>();

	// 클라이언트: 로컬 예측 공격자의 Ghost Projectile에 한해 Physics/Actor 기반 예측 판정 (코스메틱 HitStop만, GE 미적용).
	// 서버 판정(Mass 엔티티 쿼리 + GE 적용)과 완전히 분리된 경로다.
	if (!bIsServer)
	{
		if (!World)
			return;

		Context.Defer().PushCommand<FLNPGhostSweepCommand>();

		// 클라이언트 예측 전용 타겟 캡슐 수집 — Enemy MassReplication(Phase 6) 이후 EnemyQuery/PlayerQuery가
		// 클라이언트에도 유효한 엔티티를 반환하므로, 게임 스레드 전용인 TActorIterator 없이 서버 Pass 1/2와 동일하게 조회한다.
		struct FClientCapsuleTarget
		{
			AActor* Actor;            // Enemy 엔티티는 클라이언트에서 Actor 미링크(nullptr)일 수 있음 — 판정에는 불필요
			FVector Location;
			FVector UpDir;
			float   CapsuleHalfHeight;
			float   CapsuleRadius;
			bool    bIsEnemy;
			int32   PlayerID;         // Player 타겟의 PlayerId — 관전용 Ghost가 발사자 본인 캡슐에 자폭하지 않도록 제외용
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
				const FVector Loc = Transforms[i].GetTransform().GetLocation();
				ClientTargets.Add({ ActorFrags[i].GetMutable(), Loc, (-Loc).GetSafeNormal(), HalfH, Radius, true, INDEX_NONE });
			}
		});

		PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FMassActorFragment>            ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				AActor* PlayerActor = ActorFrags[i].GetMutable();
				const ALNPPlayerCharacter* PlayerPawn = Cast<ALNPPlayerCharacter>(PlayerActor);
				if (!PlayerPawn)
					continue;

				float HalfH, Radius;
				LNPHitDetection::GetCapsuleSize(PlayerPawn->GetCapsule(), HalfH, Radius);
				const APlayerState* TargetPS = PlayerPawn->GetPlayerState<APlayerState>();
				const FVector Loc = Transforms[i].GetTransform().GetLocation();
				ClientTargets.Add({ PlayerActor, Loc, (-Loc).GetSafeNormal(), HalfH, Radius, false,
					TargetPS ? TargetPS->GetPlayerId() : INDEX_NONE });
			}
		});

		if (ClientTargets.IsEmpty())
			return;

		const bool bFriendlyFireClient = GetDefault<ULNPSettings>()->bFriendlyFire;
		UMassActorSubsystem* ActorSub  = World->GetSubsystem<UMassActorSubsystem>();

		ProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
		{
			const FLNPProjectileSharedFragment&       Shared      = Ctx.GetConstSharedFragment<FLNPProjectileSharedFragment>();
			const TConstArrayView<FTransformFragment> Transforms  = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FLNPProjectileFragment>        Projectiles = Ctx.GetMutableFragmentView<FLNPProjectileFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				// 공격자 예측 Ghost(bIsLocalInstigator)와 관전용 Ghost 모두 로컬 코스메틱 충돌을 수행한다.
				// 관전용도 로컬에서 즉시 소멸·VFX를 재생해야 "캐릭터를 관통한 뒤 뒤늦게 터지는" 잔상이 없다.
				// 서버 확정 큐가 도착하면 DestroyGhostFromLocalImpact가 기록한 키로 VFX 중복 재생을 걸러낸다.
				FLNPProjectileFragment& Proj = Projectiles[i];

				const FVector CurrentPos = Transforms[i].GetTransform().GetLocation();
				AActor* InstigatorActor = (ActorSub && Proj.Instigator.IsSet()) ? ActorSub->GetActorFromHandle(Proj.Instigator) : nullptr;

				for (const FClientCapsuleTarget& Target : ClientTargets)
				{
					// 발사자 자신 제외 — 로컬 예측 Ghost는 Instigator 핸들로, 관전용 Ghost는 PlayerID로 걸러낸다.
					if (Target.Actor && InstigatorActor && Target.Actor == InstigatorActor)
						continue;
					if (!Target.bIsEnemy && Target.PlayerID != INDEX_NONE && Target.PlayerID == Proj.InstigatorPlayerID)
						continue;

					if (Proj.InstigatorTeam == ELNPInstigatorTeam::Player && !Target.bIsEnemy && !bFriendlyFireClient)
						continue;
					if (Proj.InstigatorTeam == ELNPInstigatorTeam::Enemy && Target.bIsEnemy)
						continue;

					FVector HitPoint;
					if (!SegmentHitsCapsule(Proj.PreviousPos, CurrentPos, Target.Location, Target.UpDir,
						Target.CapsuleHalfHeight, Target.CapsuleRadius + Shared.HitRadius, HitPoint))
						continue;

					// 원거리는 공격자 HitStop을 재생하지 않는다 (근접 전용 — 물리적 충돌감이 없어 어색함).
					// Ghost 즉시 소멸(트레일 관통 방지) + 예측 위치 임팩트 VFX. 서버 확정 결과가 최종.
					Ctx.Defer().PushCommand<FLNPGhostDestroyCommand>(
						FLNPGhostKey{ Proj.InstigatorPlayerID, Proj.PredictionKeyID, Proj.SpawnIndex });
					VisualSub.EnqueueImpact(Shared.VFXData, HitPoint, (HitPoint - Target.Location).GetSafeNormal());

					break;  // 이 Projectile은 예측 판정 종료 — 서버 확정 결과가 최종
				}
			}
		});
		return;
	}

	const double Now = World ? World->GetTimeSeconds() : 0.0; // 패링 창 RTT 역보정용 (섹션 5.1)

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
		if (nullptr == Shared.Config)
			return;

		const float HalfH  = Shared.Config->CapsuleHalfHeight;
		const float Radius = Shared.Config->CapsuleRadius;

		const TConstArrayView<FTransformFragment>          Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FLNPEnemyFragment>                       EnemyFrags = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		TArrayView<FMassActorFragment>                      ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPPositionHistoryFragment>  Histories  = Ctx.GetFragmentView<FLNPPositionHistoryFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector Loc   = Transforms[i].GetTransform().GetLocation();
			const FVector UpDir = (-Loc).GetSafeNormal();
			Enemies.Add({ Loc + UpDir * HalfH, UpDir, HalfH, Radius, &EnemyFrags[i], Ctx.GetEntity(i), ActorFrags[i].GetMutable(), Loc, &Histories[i] });
		}
	});

	// ── Pass 2: Player 캡슐 데이터 수집 ───────────────────────────────────────
	struct FCollectedPlayer
	{
		FVector                Location;       // 액터 위치 (충돌 판정 중심으로 사용)
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
			AActor* PlayerActor = ActorFrags[i].GetMutable();
			const ALNPPlayerCharacter* PlayerPawn = Cast<ALNPPlayerCharacter>(PlayerActor);
			if (PlayerPawn == nullptr)
				continue;

			float HalfH, Radius;
			LNPHitDetection::GetCapsuleSize(PlayerPawn->GetCapsule(), HalfH, Radius);
			const FTransform& T   = Transforms[i].GetTransform();
			const FVector     Loc = T.GetLocation();
			Players.Add({
				Loc,
				(-Loc).GetSafeNormal(),
				T.GetRotation().GetForwardVector(),
				HalfH,
				Radius,
				Ctx.GetEntity(i),
				PlayerActor,
				ParryFrags[i],
				Loc,
				&Histories[i]
			});
		}
	});

	if (Enemies.IsEmpty() && Players.IsEmpty())
		return;

	const bool bFriendlyFire = GetDefault<ULNPSettings>()->bFriendlyFire;

	// ExcludeEnemy/ExcludePlayer = 직접 피격 대상 (중복 피해 방지)
	auto ApplySplash = [&](FMassExecutionContext& Ctx,
		const FLNPProjectileSharedFragment& Shared,
		const FLNPProjectileFragment&       Proj,
		FVector                             HitPoint,
		const FCollectedEnemy*              ExcludeEnemy,
		const FCollectedPlayer*             ExcludePlayer,
		bool                                bFF)
	{
		if (Shared.ExplosionRadius <= 0.f || !Shared.DamageEffectClass)
			return;

		const float ExpRadSq = FMath::Square(Shared.ExplosionRadius);

		if (Proj.InstigatorTeam == ELNPInstigatorTeam::Player)
		{
			for (const FCollectedEnemy& SE : Enemies)
			{
				if (&SE == ExcludeEnemy) continue;
				if (!SE.Actor) continue;
				if (FVector::DistSquared(SE.CapsuleCenter, HitPoint) > ExpRadSq) continue;
				Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(
					SE.Actor, Proj.Instigator, Shared.DamageEffectClass,
					Shared.Damage, (SE.CapsuleCenter - HitPoint).GetSafeNormal(), Shared.SplashKnockbackStrength);
			}
		}

		if (Proj.InstigatorTeam == ELNPInstigatorTeam::Enemy || bFF)
		{
			for (const FCollectedPlayer& SP : Players)
			{
				if (&SP == ExcludePlayer) continue;
				if (!SP.Actor) continue;
				if (FVector::DistSquared(SP.Location, HitPoint) > ExpRadSq) continue;
				Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(
					SP.Actor, Proj.Instigator, Shared.DamageEffectClass,
					Shared.Damage, (SP.Location - HitPoint).GetSafeNormal(), Shared.SplashKnockbackStrength);
			}
		}
	};

	// ── Pass 3: 선분 vs 캡슐 충돌 판정 (Lag Compensation 포함) ─────────────────
	constexpr float MaxRewindSeconds = 0.2f; // 섹션 5.0 — 되감기 클램프 상한 200ms

	ProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPProjectileSharedFragment&                 Shared      = Ctx.GetConstSharedFragment<FLNPProjectileSharedFragment>();
		const TConstArrayView<FTransformFragment>           Transforms  = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FLNPProjectileFragment>                  Projectiles = Ctx.GetMutableFragmentView<FLNPProjectileFragment>();
		const TConstArrayView<FLNPProjectileVisualFragment> Visuals     = Ctx.GetFragmentView<FLNPProjectileVisualFragment>();

		const float HitRadius   = Shared.HitRadius;
		const float ParryRadius = Shared.ParryRadius;

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FLNPProjectileFragment& Proj       = Projectiles[i];
			const FVector           CurrentPos = Transforms[i].GetTransform().GetLocation();
			const FMassEntityHandle ProjEnt    = Ctx.GetEntity(i);

			// 공격자(발사자) RTT/2만큼 과거 시점의 피격 대상 위치로 판정한다 (섹션 5.0).
			// 발사(또는 패링 반사) 시점에 1회 캐싱된 값을 재사용 — "공격자가 조준해서 쏜 순간의 지연"만 보정하고
			// 이후에는 대상의 현재 위치와 비교한다. 매 프레임 재계산 시 느린/유도 발사체가 비행 내내
			// 대상의 과거 잔상을 쫓아가 맞는 문제와, 매 프레임 Actor→PlayerState→Ping 조회 비용을 함께 제거.
			const float  RewindSeconds   = FMath::Clamp(Proj.CachedRewindSeconds, 0.f, MaxRewindSeconds);
			const double RewindQueryTime = Now - RewindSeconds;

			// 되감긴 캡슐 중심 = 현재 캡슐 중심 + (과거 원점 - 현재 원점). RewindSeconds가 0이면 원본 그대로 반환.
			auto RewoundCenter = [&](const FVector& CapsuleCenter, const FVector& RawLoc, const FLNPPositionHistoryFragment* History) -> FVector
			{
				if (RewindSeconds <= 0.f || !History)
					return CapsuleCenter;
				return CapsuleCenter + (History->GetInterpolatedLocation(RewindQueryTime) - RawLoc);
			};

			// 피격 시 공통 후처리: 트레일 해제 · 임팩트 VFX(GameplayCue) · Dead 태그 · 스플래시
			auto FinishHit = [&](FVector HitPoint, FVector ImpactNormal,
				const FCollectedEnemy* ExclEnemy, const FCollectedPlayer* ExclPlayer)
			{
				if (Visuals[i].bInitialized)
					VisualSub.EnqueueTrailRelease(ProjEnt);

				// 캐릭터 피격 임팩트 VFX는 GameplayCue.LNP.Projectile.Impact로 일원화한다 (섹션 5.2).
				// Ghost 재조정에 필요한 토큰(PredictionKeyID/SpawnIndex)과 InstigatorPlayerID를 커스텀 컨텍스트로 전달.
				AActor* VictimActor = ExclEnemy ? ExclEnemy->Actor : (ExclPlayer ? ExclPlayer->Actor : nullptr);
				if (UAbilitySystemComponent* VictimASC = LNPHitDetection::GetASC(VictimActor))
				{
					FLNPProjectileImpactContext* ImpactCtx = new FLNPProjectileImpactContext();
					ImpactCtx->PredictionKeyID    = Proj.PredictionKeyID;
					ImpactCtx->SpawnIndex         = Proj.SpawnIndex;
					ImpactCtx->InstigatorPlayerID = Proj.InstigatorPlayerID;
					ImpactCtx->VFXData            = Shared.VFXData;

					FGameplayCueParameters CueParams;
					CueParams.Location      = HitPoint;
					CueParams.Normal        = ImpactNormal;
					CueParams.EffectContext = FGameplayEffectContextHandle(ImpactCtx);
					VictimASC->ExecuteGameplayCue(TAG_GameplayCue_Projectile_Impact, CueParams);
				}

				Ctx.Defer().AddTag<FLNPProjectileDeadTag>(ProjEnt);
				ApplySplash(Ctx, Shared, Proj, HitPoint, ExclEnemy, ExclPlayer, bFriendlyFire);
			};

			// Enemy 판정 — Player 발사체만 Enemy에게 피해를 줌 (비 Player 발사체는 캡슐에 닿아도 파괴만 됨)
			bool bHit = false;
			for (FCollectedEnemy& Enemy : Enemies)
			{
				if (Enemy.Handle == Proj.Instigator)
					continue;

				FVector HitPoint;
				if (!SegmentHitsCapsule(
					Proj.PreviousPos, CurrentPos,
					RewoundCenter(Enemy.CapsuleCenter, Enemy.RawLocation, Enemy.History), Enemy.UpDir,
					Enemy.CapsuleHalfHeight, Enemy.CapsuleRadius + HitRadius,
					HitPoint))
					continue;

				if (Proj.InstigatorTeam == ELNPInstigatorTeam::Player)
				{
					if (Enemy.Actor && Shared.DamageEffectClass)
					{
						const FVector HitFromDir = (-Proj.Velocity).GetSafeNormal();
						Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Enemy.Actor, Proj.Instigator, Shared.DamageEffectClass, Shared.Damage, HitFromDir, Shared.KnockbackStrength);
					}
					else
					{
						const float HpBefore = Enemy.Fragment->Health;
						Enemy.Fragment->Health = FMath::Max(0.f, HpBefore - LNPDamage::ApplyDefense(Shared.Damage, Enemy.Fragment->Defense));
						UE_LOG(LogLootNPop, Log, TEXT("[HitDetection][Entity] HP: %.1f -> %.1f (damage=%.1f)"), HpBefore, Enemy.Fragment->Health, Shared.Damage);
					}
				}

				FinishHit(HitPoint, (HitPoint - Enemy.CapsuleCenter).GetSafeNormal(), &Enemy, nullptr);
				bHit = true;
				break;
			}

			if (bHit)
				continue;

			// Player 판정 (패링/가드/피격 분기)
			for (FCollectedPlayer& Player : Players)
			{
				if (Player.Handle == Proj.Instigator)
					continue;

				const FLNPParryStateFragment& PS          = Player.ParryState;
				const FVector                 IncomingDir = (Proj.PreviousPos - CurrentPos).GetSafeNormal();
				const float                   Dot         = FVector::DotProduct(Player.ForwardVector, IncomingDir);
				const bool bShouldProcess = Proj.InstigatorTeam == ELNPInstigatorTeam::Enemy || bFriendlyFire;

				// 1단계: 패링 체크 (ParryRadius — 피격보다 큰 반경)
				if (bShouldProcess && PS.bIsParrying && (PS.ParryWindowExpiryTime < 0.0 || Now <= PS.ParryWindowExpiryTime) && Dot >= PS.ParryAngleCos)
				{
					FVector HitPoint;
					if (SegmentHitsCapsule(
						Proj.PreviousPos, CurrentPos,
						RewoundCenter(Player.Location, Player.RawLocation, Player.History), Player.UpDir,
						Player.CapsuleHalfHeight, Player.CapsuleRadius + ParryRadius,
						HitPoint))
					{
						// 투사체 반사: 속도 반전 + 진영 전환 + 식별자 재발급 (섹션 5.2 반사 개정 — 소멸+재스폰 방송).
						// 이후 임팩트 큐·Ghost 대조는 전부 새 식별자 기준이 된다.
						const int32 OldInstigatorPlayerID = Proj.InstigatorPlayerID;
						const int32 OldKeyOrSalvo         = Proj.PredictionKeyID;
						const uint8 OldSpawnIndex         = Proj.SpawnIndex;

						// 반사 주체(방어자)를 새 공격자로 귀속 — Lag Compensation 기준도 방어자 RTT/2로 갱신.
						int32 DefenderPlayerID  = INDEX_NONE;
						float DefenderHalfRTT   = 0.f;
						if (const APawn* DefenderPawn = Cast<APawn>(Player.Actor))
						{
							if (const APlayerState* DefenderPS = DefenderPawn->GetPlayerState())
							{
								DefenderPlayerID = DefenderPS->GetPlayerId();
								DefenderHalfRTT  = FMath::Clamp(DefenderPS->GetPingInMilliseconds() * 0.0005f, 0.f, MaxRewindSeconds);
							}
						}

						Proj.Velocity            = -Proj.Velocity;
						Proj.InstigatorTeam      = ELNPInstigatorTeam::Player;
						Proj.Instigator          = Player.Handle;
						Proj.InstigatorPlayerID  = DefenderPlayerID;
						Proj.PredictionKeyID     = ULNPGhostProjectileSubsystem::IssueServerSalvoID();
						Proj.SpawnIndex          = 0;
						Proj.CachedRewindSeconds = DefenderHalfRTT;

						FLNPProjectileParryCommand::FEntry ParryEntry;
						ParryEntry.Victim                = Player.Actor;
						ParryEntry.SharedData            = Shared;
						ParryEntry.SpawnPos              = CurrentPos;
						ParryEntry.NewVelocity           = Proj.Velocity;
						ParryEntry.LifetimeRemaining     = Proj.LifetimeRemaining;
						ParryEntry.NewTeam               = Proj.InstigatorTeam;
						ParryEntry.OldInstigatorPlayerID = OldInstigatorPlayerID;
						ParryEntry.OldKeyOrSalvo         = OldKeyOrSalvo;
						ParryEntry.OldSpawnIndex         = OldSpawnIndex;
						ParryEntry.NewInstigatorPlayerID = DefenderPlayerID;
						ParryEntry.NewKeyOrSalvo         = Proj.PredictionKeyID;
						Ctx.Defer().PushCommand<FLNPProjectileParryCommand>(ParryEntry);
						break;  // 반사된 투사체는 파괴하지 않고 계속 비행 (FinishHit 호출 없음)
					}
				}

				// 2단계: 피격 체크 (HitRadius — 정상 반경)
				FVector HitPoint;
				if (!SegmentHitsCapsule(
					Proj.PreviousPos, CurrentPos,
					RewoundCenter(Player.Location, Player.RawLocation, Player.History), Player.UpDir,
					Player.CapsuleHalfHeight, Player.CapsuleRadius + HitRadius,
					HitPoint))
					continue;

				if (bShouldProcess)
				{
					if (PS.bIsGuarding && Dot >= PS.GuardAngleCos)
						Ctx.Defer().PushCommand<FLNPGuardBlockCommand>(Player.Actor);
					else if (Shared.DamageEffectClass)
					{
						const FVector HitFromDir = (-Proj.Velocity).GetSafeNormal();
						Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Player.Actor, Proj.Instigator, Shared.DamageEffectClass, Shared.Damage, HitFromDir, Shared.KnockbackStrength);
					}
				}

				FinishHit(HitPoint, (HitPoint - Player.Location).GetSafeNormal(), nullptr, &Player);
				break;
			}
		}
	});
}

// ============================================================
// ULNPProjectileVisualizationProcessor
// ============================================================

ULNPProjectileVisualizationProcessor::ULNPProjectileVisualizationProcessor()
	: ProjectileQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::StartPhysics;

	ExecutionOrder.ExecuteAfter.Add(ULNPProjectileHitDetectionProcessor::StaticClass()->GetFName());
}

void ULNPProjectileVisualizationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProjectileQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddRequirement<FLNPProjectileFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddRequirement<FLNPProjectileVisualFragment>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.AddConstSharedRequirement<FLNPProjectileSharedFragment>(EMassFragmentPresence::All);
	ProjectileQuery.RegisterWithProcessor(*this);
	ProcessorRequirements.AddSubsystemRequirement<ULNPProjectileVisualSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPProjectileVisualizationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ULNPProjectileVisualSubsystem& VisualSub = Context.GetMutableSubsystemChecked<ULNPProjectileVisualSubsystem>();

	// Movement/HitDetection Processor가 큐에 넣은 작업 처리 (둘 다 Worker Thread에서 실행 가능)
	VisualSub.FlushTrailReleases();
	VisualSub.FlushPendingImpacts();

	ProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPProjectileSharedFragment&           Shared      = Ctx.GetConstSharedFragment<FLNPProjectileSharedFragment>();
		const TConstArrayView<FTransformFragment>     Transforms  = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPProjectileFragment> Projectiles = Ctx.GetFragmentView<FLNPProjectileFragment>();
		TArrayView<FLNPProjectileVisualFragment>      Visuals     = Ctx.GetMutableFragmentView<FLNPProjectileVisualFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector                 CurrentPos = Transforms[i].GetTransform().GetLocation();
			FLNPProjectileVisualFragment& Visual     = Visuals[i];
			const FMassEntityHandle       Entity     = Ctx.GetEntity(i);

			const ELNPInstigatorTeam Team = Projectiles[i].InstigatorTeam;

			if (false == Visual.bInitialized)
			{
				VisualSub.SpawnSpawnEffects(Shared.VFXData, Projectiles[i].SpawnLocation);
				VisualSub.AllocateTrails(Entity, Shared.VFXData, Projectiles[i].SpawnLocation, Team);
				Visual.AppliedTeam  = Team;
				Visual.bInitialized = true;
			}
			else
			{
				VisualSub.UpdateTrails(Entity, CurrentPos);

				// 패링으로 소유권이 넘어간 프레임에만 색을 다시 주입한다 — 매 프레임 Niagara 파라미터를
				// 건드리면 발사체가 많을 때(샷건 19발) 헛비용이 된다.
				if (Visual.AppliedTeam != Team)
				{
					VisualSub.SetTrailTeam(Entity, Team);
					Visual.AppliedTeam = Team;
				}
			}
		}
	});
}

// ============================================================
// ULNPProjectileDestructionProcessor
// ============================================================

ULNPProjectileDestructionProcessor::ULNPProjectileDestructionProcessor()
	: DeadProjectileQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

void ULNPProjectileDestructionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	DeadProjectileQuery.AddRequirement<FLNPProjectileFragment>(EMassFragmentAccess::ReadOnly);
	DeadProjectileQuery.AddTagRequirement<FLNPProjectileDeadTag>(EMassFragmentPresence::All);
	DeadProjectileQuery.RegisterWithProcessor(*this);
}

void ULNPProjectileDestructionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> ToDestroy;
	DeadProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			ToDestroy.Add(Ctx.GetEntity(i));
	});
	if (ToDestroy.Num() > 0)
		Context.Defer().DestroyEntities(MoveTemp(ToDestroy));
}

#if WITH_EDITOR
// ============================================================
// ULNPProjectileDebugDrawProcessor
// ============================================================

ULNPProjectileDebugDrawProcessor::ULNPProjectileDebugDrawProcessor()
	: ProjectileQuery(*this), PlayerQuery(*this), EnemyQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::StartPhysics;
	ExecutionOrder.ExecuteAfter.Add(ULNPProjectileVisualizationProcessor::StaticClass()->GetFName());
}

void ULNPProjectileDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProjectileQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddRequirement<FLNPProjectileFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddConstSharedRequirement<FLNPProjectileSharedFragment>(EMassFragmentPresence::All);
	ProjectileQuery.AddTagRequirement<FLNPProjectileDeadTag>(EMassFragmentPresence::None);
	ProjectileQuery.RegisterWithProcessor(*this);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerQuery.RegisterWithProcessor(*this);

	EnemyQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.RegisterWithProcessor(*this);
}

void ULNPProjectileDebugDrawProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
		return;

	auto Batcher = UE::Mass::Debug::FLineBatcher::MakeLineBatcher(World);

	// Player 위치 수집 + Player 캡슐 드로우 (항상 표시)
	TArray<FVector> PlayerLocations;
	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector Location  = Transforms[i].GetTransform().GetLocation();
			const FVector UpDir     = (-Location).GetSafeNormal();
			const FVector TopSphere = Location + UpDir * 54.f;  // HalfHeight(96) - Radius(42)
			const FVector BotSphere = Location - UpDir * 54.f;
			Batcher.DrawSphere(TopSphere, 42.f, FLinearColor(FColor::Green));
			Batcher.DrawSphere(BotSphere, 42.f, FLinearColor(FColor::Green));
			PlayerLocations.Add(Location);
		}
	});

	const float ProjectileProximityDistSq = GetDefault<ULNPSettings>()->DebugDrawProjectileDistSq;
	const float MeleeProximityDistSq      = GetDefault<ULNPSettings>()->DebugDrawProximityDistSq;

	auto IsNearAnyPlayer = [&](const FVector& Pos, const float& ProximityDistSq) -> bool
	{
		for (const FVector& PL : PlayerLocations)
		{
			if (FVector::DistSquared(Pos, PL) < ProximityDistSq)
				return true;
		}
		return false;
	};

	ProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment>     Transforms  = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPProjectileFragment> Projectiles = Ctx.GetFragmentView<FLNPProjectileFragment>();
		const FLNPProjectileSharedFragment&           Shared      = Ctx.GetConstSharedFragment<FLNPProjectileSharedFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector Pos = Transforms[i].GetTransform().GetLocation();
			if (!IsNearAnyPlayer(Pos, ProjectileProximityDistSq))
				continue;

			const FVector VelDir     = Projectiles[i].Velocity.GetSafeNormal();
			const bool    bPlayer    = Projectiles[i].InstigatorTeam == ELNPInstigatorTeam::Player;
			const FColor  Color      = bPlayer ? FColor::Cyan   : FColor::Red;
			const FColor  ParryColor = bPlayer ? FColor::Silver : FColor::Orange;

			//Batcher.DrawSphere(Pos, Shared.HitRadius,   FLinearColor(Color));
			//Batcher.DrawSphere(Pos, Shared.ParryRadius, FLinearColor(ParryColor));
			if (!VelDir.IsNearlyZero())
			{
				const FTransform ArrowTf(FQuat::FindBetweenNormals(FVector::ForwardVector, VelDir), Pos);
				//Batcher.DrawArrow(ArrowTf, 30.f, FColor::White);
			}
		}
	});

	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FMassActorFragment> ActorFrags = Ctx.GetFragmentView<FMassActorFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const FLNPEnemySharedFragment&            Shared     = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();

		if (!Shared.Config)
			return;

		const float HalfHeight = Shared.Config->CapsuleHalfHeight;
		const float Radius     = Shared.Config->CapsuleRadius;
		const float CylHalfLen = FMath::Max(0.f, HalfHeight - Radius);

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector Location = Transforms[i].GetTransform().GetLocation();
			if (!IsNearAnyPlayer(Location, MeleeProximityDistSq))
				continue;

			const FVector UpDir = (-Location).GetSafeNormal();
			FVector Center;
			if (const ALNPCharacterBase* Enemy = Cast<ALNPCharacterBase>(ActorFrags[i].Get()))
				Center = Location;
			else
				Center = Location + UpDir * HalfHeight;

			const FVector TopSphere = Center + UpDir * CylHalfLen;
			const FVector BotSphere = Center - UpDir * CylHalfLen;
			Batcher.DrawSphere(TopSphere, Radius, FLinearColor(FColor::Red));
			Batcher.DrawSphere(BotSphere, Radius, FLinearColor(FColor::Red));
		}
	});
}
#else
ULNPProjectileDebugDrawProcessor::ULNPProjectileDebugDrawProcessor()
	: ProjectileQuery(*this), PlayerQuery(*this), EnemyQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;
}
void ULNPProjectileDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>&) {}
void ULNPProjectileDebugDrawProcessor::Execute(FMassEntityManager&, FMassExecutionContext&) {}
#endif
