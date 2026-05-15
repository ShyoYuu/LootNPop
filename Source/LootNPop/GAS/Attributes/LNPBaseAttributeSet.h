// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "LNPBaseAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class LOOTNPOP_API ULNPBaseAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	ULNPBaseAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes")
	FGameplayAttributeData AttackSpeed;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, AttackSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes")
	FGameplayAttributeData DefensePower;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, DefensePower)

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, MoveSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes")
	FGameplayAttributeData AttackMultiplier;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, AttackMultiplier)
};
