// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityHandle.h"
#include "LNPTargetingSubsystem.generated.h"

/** Information about an enemy competing for a slot */
struct FLNPPendingTargetEntry
{
	FMassEntityHandle EnemyHandle;
	FMassEntityHandle PlayerHandle;
	float Score;
	bool bIsMelee;

	bool operator<(const FLNPPendingTargetEntry& Other) const
	{
		return Score > Other.Score; // Sort descending
	}
};

/** Slot status for a single player */
USTRUCT()
struct FLNPPlayerSlotData
{
	GENERATED_BODY()

	/** Entities currently occupying slots (Confirmed state) */
	TSet<FMassEntityHandle> OccupiedMelee;
	TSet<FMassEntityHandle> OccupiedRanged;
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

	UPROPERTY(EditDefaultsOnly, Category = "LNP|Targeting")
	int32 MaxMeleeSlotsPerPlayer = 10;

	UPROPERTY(EditDefaultsOnly, Category = "LNP|Targeting")
	int32 MaxRangedSlotsPerPlayer = 20;

protected:
	/** Map of Player Entity -> Their Slot Data */
	TMap<FMassEntityHandle, FLNPPlayerSlotData> PlayerSlots;

	/** All enemy interests registered this frame */
	TArray<FLNPPendingTargetEntry> PendingEntries;

	/** Lock for thread safety during parallel Mass processing */
	mutable FCriticalSection DataLock;
};
