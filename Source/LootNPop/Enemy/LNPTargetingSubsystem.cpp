// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/LNPTargetingSubsystem.h"

void ULNPTargetingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void ULNPTargetingSubsystem::RegisterEnemyInterest(FMassEntityHandle EnemyHandle, FMassEntityHandle PlayerHandle, float Score, bool bIsMelee)
{
	FScopeLock Lock(&DataLock);

	FLNPPlayerSlotData& SlotData = PlayerSlots.FindOrAdd(PlayerHandle);
	SlotData.PendingEnemies.Add({ EnemyHandle, Score, bIsMelee });
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

	for (auto& It : PlayerSlots)
	{
		FLNPPlayerSlotData& SlotData = It.Value;

		// 1. Separate Melee and Ranged candidates from Pending
		TArray<FLNPEnemyScoreEntry> MeleeCandidates;
		TArray<FLNPEnemyScoreEntry> RangedCandidates;

		for (const auto& Entry : SlotData.PendingEnemies)
		{
			if (Entry.bIsMelee)
				MeleeCandidates.Add(Entry);
			else
				RangedCandidates.Add(Entry);
		}

		// 2. Sort by Score (Descending)
		MeleeCandidates.Sort();
		RangedCandidates.Sort();

		// 3. Update Occupied Sets based on Max Slots
		SlotData.OccupiedMelee.Empty();
		for (int32 i = 0; i < FMath::Min(MeleeCandidates.Num(), SlotData.MaxMeleeSlots); ++i)
		{
			SlotData.OccupiedMelee.Add(MeleeCandidates[i].EnemyHandle);
		}

		SlotData.OccupiedRanged.Empty();
		for (int32 i = 0; i < FMath::Min(RangedCandidates.Num(), SlotData.MaxRangedSlots); ++i)
		{
			SlotData.OccupiedRanged.Add(RangedCandidates[i].EnemyHandle);
		}

		// 4. Clear Pending for next frame
		SlotData.PendingEnemies.Reset();
	}
}
