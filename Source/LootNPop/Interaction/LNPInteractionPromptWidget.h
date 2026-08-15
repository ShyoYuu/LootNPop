// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LNPInteractionPromptWidget.generated.h"

enum class ECommonInputType : uint8;

class UTextBlock;

/**
 * 상호작용 프롬프트 기본 위젯 — 에셋 없이 C++만으로 키 라벨을 표시한다.
 *
 * 라벨은 현재 입력 타입에 실제로 바인딩된 키를 따라간다 (키보드 F / 게임패드 □).
 * 키는 IA_Interaction의 Enhanced Input 매핑에서 뽑으므로 **리매핑을 자동으로 따라간다.**
 *
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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 표시할 상호작용 키 라벨의 초기값 — 키 해석에 실패했을 때 그대로 남는다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Interaction")
	FText KeyLabel = NSLOCTEXT("LNP", "InteractKeyLabel", "F");

private:
	/** 현재 입력 타입으로 키 글리프를 다시 해석해 KeyText에 반영한다. */
	void RefreshKeyGlyph();

	void HandleInputMethodChanged(ECommonInputType NewInputType);

	/** Initialize()가 만든 라벨 텍스트 블록. BP 서브클래스가 자체 트리를 가지면 null이다. */
	UPROPERTY()
	TObjectPtr<UTextBlock> KeyText;
};
