// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyMarkerSubsystem.h"

void ULNPEnemyMarkerSubsystem::SetParams(const FLNPEnemyMarkerParams& InParams)
{
	FScopeLock Lock(&DataLock);
	Params = InParams;
}

void ULNPEnemyMarkerSubsystem::ClearParams()
{
	FScopeLock Lock(&DataLock);
	Params.MaxDistance = 0.f;
	Params.MaxCount    = 0;
	Entries.Reset();
}

void ULNPEnemyMarkerSubsystem::GetEntries(TArray<FLNPEnemyMarkerEntry>& OutEntries) const
{
	FScopeLock Lock(&DataLock);
	OutEntries = Entries;
}

bool ULNPEnemyMarkerSubsystem::SnapshotParams(FLNPEnemyMarkerParams& OutParams) const
{
	FScopeLock Lock(&DataLock);
	if (Params.MaxDistance <= 0.f || Params.MaxCount <= 0)
		return false;

	OutParams = Params;
	return true;
}

void ULNPEnemyMarkerSubsystem::SubmitEntries(TArray<FLNPEnemyMarkerEntry>&& InEntries)
{
	FScopeLock Lock(&DataLock);
	Entries = MoveTemp(InEntries);
}
