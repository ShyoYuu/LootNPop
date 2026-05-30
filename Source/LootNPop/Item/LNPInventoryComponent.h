// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Item/LNPItemInstance.h"
#include "LNPInventoryComponent.generated.h"

class ULNPItemDefinitionBase;
class ULNPBuffData;
class UAbilitySystemComponent;

UCLASS(ClassGroup = (LNP), meta = (BlueprintSpawnableComponent))
class LOOTNPOP_API ULNPInventoryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	ULNPInventoryComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 아이템을 보관 가방에 추가한다 (GAS 효과 없음). */
	UFUNCTION(BlueprintCallable, Category = "LNP|Inventory")
	void AddToStorage(ULNPItemDefinitionBase* ItemDef);

	UFUNCTION(BlueprintCallable, Category = "LNP|Inventory")
	bool RemoveFromStorage(ULNPItemDefinitionBase* ItemDef);

	/**
	 * 버프 아이템을 추가하고 GE를 즉시 적용한다.
	 * @param InRemainingDuration  남은 초. 0 = ItemDef->MaxDuration 사용 (0 = 무한).
	 */
	UFUNCTION(BlueprintCallable, Category = "LNP|Inventory")
	void AddBuffItem(ULNPBuffData* ItemDef, float InRemainingDuration = 0.0f);

	/**
	 * ItemDef와 일치하는 첫 번째 버프 아이템을 제거하고 GE를 해제한 후 남은 지속 시간을 반환한다.
	 * World 드롭 Actor를 스폰할 때 반환 값을 사용한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "LNP|Inventory")
	float RemoveBuffItem(ULNPBuffData* ItemDef);

	const TArray<TObjectPtr<ULNPItemDefinitionBase>>& GetStoredItems() const { return StoredItems; }

private:
	UAbilitySystemComponent* GetASC() const;
	void TickBuffItems(float DeltaTime);
	void ExpireBuffItem(int32 Index);

	UPROPERTY()
	TArray<TObjectPtr<ULNPItemDefinitionBase>> StoredItems;

	UPROPERTY()
	TArray<FLNPBuffInstance> BuffItems;
};
