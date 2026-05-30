// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LNPHpBarWidget.generated.h"

class UProgressBar;

/**
 * Enemy NPC를 위한 World Space HpBar Widget.
 * 수동적 View로 동작: 푸시된 HP 데이터를 받아 렌더링한다.
 * Blueprint 서브클래스(WBP_LNPHpBar)에 "HpBar"라는 이름의 UProgressBar가 있어야 한다.
 */
UCLASS()
class LOOTNPOP_API ULNPHpBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 원시 HP 값을 시각적 퍼센트로 변환한다. ALNPEnemyCharacter에서 호출. */
	void UpdateHpBar(float Current, float Max);

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> HpBar;
};
