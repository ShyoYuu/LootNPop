// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPTargetingSubsystem.h"

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
	// 슬롯 집합만 비우지 않고 맵 전체를 비운다 — 어차피 아래 FindOrAdd로 매 프레임 재구성되고,
	// 키만 남겨 두면 리스폰 때마다 죽은 플레이어 핸들이 영구히 쌓인다 (엔티티 파괴 훅이 없다).
	TSet<FMassEntityHandle> AssignedEnemies;
	PlayerSlots.Reset();

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
