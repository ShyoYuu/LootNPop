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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes", ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes", ReplicatedUsing = OnRep_AttackSpeed)
	FGameplayAttributeData AttackSpeed;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, AttackSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes", ReplicatedUsing = OnRep_DefensePower)
	FGameplayAttributeData DefensePower;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, DefensePower)

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes", ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, MoveSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes", ReplicatedUsing = OnRep_AttackMultiplier)
	FGameplayAttributeData AttackMultiplier;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, AttackMultiplier)

	/** Meta 어트리뷰트: GE가 전달한 원시 피해량. PostGameplayEffectExecute에서 방어력 적용 후 즉시 0으로 초기화. 복제하지 않음. */
	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, IncomingDamage)

private:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_AttackSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_DefensePower(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_AttackMultiplier(const FGameplayAttributeData& OldValue);
};
