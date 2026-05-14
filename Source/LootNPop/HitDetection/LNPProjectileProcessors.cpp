// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPProjectileProcessors.h"
#include "HitDetection/LNPProjectileMassTypes.h"
#include "HitDetection/LNPProjectileVisualSubsystem.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyCharacter.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GAS/Effects/LNPGameplayEffect_Damage.h"
#include "LootNPop.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassCommandBuffer.h"
#include "AbilitySystemComponent.h"
#if WITH_EDITOR
#include "DrawDebugHelpers.h"
#endif

/** Batched command: applies a GE-based damage spec to an ASC. Runs on the game thread after the processing phase. */
struct FLNPApplyDamageGECommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		TSubclassOf<UGameplayEffect>            EffectClass;
		float                                   Damage;
	};

	FLNPApplyDamageGECommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(UAbilitySystemComponent* InASC, TSubclassOf<UGameplayEffect> InEffectClass, float InDamage)
	{
		Entries.Add({ InASC, InEffectClass, InDamage });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		for (const FEntry& Entry : Entries)
		{
			UAbilitySystemComponent* ASC = Entry.ASC.Get();
			if (!IsValid(ASC) || !Entry.EffectClass)
				continue;

			const float HpBefore = ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetHealthAttribute());

			FGameplayEffectContextHandle Ctx  = ASC->MakeEffectContext();
			FGameplayEffectSpecHandle    Spec = ASC->MakeOutgoingSpec(Entry.EffectClass, 1.0f, Ctx);
			if (!Spec.IsValid())
				continue;

			Spec.Data->SetSetByCallerMagnitude(TAG_GE_Data_Damage, -Entry.Damage);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

			const float HpAfter = ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetHealthAttribute());
			UE_LOG(LogLootNPop, Log, TEXT("[HitDetection][GE] HP: %.1f -> %.1f (damage=%.1f)"), HpBefore, HpAfter, Entry.Damage);
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()    const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};

namespace
{
	/** Returns true if the line segment (A→B) intersects the capsule at Center/UpDir/HalfHeight/Radius. */
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
#if WITH_EDITOR
				if (bHitSurface)
					VisualSub.EnqueueSurfaceImpactDebug(NewPos, FColor::Orange, 10.0f);
#endif
				Ctx.Defer().AddTag<FLNPProjectileDeadTag>(Entity);
			}
		}
	});
}

// ============================================================
// ULNPProjectileHitDetectionProcessor
// ============================================================

ULNPProjectileHitDetectionProcessor::ULNPProjectileHitDetectionProcessor()
	: ProjectileQuery(*this), EnemyQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::StartPhysics;
}

void ULNPProjectileHitDetectionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProjectileQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddRequirement<FLNPProjectileFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddRequirement<FLNPProjectileVisualFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddConstSharedRequirement<FLNPProjectileSharedFragment>(EMassFragmentPresence::All);
	ProjectileQuery.AddTagRequirement<FLNPProjectileDeadTag>(EMassFragmentPresence::None);
	ProjectileQuery.RegisterWithProcessor(*this);

	EnemyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EnemyQuery.AddRequirement<FLNPEnemyFragment>(EMassFragmentAccess::ReadWrite);
	EnemyQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EnemyQuery.AddConstSharedRequirement<FLNPEnemySharedFragment>(EMassFragmentPresence::All);
	EnemyQuery.AddTagRequirement<FLNPEnemyTag>(EMassFragmentPresence::All);
	EnemyQuery.RegisterWithProcessor(*this);
	ProcessorRequirements.AddSubsystemRequirement<ULNPProjectileVisualSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPProjectileHitDetectionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ULNPProjectileVisualSubsystem& VisualSub = Context.GetMutableSubsystemChecked<ULNPProjectileVisualSubsystem>();

	// --- Pass 1: Collect all live enemy positions and capsule dims ---
	struct FCollectedEnemy
	{
		FVector                   Location;
		float                     CapsuleHalfHeight;
		float                     CapsuleRadius;
		FLNPEnemyFragment*        Fragment;
		FMassEntityHandle         Handle;
		UAbilitySystemComponent*  ASC; // non-null only when an actor is active for this entity
	};
	TArray<FCollectedEnemy> Enemies;

	EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPEnemySharedFragment& Shared = Ctx.GetConstSharedFragment<FLNPEnemySharedFragment>();
		if (nullptr == Shared.Config)
			return;

		const float HalfH  = Shared.Config->CapsuleHalfHeight;
		const float Radius = Shared.Config->CapsuleRadius;

		const TConstArrayView<FTransformFragment>  Transforms  = Ctx.GetFragmentView<FTransformFragment>();
		TArrayView<FLNPEnemyFragment>              EnemyFrags  = Ctx.GetMutableFragmentView<FLNPEnemyFragment>();
		const TConstArrayView<FMassActorFragment>  ActorFrags  = Ctx.GetFragmentView<FMassActorFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			UAbilitySystemComponent* EnemyASC = nullptr;
			if (!ActorFrags.IsEmpty())
			{
				if (const ALNPEnemyCharacter* EnemyChar = Cast<ALNPEnemyCharacter>(ActorFrags[i].Get()))
					EnemyASC = EnemyChar->GetAbilitySystemComponent();
			}
			Enemies.Add({ Transforms[i].GetTransform().GetLocation(), HalfH, Radius, &EnemyFrags[i], Ctx.GetEntity(i), EnemyASC });
		}
	});

	if (Enemies.IsEmpty())
		return;

	// --- Pass 2: Segment vs capsule tests ---
	ProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const FLNPProjectileSharedFragment&                 Shared      = Ctx.GetConstSharedFragment<FLNPProjectileSharedFragment>();
		const TConstArrayView<FTransformFragment>           Transforms  = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPProjectileFragment>       Projectiles = Ctx.GetFragmentView<FLNPProjectileFragment>();
		const TConstArrayView<FLNPProjectileVisualFragment> Visuals     = Ctx.GetFragmentView<FLNPProjectileVisualFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FLNPProjectileFragment& Proj       = Projectiles[i];
			const FVector                 CurrentPos = Transforms[i].GetTransform().GetLocation();
			const FMassEntityHandle       ProjEnt    = Ctx.GetEntity(i);

			for (FCollectedEnemy& Enemy : Enemies)
			{
				if (Enemy.Handle == Proj.Instigator)
					continue;

				// Inner sphere world: capsule up-axis points toward the sphere center
				const FVector UpDir          = (-Enemy.Location).GetSafeNormal();
				const float   CombinedRadius = Enemy.CapsuleRadius + FMath::Sqrt(Shared.HitRadiusSq);

				FVector HitPoint;
				if (SegmentHitsCapsule(
					Proj.PreviousPos, CurrentPos,
					Enemy.Location, UpDir,
					Enemy.CapsuleHalfHeight, CombinedRadius,
					HitPoint))
				{
					if (Enemy.ASC && Shared.DamageEffectClass)
						Ctx.Defer().PushCommand<FLNPApplyDamageGECommand>(Enemy.ASC, Shared.DamageEffectClass, Shared.Damage);
					else
					{
						const float HpBefore = Enemy.Fragment->Health;
						Enemy.Fragment->Health = FMath::Max(0.f, HpBefore - Shared.Damage);
						UE_LOG(LogLootNPop, Log, TEXT("[HitDetection][Entity] HP: %.1f -> %.1f (damage=%.1f)"), HpBefore, Enemy.Fragment->Health, Shared.Damage);
					}

					if (Visuals[i].bInitialized)
						VisualSub.EnqueueTrailRelease(ProjEnt);

					const FVector ImpactNormal = (HitPoint - Enemy.Location).GetSafeNormal();
					VisualSub.EnqueueImpact(Shared.VFXData, HitPoint, ImpactNormal);

					Ctx.Defer().AddTag<FLNPProjectileDeadTag>(ProjEnt);
					break;
				}
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

	// Flush work queued by Movement and HitDetection processors (both can run on worker threads)
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
			const FVector         CurrentPos = Transforms[i].GetTransform().GetLocation();
			FLNPProjectileVisualFragment& Visual = Visuals[i];
			const FMassEntityHandle       Entity = Ctx.GetEntity(i);

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
	: ProjectileQuery(*this)
{
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
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
	ProcessorRequirements.AddSubsystemRequirement<ULNPProjectileVisualSubsystem>(EMassFragmentAccess::ReadWrite);
}

void ULNPProjectileDebugDrawProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (nullptr == World)
		return;

	Context.GetMutableSubsystemChecked<ULNPProjectileVisualSubsystem>().FlushSurfaceImpactDebug(World);

	ProjectileQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment>     Transforms  = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FLNPProjectileFragment> Projectiles = Ctx.GetFragmentView<FLNPProjectileFragment>();

		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			const FVector Pos    = Transforms[i].GetTransform().GetLocation();
			const FVector VelDir = Projectiles[i].Velocity.GetSafeNormal();

			DrawDebugSphere(World, Pos, 10.f, 8, FColor::Cyan, false, -1.f);
			DrawDebugLine(World, Pos, Pos + VelDir * 60.f, FColor::White, false, -1.f);
		}
	});
}
#endif
