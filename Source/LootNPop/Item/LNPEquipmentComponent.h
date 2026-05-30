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

	/** 현재 무기를 해제한 후 WeaponDef를 장착하고 GA/GE를 부여한다. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void EquipWeapon(ULNPWeaponData* WeaponDef);

	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void UnequipWeapon();

	/** 활성 스킬을 SlotIndex(0부터)에 장착한다. 이전 점유자를 해제한다. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void EquipActiveSkill(int32 SlotIndex, ULNPSkillData* SkillDef);

	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void UnequipActiveSkill(int32 SlotIndex);

	/** 패시브 스킬을 추가한다 (슬롯 제한 없음). GA를 즉시 부여한다. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void AddPassiveSkill(ULNPSkillData* SkillDef);

	UFUNCTION(BlueprintCallable, Category = "LNP|Equipment")
	void RemovePassiveSkill(ULNPSkillData* SkillDef);

	const FLNPWeaponInstance& GetWeaponSlot() const { return WeaponSlot; }
	const TArray<FLNPSkillInstance>& GetActiveSkillSlots() const { return ActiveSkillSlots; }
	int32 GetMaxActiveSkillSlots() const { return MaxActiveSkillSlots; }

	/** BeginPlay 시 장착되는 기본 무기. 여기에 DA_Pistol DataAsset을 할당한다. */
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
