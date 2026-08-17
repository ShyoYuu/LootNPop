// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GAS/LNPStatModifier.h"
#include "LNPItemDefinitionBase.generated.h"

class ULNPGameplayAbility;
class UGameplayEffect;

UCLASS(Abstract, BlueprintType)
class LOOTNPOP_API ULNPItemDefinitionBase : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** LootDice 6면·인벤토리 UI에 표시할 보상 아이콘 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<ULNPGameplayAbility>> AbilitiesToGrant;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TArray<TSubclassOf<UGameplayEffect>> EffectsToApply;

	/**
	 * 이 아이템이 주는 스탯 변경. 무기는 장착 중, 버프는 보유 중 적용된다.
	 * 공용 GE 2종으로 적용되므로 스탯×연산 조합마다 GE 에셋을 만들 필요가 없다 (→ GAS/LNPStatModifier.h).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	TArray<FLNPStatModifier> StatModifiers;
};
