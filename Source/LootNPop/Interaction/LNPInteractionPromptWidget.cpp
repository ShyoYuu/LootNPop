// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Interaction/LNPInteractionPromptWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

bool ULNPInteractionPromptWidget::Initialize()
{
	const bool bSuccess = Super::Initialize();

	// BP 서브클래스가 자체 디자인을 가진 경우(RootWidget 존재) C++ 기본 구성을 건너뛴다
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptBackground"));
		Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		Background->SetPadding(FMargin(14.0f, 6.0f));
		Background->SetHorizontalAlignment(HAlign_Center);
		Background->SetVerticalAlignment(VAlign_Center);

		UTextBlock* KeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptKeyText"));
		KeyText->SetText(KeyLabel);
		KeyText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 28));
		KeyText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		Background->SetContent(KeyText);
		WidgetTree->RootWidget = Background;
	}

	return bSuccess;
}
