// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPTargetingSubsystem.h"

void ULNPTargetingSubsystem::RegisterEnemyInterest(FMassEntityHandle EnemyHandle, FMassEntityHandle PlayerHandle, float Score, ELNPTargetSlotPool Pool)
{
	FScopeLock Lock(&DataLock);
	PendingEntries.Add({ EnemyHandle, PlayerHandle, Score, Pool });
}

bool ULNPTargetingSubsystem::IsSlotConfirmed(FMassEntityHandle EnemyHandle, FMassEntityHandle PlayerHandle) const
{
	FScopeLock Lock(&DataLock);

	const FLNPPlayerSlotData* SlotData = PlayerSlots.Find(PlayerHandle);
	if (nullptr == SlotData)
		return false;

	// 풀을 인자로 받지 않는다 — 호출자(StateTree·프로세서)는 "슬롯을 얻었는가"만 알면 되고,
	// 어느 풀에서 얻었는지는 이 클래스 밖에서 의미가 없다.
	for (const TSet<FMassEntityHandle>& Pool : SlotData->Occupied)
	{
		if (Pool.Contains(EnemyHandle))
			return true;
	}
	return false;
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

		// 풀은 서로 예산을 뺏지 않는다 — 잡몹이 아무리 많아도 승격 개체의 자리는 남는다.
		TSet<FMassEntityHandle>& PoolSlots = SlotData.Occupied[(int32)Entry.Pool];
		if (PoolSlots.Num() < GetMaxSlotsForPool(Entry.Pool))
		{
			PoolSlots.Add(Entry.EnemyHandle);
			AssignedEnemies.Add(Entry.EnemyHandle);
		}
	}

	// 4. 다음 프레임을 위해 초기화
	PendingEntries.Reset();
}
