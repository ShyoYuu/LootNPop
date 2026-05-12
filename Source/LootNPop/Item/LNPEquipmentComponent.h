// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Item/LNPItemInstance.h"
#include "LNPEquipmentComponent.generated.h"

class ULNPItemDefinitionBase;
class ULNPWeaponData;
class ULNPSkillData;
class UAbilitySystemComponent;

UCLASS(ClassGroup = (LNP), meta = (BlueprintSpawnableComponent))
class LOOTNPOP_API ULNPEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	ULNPEquipmentComponent();
	virtual void BeginPlay() override;

	/** Unequips any current weapon, then equips WeaponDef and grants its GA/GE. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void EquipWeapon(ULNPWeaponData* WeaponDef);

	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void UnequipWeapon();

	/** Equips an active skill into SlotIndex (0-based). Revokes the previous occupant. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void EquipActiveSkill(int32 SlotIndex, ULNPSkillData* SkillDef);

	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void UnequipActiveSkill(int32 SlotIndex);

	/** Adds a passive skill (no slot limit). GA is granted immediately. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void AddPassiveSkill(ULNPSkillData* SkillDef);

	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void RemovePassiveSkill(ULNPSkillData* SkillDef);

	const FLNPWeaponInstance& GetWeaponSlot() const { return WeaponSlot; }
	const TArray<FLNPSkillInstance>& GetActiveSkillSlots() const { return ActiveSkillSlots; }
	int32 GetMaxActiveSkillSlots() const { return MaxActiveSkillSlots; }

	/** Default weapon equipped on BeginPlay. Assign the DA_Pistol DataAsset here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LNP|Equipment|Defaults")
	TObjectPtr<ULNPWeaponData> DefaultWeapon;

private:
	UAbilitySystemComponent* GetASC() const;
	void GrantItemImpl(ULNPItemDefinitionBase* Def,
	                   TArray<FGameplayAbilitySpecHandle>& OutAbilities,
	                   TArray<FActiveGameplayEffectHandle>& OutEffects);
	void RevokeItemImpl(TArray<FGameplayAbilitySpecHandle>& Abilities,
	                    TArray<FActiveGameplayEffectHandle>& Effects);

	UPROPERTY()
	FLNPWeaponInstance WeaponSlot;

	UPROPERTY()
	TArray<FLNPSkillInstance> ActiveSkillSlots;

	UPROPERTY()
	TArray<FLNPSkillInstance> PassiveSkillInstances;

	int32 MaxActiveSkillSlots = 4;
};
