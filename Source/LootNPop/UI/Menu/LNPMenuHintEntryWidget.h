// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "LNPMenuHintEntryWidget.generated.h"

class UCommonTextBlock;

/**
 * 하단 힌트 바의 칸 하나 — 키 심볼 + 조작 설명.
 *
 * 폰트·색·간격·테두리 같은 시각 요소는 전부 이 WBP 안에서 결정된다.
 * C++는 두 텍스트에 값을 넣을 뿐이라, 글리프를 키캡 모양 테두리로 감싸거나 라벨을 아래로 내리는
 * 식의 변경에 코드가 필요 없다.
 *
 * BP 서브클래스(WBP_LNPMenuHintEntry) 요구 사항 — ⚠️ Is Variable 켜기:
 *  - UCommonTextBlock "GlyphText"
 *  - UCommonTextBlock "LabelText"
 */
UCLASS()
class LOOTNPOP_API ULNPMenuHintEntryWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetHint(const FText& InGlyph, const FText& InLabel);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> GlyphText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> LabelText;
};
