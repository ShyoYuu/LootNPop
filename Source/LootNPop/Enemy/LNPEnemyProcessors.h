// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPEnemyProcessors.generated.h"

/**
 * Calculates priority scores for enemies and registers them to the Targeting Subsystem.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyScoringProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyScoringProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery ScoringQuery;
	FMassEntityQuery PlayerQuery;
};

/**
 * Synchronizes the Targeting Subsystem's results back to the Enemy Fragments.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyTargetingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyTargetingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery TargetingQuery;
	FMassEntityQuery PlayerQuery;
};

/**
 * Handles movement intent by syncing Target data to MoveTarget fragment.
 * Also handles distance-based StateTree signaling.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyTargetFollowProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyTargetFollowProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery FollowQuery;
};

/**
 * Pure movement execution processor. 
 * Reads MoveTarget intent and applies actual movement/rotation.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyMovementProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery MovementQuery;
};

#if WITH_EDITOR
/**
 * Visual debugging for Enemy NPCs.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyDebugDrawProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyDebugDrawProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery DebugQuery;
};
#endif

/**
 * Handles health updates and death for enemies.
 */
UCLASS()
class LOOTNPOP_API ULNPHealthProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPHealthProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery HealthQuery;
};

/**
 * Overrides the representation state (Actor vs. Entity) based on custom logic (Targeting, StateTree).
 * Runs before the built-in MassRepresentationProcessor.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyLODOverrideProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyLODOverrideProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery LODOverrideQuery;
};

/**
 * Handles initialization of spawned Actors and data sync from fragments.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyActorInitializerProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyActorInitializerProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery InitializerQuery;
};

/**
 * Runs after the Representation group on the game thread.
 * - Actor valid:  syncs ASC health back to FLNPEnemyFragment (SyncToEntity).
 * - Actor null:   removes FLNPEnemyActorInitializedTag so ActorInitializer re-runs on the next spawn.
 *
 * Trade-off vs two separate processors: if an entity takes GE damage and gets LOD-despawned in the
 * same frame, that frame's damage won't be flushed to the fragment. Acceptable because the two
 * events (fatal hit + LOD boundary crossing) are extremely unlikely to coincide.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyActorSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyActorSyncProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery SyncQuery;
};

/**
 * Ticks DeathCountdown for dying enemies and destroys entities when the timer reaches zero.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyDeathTimerProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEnemyDeathTimerProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery DeathTimerQuery;
};
