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

	/**
	 * ⚠️ **Iris는 제거를 이 콜백으로만 디스패치한다** — `PostReplicatedRemove`는 영영 불리지 않는다
	 * (FastArrayReplicationFragmentInternal.h:320 vs 333/340: Pre'Remove'/Post'Add'/Post'Change').
	 * 아래 PostReplicatedRemove는 Iris를 끈 클래식 경로용으로만 남아 있다.
	 *
	 * 그리고 이 콜백은 **항목이 아직 배열에 남아 있는 상태**에서 불린다 (실제 제거는 그 뒤).
	 * 그래서 통지를 다음 틱으로 미룬다 — 여기서 바로 알리면 UI가 방금 사라진 항목을 다시 그린다.
	 */
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);

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
