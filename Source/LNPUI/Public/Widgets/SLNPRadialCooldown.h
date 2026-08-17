// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/LNPRadialCooldownStyle.h"

/**
 * 방사형 쿨다운 가림막 위젯.
 *
 * 아이콘 위를 덮은 부채꼴이 쿨다운이 도는 동안 걷힌다 — UProgressBar·UImage 조합으로는
 * 부채꼴을 그릴 수 없어 직접 그린다.
 *
 * 진행률을 SLATE_ATTRIBUTE로 열지 않는다. 그러면 값을 매 프레임 밀어 주는 쪽에 Tick이 생기고
 * 어트리뷰트가 매 프레임 평가되므로, 대신 위젯이 시작 시점에 duration 하나만 받고
 * 경과 시간을 스스로 누적한다. 쿨다운이 없는 동안에는 Tick이 꺼져 비용이 0이다.
 */
class LNPUI_API SLNPRadialCooldown : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLNPRadialCooldown)
		: _Style(&FLNPRadialCooldownStyle::GetDefault())
		, _SegmentCount(64)
		{}

		/** 룩 전체 */
		SLATE_STYLE_ARGUMENT(FLNPRadialCooldownStyle, Style)

		/** 호를 근사할 최대 세그먼트 수. 남은 각에 비례해 실제 사용량은 줄어든다. */
		SLATE_ARGUMENT(int32, SegmentCount)

		/** 쿨다운이 끝난 순간 한 번 호출된다. */
		SLATE_EVENT(FSimpleDelegate, OnCooldownFinished)

	SLATE_END_ARGS()

	SLNPRadialCooldown();

	void Construct(const FArguments& InArgs);

	/** 쿨다운을 시작한다. 진행 중에 다시 호출하면 처음부터 재시작한다. */
	void StartCooldown(float InDurationSeconds);

	/** 진행 중인 쿨다운을 즉시 지운다. OnCooldownFinished는 발송하지 않는다. */
	void ClearCooldown();

	bool IsCooldownActive() const { return Duration > 0.f; }

	void SetStyle(const FLNPRadialCooldownStyle* InStyle);
	void SetSegmentCount(int32 InSegmentCount);

	/** 디자이너 프리뷰용 고정 표시 비율. 0 미만이면 실제 쿨다운 상태를 따른다. */
	void SetPreviewRemaining(float InRemaining);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	/** 지금 그려야 할 남은 비율 (1 = 전부 가림, 0 = 그리지 않음). */
	float GetRemainingRatio() const;

	const FLNPRadialCooldownStyle* Style = nullptr;
	FSimpleDelegate OnCooldownFinished;

	int32 SegmentCount = 64;
	float Duration = 0.f;
	float Elapsed = 0.f;
	float PreviewRemaining = -1.f;
};
