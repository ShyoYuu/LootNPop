// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "LNPGameplayTagStack.generated.h"

struct FNetDeltaSerializeInfo;

/**
 * 게임플레이 태그 하나와 정수 카운트의 쌍.
 * 아이템 인스턴스의 데이터 주도 스탯(레벨·소모 탄수 등)을 서브클래스 없이 담는 단위.
 */
USTRUCT(BlueprintType)
struct FLNPGameplayTagStack : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FLNPGameplayTagStack() = default;
	FLNPGameplayTagStack(FGameplayTag InTag, int32 InCount)
		: Tag(InTag), Count(InCount)
	{}

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	int32 Count = 0;
};

/**
 * FLNPGameplayTagStack 목록. FastArray로 변경분만 델타 복제한다.
 * 아이템당 태그가 몇 개뿐이라 별도 캐시 없이 배열을 선형 탐색한다 —
 * 그래서 클라이언트 복제 콜백으로 맞춰 줄 파생 상태가 없다.
 */
USTRUCT(BlueprintType)
struct FLNPGameplayTagStackContainer : public FFastArraySerializer
{
	GENERATED_BODY()

	/** 태그의 카운트를 Delta만큼 늘린다. 없던 태그면 새로 추가한다. Delta <= 0은 무시. 서버 권위. */
	void AddStack(FGameplayTag Tag, int32 Delta);

	/** 태그의 카운트를 Delta만큼 줄인다. 0 이하가 되면 항목을 지운다. Delta <= 0은 무시. 서버 권위. */
	void RemoveStack(FGameplayTag Tag, int32 Delta);

	/** 태그의 현재 카운트 (없으면 0). */
	int32 GetStackCount(FGameplayTag Tag) const
	{
		const FLNPGameplayTagStack* Entry = Find(Tag);
		return Entry ? Entry->Count : 0;
	}

	bool ContainsTag(FGameplayTag Tag) const
	{
		return Find(Tag) != nullptr;
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FLNPGameplayTagStack, FLNPGameplayTagStackContainer>(Entries, DeltaParms, *this);
	}

private:
	const FLNPGameplayTagStack* Find(FGameplayTag Tag) const
	{
		return Entries.FindByPredicate([Tag](const FLNPGameplayTagStack& Entry) { return Entry.Tag == Tag; });
	}

	FLNPGameplayTagStack* Find(FGameplayTag Tag)
	{
		return Entries.FindByPredicate([Tag](const FLNPGameplayTagStack& Entry) { return Entry.Tag == Tag; });
	}

	/** 복제되는 태그 카운트 목록. */
	UPROPERTY()
	TArray<FLNPGameplayTagStack> Entries;
};

template<>
struct TStructOpsTypeTraits<FLNPGameplayTagStackContainer> : public TStructOpsTypeTraitsBase2<FLNPGameplayTagStackContainer>
{
	enum { WithNetDeltaSerializer = true };
};
