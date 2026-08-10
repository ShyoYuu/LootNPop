// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "LNPMenuButtonWidget.generated.h"

class UTextBlock;

/**
 * 라벨을 가진 범용 메뉴 버튼 (Equip / Drop 등).
 *
 * UCommonButtonBase에는 텍스트 개념이 없어서, 라벨을 쓰지 않으면 버튼이
 * 배경만 있는 빈 상자로 그려진다. 소유 위젯이 SetButtonLabel로 글자를 넣어 준다.
 *
 * BP 서브클래스(WBP_LNPMenuButton) 요구 사항: "ButtonLabel"(UTextBlock 파생) — Is Variable 켜기
 */
UCLASS()
class LOOTNPOP_API ULNPMenuButtonWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** 버튼에 표시할 이름을 설정한다. */
	void SetButtonLabel(const FText& InText);

protected:
	// UCommonTextBlock도 UTextBlock 파생이므로 어느 쪽으로 만들어도 바인딩된다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ButtonLabel;
};
