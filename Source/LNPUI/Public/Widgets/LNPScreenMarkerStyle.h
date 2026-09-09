// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateWidgetStyle.h"
#include "LNPScreenMarkerStyle.generated.h"

/**
 * SLNPScreenMarkers의 룩을 정의하는 스타일.
 *
 * 같은 위젯 클래스가 이 구조체 하나로 룩이 갈린다 — 락온 마커는 채움 없는 표식 하나,
 * 적 HP 바는 배경 + 비율 채움. 위젯은 "어떻게 보일지"만 알고 무엇을 가리키는지는 모른다.
 *
 * ⚠️ 커스텀 정점으로 직접 그리므로 **단순 Image 브러시만 지원한다.**
 *    9-slice(Box/Border)나 Tile 브러시를 넣으면 늘어난 사각형 하나로만 나온다.
 */
USTRUCT(BlueprintType)
struct LNPUI_API FLNPScreenMarkerStyle : public FSlateWidgetStyle
{
	GENERATED_BODY()

	FLNPScreenMarkerStyle();
	virtual ~FLNPScreenMarkerStyle() override;

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; }
	static const FLNPScreenMarkerStyle& GetDefault();

	/** 브러시를 참조로 노출한다. 누락 시 쿠킹에서 텍스처가 빠질 수 있다. */
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;

	/** 마커 본체(락온) 또는 게이지 배경(HP 바). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush BackBrush;

	/** 비율만큼 채워지는 브러시. bDrawFill이 켜져 있을 때만 쓰인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush FillBrush;

	/** 켜면 배경 위에 Ratio만큼의 채움을 그린다. 락온 마커처럼 비율이 없는 표식은 끈다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	bool bDrawFill = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor BackTint = FLinearColor(0.f, 0.f, 0.f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor FillTint = FLinearColor(0.85f, 0.15f, 0.15f, 1.f);

	/** 스케일 1일 때의 마커 크기 (슬레이트 단위). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FVector2D MarkerSize = FVector2D(64.f, 64.f);

	/** 넘겨받은 스크린 좌표를 마커의 어디에 맞출지. (0.5, 0.5) = 중앙, (0.5, 1) = 아래 중앙. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FVector2D Pivot = FVector2D(0.5f, 0.5f);

	/** 채움이 배경 안쪽으로 들어가는 여백 (슬레이트 단위). 테두리처럼 보이게 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance, meta = (ClampMin = "0.0"))
	float FillInset = 1.f;
};
