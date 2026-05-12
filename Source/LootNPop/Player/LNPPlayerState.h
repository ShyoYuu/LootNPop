// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "LNPPlayerState.generated.h"

class UAbilitySystemComponent;
class ULNPBaseAttributeSet;
class ULNPEquipmentComponent;
class ULNPInventoryComponent;

UCLASS()
class LOOTNPOP_API ALNPPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	ALNPPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "LNP|Equipment")
	ULNPEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	UFUNCTION(BlueprintPure, Category = "LNP|Inventory")
	ULNPInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<ULNPBaseAttributeSet> BaseAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPInventoryComponent> InventoryComponent;
};
