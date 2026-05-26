// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LNPHpBarWidget.generated.h"

class UProgressBar;

/**
 * World-space health bar widget for enemy NPCs.
 * Acts as a passive View: receives pushed health data and renders it.
 * The Blueprint subclass (WBP_LNPHpBar) must contain a UProgressBar named "HpBar".
 */
UCLASS()
class LOOTNPOP_API ULNPHpBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Converts raw health values to a visual percentage. Called from ALNPEnemyCharacter. */
	void UpdateHpBar(float Current, float Max);

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> HpBar;
};
