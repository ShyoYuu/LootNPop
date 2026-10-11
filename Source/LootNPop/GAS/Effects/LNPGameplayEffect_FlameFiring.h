// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LNPGameplayEffect_FlameFiring.generated.h"

/**
 * 분사 중 상태 — 발사(틱)마다 다시 적용되어 지속시간을 갱신하고, 끊기면 지속시간 뒤에 스스로 풀린다.
 * 부여 태그 LNP.State.FlameFiring이 충전형 탄창 회복(ULNPGameplayEffect_MagazineRegen)을 막는다.
 *
 * Duration은 어빌리티가 무기의 MagazineRegenDelay로 주입한다 — "마지막 발사 후 회복 지연"이 곧 이 GE의 수명이다.
 *
 * ⚠️ 서버에서만 적용한다. 스택 갱신형 GE의 예측 적용은 엔진 기본값(AbilitySystem.AllowPredictiveStackingGEs=false)이
 * 막는다. 회복이 서버에서만 돌기 때문에 이 태그도 서버에만 있으면 충분하다.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayEffect_FlameFiring : public UGameplayEffect
{
	GENERATED_BODY()
public:
	ULNPGameplayEffect_FlameFiring();
};
