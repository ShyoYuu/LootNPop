// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LNPInteractionPromptWidget.generated.h"

/**
 * 상호작용 프롬프트 기본 위젯 — 에셋 없이 C++만으로 키 라벨("F")을 표시한다.
 * 정식 아이콘 아트가 준비되면 ALNPLootPod의 WidgetComponent에서 WidgetClass만 교체하면 된다.
 * BP 서브클래스가 자체 디자인(RootWidget)을 가지면 C++ 기본 구성은 건너뛴다.
 */
UCLASS()
class LOOTNPOP_API ULNPInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

protected:
	/** 표시할 상호작용 키 라벨 */
	UPROPERTY(EditAnywhere, Category = "LNP|Interaction")
	FText KeyLabel = NSLOCTEXT("LNP", "InteractKeyLabel", "F");
};
