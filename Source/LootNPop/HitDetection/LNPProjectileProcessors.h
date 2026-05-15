// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "LNPProjectileProcessors.generated.h"

/**
 * Advances projectile positions, handles lifetime expiry and terrain collision.
 * Enqueues trail releases and impact effects; defers entity destruction.
 */
UCLASS()
class LOOTNPOP_API ULNPProjectileMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPProjectileMovementProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ProjectileQuery;
};

/**
 * Checks projectile line segments against enemy and player capsules each frame.
 * On hit: applies damage, enqueues trail release and impact effect, defers destruction.
 * Runs after MovementProcessor.
 */
UCLASS()
class LOOTNPOP_API ULNPProjectileHitDetectionProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPProjectileHitDetectionProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ProjectileQuery;
	FMassEntityQuery EnemyQuery;
	FMassEntityQuery PlayerQuery;
};

/**
 * Game-thread processor: flushes queued trail releases and impact effects,
 * then spawns/updates Niagara trail components for live projectiles.
 * Runs after HitDetectionProcessor.
 */
UCLASS()
class LOOTNPOP_API ULNPProjectileVisualizationProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPProjectileVisualizationProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ProjectileQuery;
};

/**
 * Queries all FLNPProjectileDeadTag entities and defers their destruction.
 * Runs in PostPhysics, after the StartPhysics command buffer flush has applied Dead tags
 * from both MovementProcessor and HitDetectionProcessor.
 */
UCLASS()
class LOOTNPOP_API ULNPProjectileDestructionProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPProjectileDestructionProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery DeadProjectileQuery;
};

#if WITH_EDITOR
/**
 * Per-frame debug draw for live projectiles (cyan sphere + velocity arrow).
 * Also flushes surface impact markers (orange sphere, 1-second duration).
 * Runs after VisualizationProcessor on the game thread.
 */
UCLASS()
class LOOTNPOP_API ULNPProjectileDebugDrawProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULNPProjectileDebugDrawProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery ProjectileQuery;
	FMassEntityQuery PlayerQuery;
};
#endif
