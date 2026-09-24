// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPOctantLevelInstance.h"

bool ALNPOctantLevelInstance::SetRuntimeWorldAsset(const TSoftObjectPtr<UWorld>& InWorldAsset)
{
#if WITH_EDITOR
	return SetWorldAsset(InWorldAsset);
#else
	CookedWorldAsset = InWorldAsset;
	return !CookedWorldAsset.IsNull();
#endif
}
