// Copyright (c) 2026 LootNPop. All rights reserved.

#include "DataAsset/LNPMassSpawnConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "LNPMassSpawnConfig"

EDataValidationResult ULNPMassSpawnConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TSet<FName> SpawnSetIds;
	for (int32 Index = 0; Index < LootPodSpawnSets.Num(); ++Index)
	{
		const FLNPLootPodSpawnEntry& Entry = LootPodSpawnSets[Index];
		if (Entry.SpawnSetId.IsNone())
		{
			Context.AddError(FText::Format(
				LOCTEXT("EmptySpawnSetId", "LootPodSpawnSets[{0}] has an empty SpawnSetId."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
		else if (SpawnSetIds.Contains(Entry.SpawnSetId))
		{
			Context.AddError(FText::Format(
				LOCTEXT("DuplicateSpawnSetId", "LootPodSpawnSets[{0}] duplicates SpawnSetId '{1}'."),
				FText::AsNumber(Index), FText::FromName(Entry.SpawnSetId)));
			Result = EDataValidationResult::Invalid;
		}
		SpawnSetIds.Add(Entry.SpawnSetId);

		if (!Entry.LootPodEntityConfig)
		{
			Context.AddError(FText::Format(
				LOCTEXT("MissingPodConfig", "LootPodSpawnSets[{0}] has no LootPodEntityConfig."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
		if (Entry.PodSetCount < 0)
		{
			Context.AddError(FText::Format(
				LOCTEXT("NegativePodCount", "LootPodSpawnSets[{0}] has a negative PodSetCount."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}

#undef LOCTEXT_NAMESPACE
#endif
