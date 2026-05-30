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

	// 1. 모든 어그로 목록을 점수 내림차순으로 정렬
	PendingEntries.Sort();

	// 2. 현재 할당 초기화 및 재할당 준비
	TSet<FMassEntityHandle> AssignedEnemies;
	for (auto& It : PlayerSlots)
	{
		It.Value.OccupiedMelee.Empty();
		It.Value.OccupiedRanged.Empty();
	}

	// 3. 전역 점수 기반 그리디 할당
	for (const FLNPPendingTargetEntry& Entry : PendingEntries)
	{
		// 이미 Player에 할당된 Enemy은 건너뜀
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

	// 4. 다음 프레임을 위해 초기화
	PendingEntries.Reset();
}
