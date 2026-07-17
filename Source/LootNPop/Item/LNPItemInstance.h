// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "ActiveGameplayEffectHandle.h"
#include "LNPItemInstance.generated.h"

class ULNPWeaponData;
class ULNPSkillData;
class ULNPBuffData;
class ULNPInventoryItemInstance;

USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPWeaponInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ULNPWeaponData> Definition = nullptr;

	/** 이 장착을 유발한 가방 인스턴스 (bag-equipped면 유효, 기본/innate 무기면 null). */
	UPROPERTY()
	TObjectPtr<ULNPInventoryItemInstance> SourceInstance = nullptr;

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> GrantedAbilities;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> AppliedEffects;

	bool IsValid() const { return Definition != nullptr; }
	void Reset() { *this = FLNPWeaponInstance(); }
};

USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPSkillInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ULNPSkillData> Definition = nullptr;

	/** 이 장착을 유발한 가방 인스턴스 (bag-equipped면 유효). */
	UPROPERTY()
	TObjectPtr<ULNPInventoryItemInstance> SourceInstance = nullptr;

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> GrantedAbilities;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> AppliedEffects;

	bool IsValid() const { return Definition != nullptr; }
	void Reset() { *this = FLNPSkillInstance(); }
};
