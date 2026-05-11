// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/LNPTargetingSubsystem.h"

void ULNPTargetingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void ULNPTargetingSubsystem::RegisterEnemyInterest(FMassEntityHandle EnemyHandle, FMassEntityHandle PlayerHandle, float Score, bool bIsMelee)
{
	FScopeLock Lock(&DataLock);
	PendingEntries.Add({ EnemyHandle, PlayerHandle, Score, bIsMelee });
}

bool ULNPTargetingSubsystem::IsSlotConfirmed(FMassEntityHandle EnemyHandle, FMassEntityHandle PlayerHandle) const
{
	FScopeLock Lock(&DataLock);

	const FLNPPlayerSlotData* SlotData = PlayerSlots.Find(PlayerHandle);
	if (nullptr == SlotData)
		return false;

	return SlotData->OccupiedMelee.Contains(EnemyHandle) || SlotData->OccupiedRanged.Contains(EnemyHandle);
}

void ULNPTargetingSubsystem::RebalanceSlots()
{
	FScopeLock Lock(&DataLock);

	// 1. Sort all interests by Score descending
	PendingEntries.Sort();

	// 2. Clear current assignments and prepare to re-assign
	TSet<FMassEntityHandle> AssignedEnemies;
	for (auto& It : PlayerSlots)
	{
		It.Value.OccupiedMelee.Empty();
		It.Value.OccupiedRanged.Empty();
	}

	// 3. Greedy allocation based on global score
	for (const FLNPPendingTargetEntry& Entry : PendingEntries)
	{
		// Skip if this enemy is already assigned to a player
		if (AssignedEnemies.Contains(Entry.EnemyHandle))
		{
			continue;
		}

		FLNPPlayerSlotData& SlotData = PlayerSlots.FindOrAdd(Entry.PlayerHandle);

		if (Entry.bIsMelee)
		{
			if (SlotData.OccupiedMelee.Num() < MaxMeleeSlotsPerPlayer)
			{
				SlotData.OccupiedMelee.Add(Entry.EnemyHandle);
				AssignedEnemies.Add(Entry.EnemyHandle);
			}
		}
		else
		{
			if (SlotData.OccupiedRanged.Num() < MaxRangedSlotsPerPlayer)
			{
				SlotData.OccupiedRanged.Add(Entry.EnemyHandle);
				AssignedEnemies.Add(Entry.EnemyHandle);
			}
		}
	}

	// 4. Clear for next frame
	PendingEntries.Reset();
}
