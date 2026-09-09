// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/LNPScreenMarkerStyle.h"

/**
 * 화면 위 한 지점에 그릴 마커 하나.
 *
 * ⚠️ 원소가 하나뿐인 소비처(락온)가 있어도 API를 스칼라로 좁히지 않는다 —
 *    같은 위젯에 적 HP 바 N개가 그대로 올라타야 하기 때문이다.
 */
struct FLNPScreenMarker
{
	/** 위젯 로컬 좌표(슬레이트 단위). 뷰포트 픽셀을 DPI 스케일로 나눈 값이어야 한다. */
	FVector2f LocalPos = FVector2f::ZeroVector;

	/** 스타일의 MarkerSize에 곱해지는 배율. 원근 스케일이 여기로 들어온다. */
	float Scale = 1.f;

	/** 이 마커만의 추가 투명도. 페이드 인·아웃에 쓴다. */
	float Alpha = 1.f;

	/** 채움 비율 (0~1). 스타일의 bDrawFill이 꺼져 있으면 쓰이지 않는다. */
	float Ratio = 1.f;
};

/**
 * 스크린 스페이스 마커를 배열로 받아 한 덩어리로 그리는 위젯.
 *
 * 순수 엔티티(Actor 없는 적)에는 UWidgetComponent를 달 방법이 없어, 월드 스페이스 위젯 대신
 * 화면 좌표를 직접 받아 그린다. 위젯은 적이 무엇인지 모르고 좌표·비율·알파·스케일만 안다.
 *
 * 진행률을 SLATE_ATTRIBUTE로 열지 않는다 — 바인딩된 어트리뷰트는 매 프레임 평가되며 위젯을
 * volatile로 만든다. 값은 밖에서 SetMarkers로 민다. 위젯이 스스로 세는 시간이 없으므로 Tick도 없다.
 *
 * 전체 마커를 브러시당 MakeCustomVerts 한 번으로 배칭하므로, 마커가 몇 개든 드로우 콜은 최대 2개다.
 */
class LNPUI_API SLNPScreenMarkers : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLNPScreenMarkers)
		: _Style(&FLNPScreenMarkerStyle::GetDefault())
		{}

		/** 룩 전체 */
		SLATE_STYLE_ARGUMENT(FLNPScreenMarkerStyle, Style)

	SLATE_END_ARGS()

	SLNPScreenMarkers();

	void Construct(const FArguments& InArgs);

	/** 이번 프레임에 그릴 마커 전체를 교체한다. 소유권을 가져가므로 호출 측 배열은 비워진다. */
	void SetMarkers(TArray<FLNPScreenMarker>&& InMarkers);

	/** 마커를 모두 지운다. 아무것도 그리지 않는 상태가 된다. */
	void ClearMarkers();

	void SetStyle(const FLNPScreenMarkerStyle* InStyle);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	/** 화면 전체를 덮는 오버레이라 희망 크기를 주장하지 않는다 — 배치는 부모(Canvas 앵커)가 정한다. */
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	const FLNPScreenMarkerStyle* Style = nullptr;
	TArray<FLNPScreenMarker> Markers;
};
