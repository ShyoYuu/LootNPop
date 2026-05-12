// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "ActiveGameplayEffectHandle.h"
#include "LNPItemInstance.generated.h"

class ULNPWeaponData;
class ULNPSkillData;
class ULNPBuffData;

USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPWeaponInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ULNPWeaponData> Definition = nullptr;

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

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> GrantedAbilities;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> AppliedEffects;

	bool IsValid() const { return Definition != nullptr; }
	void Reset() { *this = FLNPSkillInstance(); }
};

USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPBuffInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ULNPBuffData> Definition = nullptr;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> AppliedEffects;

	UPROPERTY(BlueprintReadOnly)
	float RemainingDuration = 0.0f;

	bool IsValid() const { return Definition != nullptr; }
	void Reset() { *this = FLNPBuffInstance(); }
};
