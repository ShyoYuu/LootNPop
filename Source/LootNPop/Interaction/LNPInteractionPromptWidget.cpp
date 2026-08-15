// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Interaction/LNPInteractionPromptWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/LNPInputHandlerComponent.h"
#include "CommonInputSubsystem.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "Styling/CoreStyle.h"
#include "UI/LNPInputGlyph.h"

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

		KeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptKeyText"));
		KeyText->SetText(KeyLabel);
		KeyText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 28));
		KeyText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		Background->SetContent(KeyText);
		WidgetTree->RootWidget = Background;
	}

	return bSuccess;
}

void ULNPInteractionPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// UWidgetComponent(EWidgetSpace::Screen)는 컴포넌트 가시성이 켜질 때 AddWidgetToScreen,
	// 꺼질 때 RemoveWidgetFromScreen을 부른다. 즉 NativeConstruct/NativeDestruct가
	// **프롬프트가 뜨고 사라질 때마다** 도므로, 여기가 곧 표시 시점 갱신 지점이다.
	if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		InputSubsystem->OnInputMethodChangedNative.AddUObject(this, &ULNPInteractionPromptWidget::HandleInputMethodChanged);
	}

	RefreshKeyGlyph();
}

void ULNPInteractionPromptWidget::NativeDestruct()
{
	if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		InputSubsystem->OnInputMethodChangedNative.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void ULNPInteractionPromptWidget::HandleInputMethodChanged(ECommonInputType NewInputType)
{
	RefreshKeyGlyph();
}

void ULNPInteractionPromptWidget::RefreshKeyGlyph()
{
	if (KeyText == nullptr)
		return;

	const APawn* OwningPawn = GetOwningPlayerPawn();
	const ULNPInputHandlerComponent* InputHandler =
		OwningPawn ? OwningPawn->FindComponentByClass<ULNPInputHandlerComponent>() : nullptr;
	if (InputHandler == nullptr)
		return;

	const FText Glyph = LNPInputGlyph::GetActionGlyph(GetOwningLocalPlayer(), InputHandler->GetInteractAction());

	// ⚠️ 빈 결과면 지우지 말고 직전 글리프를 유지한다. 키 해석은 **현재 적용 중인** 매핑 컨텍스트만 읽는데,
	// 메뉴가 열려 있는 동안에는 폰의 IMC_Pawn이 통째로 제거되어 있어 무효 키가 나온다.
	if (!Glyph.IsEmpty())
	{
		KeyText->SetText(Glyph);
	}
}
