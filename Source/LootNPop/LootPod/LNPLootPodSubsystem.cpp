// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LootPod/LNPLootPodSubsystem.h"
#include "LootPod/LNPLootPod.h"

void ULNPLootPodSubsystem::RegisterPod(ALNPLootPod* Pod)
{
	if (Pod != nullptr)
	{
		ActivePods.AddUnique(Pod);
	}
}

void ULNPLootPodSubsystem::UnregisterPod(ALNPLootPod* Pod)
{
	ActivePods.Remove(Pod);

	// 파괴 통지를 놓친 stale 엔트리도 이 기회에 정리
	ActivePods.RemoveAll([](const TWeakObjectPtr<ALNPLootPod>& Entry) { return !Entry.IsValid(); });
}
