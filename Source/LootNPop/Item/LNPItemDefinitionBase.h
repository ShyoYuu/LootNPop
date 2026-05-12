// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<ULNPGameplayAbility>> AbilitiesToGrant;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TArray<TSubclassOf<UGameplayEffect>> EffectsToApply;
};
