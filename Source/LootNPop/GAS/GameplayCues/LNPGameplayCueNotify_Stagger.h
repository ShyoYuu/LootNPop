// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "LNPGameplayCueNotify_Stagger.generated.h"

/**
 * GameplayCue.LNP.Character.Stagger 발동 시 경직 몽타주를 재생한다.
 * 블루프린트 그래프 없이 순수 C++로 동작 — 블루프린트 애셋은 부모 클래스·GameplayCueTag만 지정.
 *
 * 경직 단계는 Parameters.AggregatedSourceTags에 실린 LNP.Montage.Value.Stagger.* 태그로 전달된다
 * (FLNPStaggerCommand::Run 참조). 태그가 없으면 Light로 본다.
 *
 * 몽타주가 GA가 아니라 이 큐에 붙어 있는 이유는 ULNPAbility_Stagger 주석 참조 —
 * 적 ASC의 Minimal 복제로는 시뮬 프록시에 어빌리티 활성화가 도달하지 않는다.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayCueNotify_Stagger : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
