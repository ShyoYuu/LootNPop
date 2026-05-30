// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "LNPGameplayEffect_Damage.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_Damage)

/** 즉발 피해 GE. Magnitude는 TAG_GE_Data_Damage를 통해 SetByCaller로 설정한다 (음수 값으로 전달). */
UCLASS()
class LOOTNPOP_API ULNPGameplayEffect_Damage : public UGameplayEffect
{
	GENERATED_BODY()
public:
	ULNPGameplayEffect_Damage();
};
