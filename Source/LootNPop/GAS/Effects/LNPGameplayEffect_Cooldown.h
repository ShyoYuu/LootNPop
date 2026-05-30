// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LNPGameplayEffect_Cooldown.generated.h"

/**
 * 원거리 (및 향후) 공격 Ability가 적용하는 쿨다운 GE.
 * Duration은 Ability가 spec별로 주입하므로 단일 클래스로
 * 발사 속도가 다른 모든 무기를 처리할 수 있다.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayEffect_Cooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	ULNPGameplayEffect_Cooldown();
};
