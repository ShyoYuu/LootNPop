// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "LNPGameplayCueNotify_AttackerHitStop.generated.h"

/**
 * GameplayCue.LNP.Melee.AttackerHitStop 발동 시 근접 공격자의 HitStop을 제3자(구경꾼) 화면에 전파한다.
 * 공격자 본인 화면은 이미 예측 경로(서버 직접 호출 또는 ApplyLocalHitFeedback)로 즉시 처리되므로,
 * MyTarget이 그 클라이언트에서 로컬 컨트롤 중이면 중복 재생을 피하기 위해 no-op 처리한다.
 * 블루프린트 그래프 없이 순수 C++로 동작 — 블루프린트 애셋은 부모 클래스·GameplayCueTag만 지정.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayCueNotify_AttackerHitStop : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
