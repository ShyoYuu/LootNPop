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

/**
 * 플레이어·적 공용 스탯.
 *
 * **버프 규약** — 스탯 하나당 어트리뷰트 하나다. 배율용 보조 어트리뷰트를 따로 두지 않는다.
 *   최종 = (기초 + 무기 스텟 + 합연산 버프) × (1 + Σ 곱연산 버프)
 * 합연산은 `EGameplayModOp::AddBase`, 곱연산은 `MultiplyAdditive`만 쓴다 (→ `GAS/LNPStatModifier.h`).
 * ⚠️ DivideAdditive / MultiplyCompound / AddFinal / Override를 쓰면 스탯 UI의 `C = A × B` 분해가 깨진다.
 *
 * ⚠️ 곱연산 버프는 기초값이 0인 스탯에서 무효다(0 × 1.4 = 0). 새 스탯을 추가할 때는
 *    기초값을 반드시 양수로 준다.
 */
UCLASS()
class LOOTNPOP_API ULNPBaseAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	ULNPBaseAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
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

	/** 루팅 속도 배율 — LootPod 게이지 기여 속도. 버프 GE가 이 값을 변조하면 FLNPPlayerLootingFragment로 자동 동기화된다. */
	UPROPERTY(BlueprintReadOnly, Category = "LNP|Attributes", ReplicatedUsing = OnRep_LootSpeed)
	FGameplayAttributeData LootSpeed;
	ATTRIBUTE_ACCESSORS(ULNPBaseAttributeSet, LootSpeed)

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
	void OnRep_LootSpeed(const FGameplayAttributeData& OldValue);
};
