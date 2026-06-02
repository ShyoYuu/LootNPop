// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "LNPGameplayEffect_Damage.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Damage)

/** 즉발 피해 GE. Magnitude는 TAG_GE_Data_Damage로 SetByCaller 전달 (양수 원시 피해량). 방어력 차감은 PostGameplayEffectExecute에서 처리. */
UCLASS()
class LOOTNPOP_API ULNPGameplayEffect_Damage : public UGameplayEffect
{
	GENERATED_BODY()
public:
	ULNPGameplayEffect_Damage();
};
