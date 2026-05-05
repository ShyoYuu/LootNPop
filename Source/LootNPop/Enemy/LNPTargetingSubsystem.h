// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityHandle.h"
#include "LNPTargetingSubsystem.generated.h"

/** Information about an enemy competing for a slot */
struct FLNPEnemyScoreEntry
{
	FMassEntityHandle EnemyHandle;
	float Score;
	bool bIsMelee;

	bool operator<(const FLNPEnemyScoreEntry& Other) const
	{
		return Score > Other.Score; // Sort descending
	}
};

/** Slot status for a single player */
USTRUCT()
struct FLNPPlayerSlotData
{
	GENERATED_BODY()

	/** Maximum allowed slots */
	int32 MaxMeleeSlots = 3;
	int32 MaxRangedSlots = 20;

	/** Entities currently occupying slots (Confirmed state) */
	TSet<FMassEntityHandle> OccupiedMelee;
	TSet<FMassEntityHandle> OccupiedRanged;

	/** Entities wanting to enter (scored but not yet processed) */
	TArray<FLNPEnemyScoreEntry> PendingEnemies;
};

/**
 * Global Subsystem to manage combat density per player.
 * Uses a scoring system to decide which enemies get to attack.
 */
UCLASS()
class LOOTNPOP_API ULNPTargetingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Registers an enemy's interest and score for a specific player target */
	void RegisterEnemyInterest(FMassEntityHandle EnemyHandle, FMassEntityHandle PlayerHandle, float Score, bool bIsMelee);

	/** Returns the confirmed state for an enemy */
	bool IsSlotConfirmed(FMassEntityHandle EnemyHandle, FMassEntityHandle PlayerHandle) const;

	/** Performs rebalancing: Promotes high-score enemies and demotes low-score ones */
	void RebalanceSlots();

protected:
	/** Map of Player Entity -> Their Slot Data */
	TMap<FMassEntityHandle, FLNPPlayerSlotData> PlayerSlots;

	/** Lock for thread safety during parallel Mass processing */
	mutable FCriticalSection DataLock;
};
