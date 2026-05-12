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

	/** Add an item to the storage bag (no GAS effect). */
	UFUNCTION(BlueprintCallable, Category = "LNP|Inventory")
	void AddToStorage(ULNPItemDefinitionBase* ItemDef);

	UFUNCTION(BlueprintCallable, Category = "LNP|Inventory")
	bool RemoveFromStorage(ULNPItemDefinitionBase* ItemDef);

	/**
	 * Add a buff item and apply its GE immediately.
	 * @param InRemainingDuration  Remaining seconds. 0 = use ItemDef->MaxDuration (0 = infinite).
	 */
	UFUNCTION(BlueprintCallable, Category = "LNP|Inventory")
	void AddBuffItem(ULNPBuffData* ItemDef, float InRemainingDuration = 0.0f);

	/**
	 * Remove the first buff item matching ItemDef, revoke its GE, and return its remaining duration.
	 * Use the returned value when spawning the world-drop actor.
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
