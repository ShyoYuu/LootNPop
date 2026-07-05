// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "LNPGameplayCueNotify_HitReact.generated.h"

/**
 * GameplayCue.LNP.Character.HitReact 발동 시 피격자의 HitReact 몽타주와 HitStop을 재생한다.
 * 블루프린트 그래프 없이 순수 C++로 동작 — 블루프린트 애셋은 부모 클래스·GameplayCueTag만 지정.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayCueNotify_HitReact : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
