// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "MassEntityHandle.h"
#include "Async/Future.h"
#include "LNPMassSpawnSubsystem.generated.h"

class ULNPMassSpawnConfig;
class UMassEntityConfigAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLNPOnSpawningComplete);

/** Defines the type of entity being requested for spawn */
enum class ELNPSpawnRequestType : uint8
{
	LootPod,
	Enemy
};

/** Shared state between a Pod spawn and its associated enemies */
struct FLNPSpawnLink
{
	FMassEntityHandle PodHandle;
	FVector PodLocation = FVector::ZeroVector;
};

/** Internal struct to track a batch of entities waiting to be spawned */
struct FLNPMassSpawnRequest
{
	TObjectPtr<UMassEntityConfigAsset> ConfigAsset;
	TArray<FTransform> TargetTransforms;
	int32 ProcessedCount = 0;

	/** Type of entities in this request */
	ELNPSpawnRequestType RequestType = ELNPSpawnRequestType::Enemy;

	/** Shared link to pass the Pod handle to enemies */
	TSharedPtr<FLNPSpawnLink> SpawnLink;

	bool IsComplete() const { return ProcessedCount >= TargetTransforms.Num(); }
};

/**
 * Output of the async queue-build task: one entry per spawn request, without UObject refs.
 * UObject refs (ConfigAsset) are resolved on the game thread via CapturedAssets index.
 */
struct FLNPAsyncSpawnEntry
{
	TArray<FTransform> Transforms;
	ELNPSpawnRequestType RequestType = ELNPSpawnRequestType::Enemy;
	TSharedPtr<FLNPSpawnLink> SpawnLink;
	int32 AssetIndex = -1;
};

/**
 * Subsystem responsible for hierarchical spawning of Mass Entities (LootPods and Enemies).
 * Handles frame-budgeting, surface validation, and manual transform application post-spawn.
 * Uses global settings from ULNPSettings.
 */
UCLASS()
class LOOTNPOP_API ULNPMassSpawnSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// End USubsystem

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return SpawnBuildFuture.IsValid() || SpawnQueueHead < SpawnQueue.Num(); }
	virtual TStatId GetStatId() const override;
	// End FTickableGameObject

	/** Entry point to start the spawning process based on a config */
	UFUNCTION(BlueprintCallable, Category = "LNP|Mass Spawning")
	void EnqueueSpawnProject(ULNPMassSpawnConfig* InConfig, const float SphereRadius);

	/** Called by GameMode after surface baking completes to begin entity spawning */
	void BeginSpawning();

	/** Fired when all queued entities have been spawned */
	UPROPERTY(BlueprintAssignable, Category = "LNP|Mass Spawning")
	FLNPOnSpawningComplete OnSpawningComplete;

protected:

	/** 
	 * Initializes spawned entities with transforms and optional metadata (leash, etc.).
	 */
	void SetupSpawnedEntities(TConstArrayView<FMassEntityHandle> Entities, TConstArrayView<FTransform> Transforms, FMassEntityHandle ParentLootPod = FMassEntityHandle(), const FVector& ParentPodLocation = FVector::ZeroVector);

private:
	/** Processes a chunk of the spawn queue */
	void ProcessQueue();

	/** Assembles SpawnQueue from the completed async build result. Called on game thread. */
	void AssembleSpawnQueueFromAsyncResult();

	/** Current active configuration being processed */
	UPROPERTY(Transient)
	TObjectPtr<ULNPMassSpawnConfig> ActiveConfig;

	/** Pre-captured UObject refs; kept alive (GC-safe) while async task runs */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMassEntityConfigAsset>> CapturedAssets;

	TArray<FLNPMassSpawnRequest> SpawnQueue;
	int32 SpawnQueueHead = 0;

	/** Result written by the async build task; read on game thread after IsReady() */
	TArray<FLNPAsyncSpawnEntry> AsyncBuildResult;

	/** Future for the background queue-build task */
	TFuture<void> SpawnBuildFuture;

	FRandomStream RandomStream;
};
