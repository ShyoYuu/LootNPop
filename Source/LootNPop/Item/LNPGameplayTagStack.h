// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "LNPGameplayTagStack.generated.h"

struct FLNPGameplayTagStackContainer;
struct FNetDeltaSerializeInfo;

/**
 * 태그 하나 + 정수 스택 카운트 쌍 (Lyra FGameplayTagStack 포팅).
 * 아이템 인스턴스의 데이터 주도 스탯(레벨·랜덤 스탯 등)을 서브클래스 폭발 없이 담기 위한 단위.
 */
USTRUCT(BlueprintType)
struct FLNPGameplayTagStack : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FLNPGameplayTagStack() {}
	FLNPGameplayTagStack(FGameplayTag InTag, int32 InStackCount)
		: Tag(InTag), StackCount(InStackCount)
	{}

	FString GetDebugString() const;

private:
	friend FLNPGameplayTagStackContainer;

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	int32 StackCount = 0;
};

/**
 * FLNPGameplayTagStack의 FastArray 컨테이너 (Lyra FGameplayTagStackContainer 포팅).
 * 델타 복제(변경분만 전송)되며, 조회 가속용 TMap 캐시(비복제)를 유지한다.
 */
USTRUCT(BlueprintType)
struct FLNPGameplayTagStackContainer : public FFastArraySerializer
{
	GENERATED_BODY()

	FLNPGameplayTagStackContainer() {}

public:
	/** 지정 태그의 스택을 StackCount만큼 더한다 (StackCount <= 0 무시). 서버 권위. */
	void AddStack(FGameplayTag Tag, int32 StackCount);

	/** 지정 태그의 스택을 StackCount만큼 뺀다. 0 이하가 되면 태그 제거. 서버 권위. */
	void RemoveStack(FGameplayTag Tag, int32 StackCount);

	/** 지정 태그의 현재 스택 수 (없으면 0). */
	int32 GetStackCount(FGameplayTag Tag) const
	{
		return TagToCountMap.FindRef(Tag);
	}

	bool ContainsTag(FGameplayTag Tag) const
	{
		return TagToCountMap.Contains(Tag);
	}

	// ~FFastArraySerializer 콜백 — 클라이언트에서 캐시 맵을 동기화
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FLNPGameplayTagStack, FLNPGameplayTagStackContainer>(Stacks, DeltaParms, *this);
	}

private:
	/** 복제되는 스택 목록. */
	UPROPERTY()
	TArray<FLNPGameplayTagStack> Stacks;

	/** 조회 가속 캐시 (비복제 — 콜백에서 재구성). */
	TMap<FGameplayTag, int32> TagToCountMap;
};

template<>
struct TStructOpsTypeTraits<FLNPGameplayTagStackContainer> : public TStructOpsTypeTraitsBase2<FLNPGameplayTagStackContainer>
{
	enum { WithNetDeltaSerializer = true };
};
