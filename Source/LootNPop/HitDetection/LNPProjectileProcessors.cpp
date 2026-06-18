// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPProjectileProcessors.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "HitDetection/LNPProjectileVisualSubsystem.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "HitDetection/LNPGuardParryTypes.h"
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
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.RegisterWithProcessor(*this);

	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	PlayerQuery.AddRequirement<FLNPParryStateFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddTagRequirement<FLNPPlayerTag>(EMassFragmentPresence::All);
	PlayerQuery.RegisterWithProcessor(*this);

	ProcessorRequirements.AddSubsystemRequirement<ULNPProjectileVisualSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPProjectileHitDetectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ULNPProjectileVisualSubsystem& VisualSub = Context.GetMutableSubsystemChecked<ULNPProjectileVisualSubsystem>();

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
		if (nullptr == Shared.Config)
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
			Enemies.Add({ Loc + UpDir * HalfH, UpDir, HalfH, Radius, &EnemyFrags[i], Ctx.GetEntity(i), ActorFrags[i].GetMutable() });
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
	};
	TArray<FCollectedPlayer> Players;

	PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment>     Transforms = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FMassActorFragment>                ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const TConstArrayView<FLNPParryStateFragment> ParryFrags = Ctx.GetFragmentView<FLNPParryStateFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			AActor* PlayerActor = ActorFrags[i].GetMutable();
			const ALNPPlayerCharacter* PlayerPawn = Cast<ALNPPlayerCharacter>(PlayerActor);
			if (PlayerPawn == nullptr)
				continue;

			const UCapsuleComponent* Capsule = PlayerPawn->GetCapsule();
			const FTransform& T   = Transforms[i].GetTransform();
			const FVector     Loc = T.GetLocation();
			Players.Add({
				Loc,
				(-Loc).GetSafeNormal(),
				T.GetRotation().GetForwardVector(),
				Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 96.f,
				Capsule ? Capsule->GetScaledCapsuleRadius()     : 42.f,
				Ctx.GetEntity(i),
				PlayerActor,
				ParryFrags[i]
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
					SE.Actor, FMassEntityHandle{}, Shared.DamageEffectClass,
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
					SP.Actor, FMassEntityHandle{}, Shared.DamageEffectClass,
					Shared.Damage, (SP.Location - HitPoint).GetSafeNormal(), Shared.SplashKnockbackStrength);
			}
		}
	};

	// ── Pass 3: 선분 vs 캡슐 충돌 판정 ───────────────────────────────────────
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

			// 피격 시 공통 후처리: 트레일 해제 · 임팩트 VFX · Dead 태그 · 스플래시
			auto FinishHit = [&](FVector HitPoint, FVector ImpactNormal,
				const FCollectedEnemy* ExclEnemy, const FCollectedPlayer* ExclPlayer)
			{
				if (Visuals[i].bInitialized)
					VisualSub.EnqueueTrailRelease(ProjEnt);
				VisualSub.EnqueueImpact(Shared.VFXData, HitPoint, ImpactNormal);
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
					Enemy.CapsuleCenter, Enemy.UpDir,
					Enemy.CapsuleHalfHeight, Enemy.CapsuleRadius + HitRadius,
					HitPoint))
					continue;

				if (Proj.InstigatorTeam == ELNPInstigatorTeam::Player)
				{
					if (Enemy.Actor && Shared.DamageEffectClass)
					{
						const FVector HitFromDir = (-Proj.Velocity).GetSafeNormal();
						Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Enemy.Actor, FMassEntityHandle{}, Shared.DamageEffectClass, Shared.Damage, HitFromDir, Shared.KnockbackStrength);
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
				if (bShouldProcess && PS.bIsParrying && Dot >= PS.ParryAngleCos)
				{
					FVector HitPoint;
					if (SegmentHitsCapsule(
						Proj.PreviousPos, CurrentPos,
						Player.Location, Player.UpDir,
						Player.CapsuleHalfHeight, Player.CapsuleRadius + ParryRadius,
						HitPoint))
					{
						// 투사체 반사: 속도 반전 + 진영 전환
						Proj.Velocity       = -Proj.Velocity;
						Proj.InstigatorTeam = ELNPInstigatorTeam::Player;
						Proj.Instigator     = Player.Handle;
						Ctx.Defer().PushCommand<FLNPProjectileParryCommand>(Player.Actor);
						break;  // 반사된 투사체는 파괴하지 않고 계속 비행 (FinishHit 호출 없음)
					}
				}

				// 2단계: 피격 체크 (HitRadius — 정상 반경)
				FVector HitPoint;
				if (!SegmentHitsCapsule(
					Proj.PreviousPos, CurrentPos,
					Player.Location, Player.UpDir,
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
						Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Player.Actor, FMassEntityHandle{}, Shared.DamageEffectClass, Shared.Damage, HitFromDir, Shared.KnockbackStrength);
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

			if (false == Visual.bInitialized)
			{
				VisualSub.SpawnSpawnEffects(Shared.VFXData, Projectiles[i].SpawnLocation);
				VisualSub.AllocateTrails(Entity, Shared.VFXData, CurrentPos);
				Visual.bInitialized = true;
			}
			else
			{
				VisualSub.UpdateTrails(Entity, CurrentPos);
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

			Batcher.DrawSphere(Pos, Shared.HitRadius,   FLinearColor(Color));
			Batcher.DrawSphere(Pos, Shared.ParryRadius, FLinearColor(ParryColor));
			if (!VelDir.IsNearlyZero())
			{
				const FTransform ArrowTf(FQuat::FindBetweenNormals(FVector::ForwardVector, VelDir), Pos);
				Batcher.DrawArrow(ArrowTf, 30.f, FColor::White);
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
