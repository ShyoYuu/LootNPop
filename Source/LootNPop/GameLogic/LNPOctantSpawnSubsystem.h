// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "LNPOctantSpawnSubsystem.generated.h"

class ULNPOctantPoolData;
class ALevelInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLNPOnWorldGenerationFinished);

/**
 * Subsystem responsible for generating the spherical world by spawning Octant Level Instances.
 * Waits for all instances to be fully loaded before broadcasting finished event.
 */
UCLASS()
class LOOTNPOP_API ULNPOctantSpawnSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	// End USubsystem

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bIsGenerating; }
	virtual TStatId GetStatId() const override;
	// End FTickableGameObject

	/** Starts the world generation process. */
	UFUNCTION(BlueprintCallable, Category = "LNP|World Generation")
	void StartWorldGeneration();

	/** Event fired when all octants have been spawned and FULLY LOADED. */
	UPROPERTY(BlueprintAssignable, Category = "LNP|World Generation")
	FLNPOnWorldGenerationFinished OnWorldGenerationFinished;

private:
	static const FRotator OctantRotations[8];

	bool bIsGenerating = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ALevelInstance>> SpawnedOctants;
};
