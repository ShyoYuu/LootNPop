// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "LNPMenuTabButtonWidget.generated.h"

class UCommonTextBlock;
class UWidget;

/**
 * 상단 카테고리 탭 버튼.
 *
 * UCommonButtonBase는 라벨 개념이 없으므로, 탭 리스트가 생성 직후 SetTabLabel로 이름을 넣어 준다.
 *
 * **선택 표현은 밑줄**(기획 §3의 스케일 확대와 병행)이다.
 * ⚠️ 밑줄은 `UCommonButtonStyle`의 브러시로는 만들 수 없다 — 버튼 브러시는 버튼 지오메트리 전체를
 * 채우므로 "아래 몇 px만" 그릴 방법이 없다. 그래서 밑줄 위젯을 따로 두고 선택 상태에 맞춰 토글한다.
 *
 * BP 서브클래스(WBP_LNPMenuTabButton) 요구 사항 — Is Variable 켜기:
 *  - "TabLabel" (UCommonTextBlock)
 *  - "SelectionUnderline" (UImage 등 아무 UWidget) — 선택됐을 때만 보인다
 */
UCLASS()
class LOOTNPOP_API ULNPMenuTabButtonWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** 탭 버튼에 표시할 이름을 설정한다. */
	void SetTabLabel(const FText& InText);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> TabLabel;

	/** 선택된 탭 아래에 깔리는 강조 밑줄. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectionUnderline;

private:
	/** 현재 선택 상태에 맞춰 밑줄 가시성을 맞춘다. */
	void UpdateUnderline(bool bIsTabSelected);
};
