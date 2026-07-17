// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "LNPInventoryList.generated.h"

class ULNPInventoryItemInstance;
class ULNPInventoryComponent;
struct FNetDeltaSerializeInfo;

/** FastArray 엔트리 — 아이템 인스턴스 하나를 참조한다 (인스턴스 자체는 등록 서브오브젝트로 복제). */
USTRUCT()
struct FLNPInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ULNPInventoryItemInstance> Instance = nullptr;
};

/**
 * 아이템 인스턴스의 FastArray 리스트 (Lyra FLyraInventoryList식). 델타 복제되며,
 * 복제 콜백에서 소유 컴포넌트의 OnInventoryChanged를 브로드캐스트해 UI를 갱신한다.
 * 가방(BagList)과 활성 버프(ActiveBuffList)가 모두 이 타입을 쓴다.
 */
USTRUCT()
struct FLNPInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	FLNPInventoryList() {}

	void SetOwnerComponent(ULNPInventoryComponent* InOwner) { OwnerComponent = InOwner; }

	/** 서버: 인스턴스를 엔트리로 추가하고 델타 마킹. */
	void AddEntry(ULNPInventoryItemInstance* Instance);

	/** 서버: 인스턴스를 제거하고 델타 마킹. */
	void RemoveEntry(ULNPInventoryItemInstance* Instance);

	const TArray<FLNPInventoryEntry>& GetEntries() const { return Entries; }

	// ~FFastArraySerializer 콜백 (클라이언트에서 UI 갱신 통지)
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	void PostReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FLNPInventoryEntry, FLNPInventoryList>(Entries, DeltaParms, *this);
	}

private:
	void NotifyOwnerChanged();

	UPROPERTY()
	TArray<FLNPInventoryEntry> Entries;

	/** 소유 컴포넌트 역참조 (비복제 — 양측 생성자에서 설정). */
	UPROPERTY(NotReplicated)
	TObjectPtr<ULNPInventoryComponent> OwnerComponent = nullptr;
};

template<>
struct TStructOpsTypeTraits<FLNPInventoryList> : public TStructOpsTypeTraitsBase2<FLNPInventoryList>
{
	enum { WithNetDeltaSerializer = true };
};
