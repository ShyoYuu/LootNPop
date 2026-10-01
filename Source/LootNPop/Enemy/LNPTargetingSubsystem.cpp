// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPTargetingSubsystem.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "Misc/ScopeExit.h"

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

void ULNPTargetingSubsystem::RebalanceSlots(FMassEntityManager& EntityManager, const FLNPNavSnapshot* Nav, const double Now)
{
	const double Begin = FPlatformTime::Seconds();
	FScopeLock Lock(&DataLock);
	ON_SCOPE_EXIT { LastRebalanceSeconds = FPlatformTime::Seconds() - Begin; };

	// 1. 모든 어그로 목록을 점수 내림차순으로 정렬
	PendingEntries.Sort();

	// 풀과 무관하게 지상 근접만 도달성을 검사한다(승격 근접도 포함). 원거리·비행은 기존 규칙이다.
	PendingEntries.RemoveAll([&](const FLNPPendingTargetEntry& Entry)
	{
		if (!EntityManager.IsEntityActive(Entry.EnemyHandle) || !EntityManager.IsEntityActive(Entry.PlayerHandle))
		{
			return true;
		}
		const FLNPEnemySharedFragment* Shared = EntityManager.GetConstSharedFragmentDataPtr<FLNPEnemySharedFragment>(Entry.EnemyHandle);
		if (!Shared || !Shared->Config || Shared->Config->IsFlying()
			|| Shared->Config->AttackType != ELNPEnemyAttackType::Melee)
		{
			return false;
		}
		const FLNPPlayerSlotData* Previous = PlayerSlots.Find(Entry.PlayerHandle);
		const bool bOccupied = Previous && Previous->Occupied[static_cast<int32>(Entry.Pool)].Contains(Entry.EnemyHandle);
		FLNPEnemyTargetingFragment* Target = EntityManager.GetFragmentDataPtr<FLNPEnemyTargetingFragment>(Entry.EnemyHandle);
		const FLNPEnemyFragment* Enemy = EntityManager.GetFragmentDataPtr<FLNPEnemyFragment>(Entry.EnemyHandle);
		const FLNPPlayerNavFragment* Player = EntityManager.GetFragmentDataPtr<FLNPPlayerNavFragment>(Entry.PlayerHandle);
		const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entry.EnemyHandle);
		if (!Nav || !Target || !Enemy || !Player || !Transform)
		{
			return !bOccupied;
		}
		FLNPNavEndpointQuery Query;
		const FVector3d Center = Transform->GetTransform().GetLocation();
		Query.StartPosition = Center + Center.GetSafeNormal() * Shared->Config->CapsuleHalfHeight;
		Query.StartSurface = &Enemy->SurfaceHandle;
		// D-062는 원래 지면점 주위에서 짝을 고른다. 이미 스냅한 node를 다시 목표로 쓰지 않는다.
		Query.GoalPosition = Player->GroundPoint;
		Query.GoalSurface = &Player->Surface;
		Query.ApproachRadius = 0.0;
		const FLNPNavEndpoints Ends = LNPNavGraph::ResolveEndpoints(*Nav, Query);
		// 다른 후보 검사가 현재 슬롯의 유예 상태를 덮지 않게 한다.
		FLNPEnemySlotReachability State = Target->SlotReachability;
		const bool bAllowed = LNPEnemyNavigation::UpdateMeleeSlot(*Nav, Ends.Status, Entry.PlayerHandle,
			Player->GroundGroup, Player->bGrounded, bOccupied, Now, State);
		if (bOccupied || Target->TargetPlayer == Entry.PlayerHandle)
		{
			Target->SlotReachability = State;
		}
		return !bAllowed;
	});

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
