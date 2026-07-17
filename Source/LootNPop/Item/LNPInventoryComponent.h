// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Item/LNPItemInstance.h"
#include "Item/LNPInventoryList.h"
#include "ActiveGameplayEffectHandle.h"
#include "LNPInventoryComponent.generated.h"

class ULNPItemDefinitionBase;
class ULNPBuffData;
class ULNPInventoryItemInstance;
class UAbilitySystemComponent;

/** 인벤토리(가방/버프) 내용이 바뀔 때 발송 — UI가 구독해 리스트를 갱신한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLNPInventoryChanged);

/** 서버 전용 버프 런타임 상태 (GAS 이펙트 핸들·권위 잔여시간). 복제·UPROPERTY 아님 — ItemId로 키잉. */
struct FLNPBuffRuntime
{
	TArray<FActiveGameplayEffectHandle> AppliedEffects;
	float RemainingDuration = 0.0f;
};

/**
 * 인벤토리 컴포넌트 (PlayerState 소유). 가방·활성 버프를 UObject 아이템 인스턴스로 담고
 * FastArray + 등록 서브오브젝트로 소유 클라이언트에 복제한다 (Part B 하이브리드 모델).
 * 인스턴스 정체성(ItemId)으로 "같은 DataAsset의 서로 다른 사본"을 구분한다.
 */
UCLASS(ClassGroup = (LNP), meta = (BlueprintSpawnableComponent))
class LOOTNPOP_API ULNPInventoryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	ULNPInventoryComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- 가방 인스턴스 API ---

	/** 서버: 인스턴스를 생성해 가방에 편입하고 등록 서브오브젝트로 복제한다. 생성된 인스턴스 반환. */
	ULNPInventoryItemInstance* AddItemInstance(ULNPItemDefinitionBase* ItemDef);

	/** 서버: ItemId로 가방 인스턴스를 제거한다. 성공 시 true. */
	bool RemoveItemInstance(const FGuid& ItemId);

	/** ItemId로 가방 인스턴스를 조회한다 (없으면 nullptr). */
	ULNPInventoryItemInstance* FindItemInstance(const FGuid& ItemId) const;

	/** 가방 인스턴스 목록 (UI용 — 장착본 포함, 필터는 UI가). */
	TArray<ULNPInventoryItemInstance*> GetBagInstances() const;

	// --- 버프 인스턴스 API ---

	/**
	 * 서버: 버프 인스턴스를 생성해 GE를 즉시 적용하고 활성 버프 리스트에 편입한다.
	 * @param InRemainingDuration  남은 초. 0 = ItemDef->MaxDuration (0 = 무한).
	 */
	UFUNCTION(BlueprintCallable, Category = "LNP|Inventory")
	void AddBuffItem(ULNPBuffData* ItemDef, float InRemainingDuration = 0.0f);

	/** 서버: ItemId로 버프 인스턴스를 제거하고 GE를 해제한 뒤 남은 지속 시간을 반환한다 (양도 페이로드용). */
	float RemoveBuffInstance(const FGuid& ItemId);

	/** ItemId로 활성 버프 인스턴스를 조회한다 (없으면 nullptr). */
	ULNPInventoryItemInstance* FindBuffInstance(const FGuid& ItemId) const;

	/** ItemDef와 일치하는 활성 버프 보유 여부 (BP 편의). */
	UFUNCTION(BlueprintPure, Category = "LNP|Inventory")
	bool HasBuffItem(const ULNPBuffData* ItemDef) const;

	/** 활성 버프 인스턴스 목록 (UI용). */
	TArray<ULNPInventoryItemInstance*> GetActiveBuffInstances() const;

	/** 인벤토리 내용 변경 통지 (서버는 변경 직후, 클라이언트는 FastArray 복제 콜백에서 발송). */
	UPROPERTY(BlueprintAssignable, Category = "LNP|Inventory")
	FLNPInventoryChanged OnInventoryChanged;

private:
	UAbilitySystemComponent* GetASC() const;
	void TickBuffItems(float DeltaTime);

	/** 서버: 버프 인스턴스 만료·제거 공용 경로 — GE 해제 + 리스트/서브오브젝트/런타임 제거. */
	void ExpireBuffInstance(const FGuid& ItemId);

	/** 가방 아이템 인스턴스 FastArray (소유자 전용 델타 복제). */
	UPROPERTY(Replicated)
	FLNPInventoryList BagList;

	/** 활성 버프 인스턴스 FastArray (소유자 전용 델타 복제). */
	UPROPERTY(Replicated)
	FLNPInventoryList ActiveBuffList;

	/** 서버 권위 버프 런타임 (GAS 핸들·잔여시간). ItemId 키. 비복제. */
	TMap<FGuid, FLNPBuffRuntime> BuffRuntime;
};
