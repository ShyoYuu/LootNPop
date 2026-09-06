// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPTargetQuerySubsystem.h"

FLNPTargetQueryHandle ULNPTargetQuerySubsystem::RegisterQuery()
{
	FScopeLock Lock(&SlotsLock);

	FLNPTargetQueryHandle Handle;

	// 반납된 슬롯을 먼저 재사용한다 — 폰이 리스폰할 때마다 배열이 자라는 것을 막는다.
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (!Slots[i].bInUse)
		{
			Slots[i] = FSlot();
			Slots[i].bInUse = true;
			Handle.SlotIndex = i;
			return Handle;
		}
	}

	Handle.SlotIndex = Slots.AddDefaulted();
	Slots[Handle.SlotIndex].bInUse = true;
	return Handle;
}

void ULNPTargetQuerySubsystem::UnregisterQuery(FLNPTargetQueryHandle& InOutHandle)
{
	if (!InOutHandle.IsValid())
		return;

	{
		FScopeLock Lock(&SlotsLock);
		if (Slots.IsValidIndex(InOutHandle.SlotIndex))
			Slots[InOutHandle.SlotIndex] = FSlot();
	}

	InOutHandle.SlotIndex = INDEX_NONE;
}

void ULNPTargetQuerySubsystem::SetRayQuery(const FLNPTargetQueryHandle& Handle, const FVector& Origin, const FVector& Direction, float MaxDistance)
{
	if (!Handle.IsValid())
		return;

	FScopeLock Lock(&SlotsLock);
	if (!Slots.IsValidIndex(Handle.SlotIndex) || !Slots[Handle.SlotIndex].bInUse)
		return;

	FLNPTargetQueryParams& Params = Slots[Handle.SlotIndex].Params;
	Params.Kind        = ELNPTargetQueryKind::Ray;
	Params.Origin      = Origin;
	Params.Direction   = Direction;
	Params.MaxDistance = MaxDistance;
}

void ULNPTargetQuerySubsystem::SetConeQuery(const FLNPTargetQueryHandle& Handle, const FVector& Origin, const FVector& Direction,
	const FVector& UpDir, float Radius, float MaxAngleDeg, float AngleWeight, float DistanceWeight)
{
	if (!Handle.IsValid())
		return;

	FScopeLock Lock(&SlotsLock);
	if (!Slots.IsValidIndex(Handle.SlotIndex) || !Slots[Handle.SlotIndex].bInUse)
		return;

	FLNPTargetQueryParams& Params = Slots[Handle.SlotIndex].Params;
	Params.Kind           = ELNPTargetQueryKind::Cone;
	Params.Origin         = Origin;
	Params.Direction      = Direction;
	Params.UpDir          = UpDir;
	Params.MaxDistance    = Radius;
	Params.MaxAngleDeg    = MaxAngleDeg;
	Params.AngleWeight    = AngleWeight;
	Params.DistanceWeight = DistanceWeight;
}

bool ULNPTargetQuerySubsystem::GetResult(const FLNPTargetQueryHandle& Handle, FLNPTargetQueryResult& OutResult) const
{
	if (!Handle.IsValid())
		return false;

	FScopeLock Lock(&SlotsLock);
	if (!Slots.IsValidIndex(Handle.SlotIndex) || !Slots[Handle.SlotIndex].bInUse)
		return false;

	OutResult = Slots[Handle.SlotIndex].Result;
	return true;
}

void ULNPTargetQuerySubsystem::SnapshotQueries(TArray<int32>& OutSlotIndices, TArray<FLNPTargetQueryParams>& OutParams) const
{
	OutSlotIndices.Reset();
	OutParams.Reset();

	FScopeLock Lock(&SlotsLock);
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (!Slots[i].bInUse || Slots[i].Params.MaxDistance <= 0.f)
			continue;

		OutSlotIndices.Add(i);
		OutParams.Add(Slots[i].Params);
	}
}

void ULNPTargetQuerySubsystem::SubmitResults(const TArray<int32>& SlotIndices, const TArray<FLNPTargetQueryResult>& Results)
{
	check(SlotIndices.Num() == Results.Num());

	FScopeLock Lock(&SlotsLock);
	for (int32 i = 0; i < SlotIndices.Num(); ++i)
	{
		// 스냅샷과 제출 사이에 소비처가 슬롯을 반납했을 수 있다 — 그 경우 결과를 버린다.
		if (Slots.IsValidIndex(SlotIndices[i]) && Slots[SlotIndices[i]].bInUse)
			Slots[SlotIndices[i]].Result = Results[i];
	}
}
