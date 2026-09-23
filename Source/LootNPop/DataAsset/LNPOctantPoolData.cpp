// Copyright (c) 2026 LootNPop. All rights reserved.

#include "DataAsset/LNPOctantPoolData.h"

void ULNPOctantPoolData::BuildEffectiveDefinitions(TArray<FLNPOctantDefinition>& OutDefinitions) const
{
	if (!OctantDefinitions.IsEmpty())
	{
		OutDefinitions = OctantDefinitions;
		return;
	}

	OutDefinitions.Reset(OctantPool.Num());
	for (const TSoftObjectPtr<UWorld>& LegacyLevel : OctantPool)
	{
		FLNPOctantDefinition& Definition = OutDefinitions.AddDefaulted_GetRef();
		Definition.LevelAsset = LegacyLevel;
	}
}
