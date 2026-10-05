// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPGameplayTagStack.h"

void FLNPGameplayTagStackContainer::AddStack(FGameplayTag Tag, int32 Delta)
{
	if (!Tag.IsValid() || Delta <= 0)
	{
		return;
	}

	if (FLNPGameplayTagStack* Entry = Find(Tag))
	{
		Entry->Count += Delta;
		MarkItemDirty(*Entry);
	}
	else
	{
		MarkItemDirty(Entries.Emplace_GetRef(Tag, Delta));
	}
}

void FLNPGameplayTagStackContainer::RemoveStack(FGameplayTag Tag, int32 Delta)
{
	if (!Tag.IsValid() || Delta <= 0)
	{
		return;
	}

	const int32 Index = Entries.IndexOfByPredicate([Tag](const FLNPGameplayTagStack& Entry) { return Entry.Tag == Tag; });
	if (Index == INDEX_NONE)
	{
		return;
	}

	FLNPGameplayTagStack& Entry = Entries[Index];
	if (Entry.Count > Delta)
	{
		Entry.Count -= Delta;
		MarkItemDirty(Entry);
	}
	else
	{
		Entries.RemoveAtSwap(Index);
		MarkArrayDirty();
	}
}
