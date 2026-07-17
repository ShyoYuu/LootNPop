// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPGameplayTagStack.h"

FString FLNPGameplayTagStack::GetDebugString() const
{
	return FString::Printf(TEXT("%sx%d"), *Tag.ToString(), StackCount);
}

void FLNPGameplayTagStackContainer::AddStack(FGameplayTag Tag, int32 StackCount)
{
	if (!Tag.IsValid() || StackCount <= 0)
	{
		return;
	}

	for (FLNPGameplayTagStack& Stack : Stacks)
	{
		if (Stack.Tag == Tag)
		{
			Stack.StackCount += StackCount;
			TagToCountMap[Tag] = Stack.StackCount;
			MarkItemDirty(Stack);
			return;
		}
	}

	FLNPGameplayTagStack& NewStack = Stacks.Emplace_GetRef(Tag, StackCount);
	MarkItemDirty(NewStack);
	TagToCountMap.Add(Tag, StackCount);
}

void FLNPGameplayTagStackContainer::RemoveStack(FGameplayTag Tag, int32 StackCount)
{
	if (!Tag.IsValid() || StackCount <= 0)
	{
		return;
	}

	for (auto It = Stacks.CreateIterator(); It; ++It)
	{
		FLNPGameplayTagStack& Stack = *It;
		if (Stack.Tag == Tag)
		{
			if (Stack.StackCount <= StackCount)
			{
				It.RemoveCurrent();
				TagToCountMap.Remove(Tag);
				MarkArrayDirty();
			}
			else
			{
				Stack.StackCount -= StackCount;
				TagToCountMap[Tag] = Stack.StackCount;
				MarkItemDirty(Stack);
			}
			return;
		}
	}
}

void FLNPGameplayTagStackContainer::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (int32 Index : RemovedIndices)
	{
		TagToCountMap.Remove(Stacks[Index].Tag);
	}
}

void FLNPGameplayTagStackContainer::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		const FLNPGameplayTagStack& Stack = Stacks[Index];
		TagToCountMap.Add(Stack.Tag, Stack.StackCount);
	}
}

void FLNPGameplayTagStackContainer::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (int32 Index : ChangedIndices)
	{
		const FLNPGameplayTagStack& Stack = Stacks[Index];
		TagToCountMap[Stack.Tag] = Stack.StackCount;
	}
}
