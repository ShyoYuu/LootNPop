// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Widgets/SLNPRadialCooldown.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

SLNPRadialCooldown::SLNPRadialCooldown()
{
	// 쿨다운이 도는 동안에만 Tick을 켠다. 유휴 상태의 비용은 0이다.
	SetCanTick(false);
}

void SLNPRadialCooldown::Construct(const FArguments& InArgs)
{
	Style              = InArgs._Style;
	SegmentCount       = FMath::Max(3, InArgs._SegmentCount);
	OnCooldownFinished = InArgs._OnCooldownFinished;

	// 부채꼴 반지름이 사각형의 반대각선이라 모서리 밖으로 넘친다. 위젯 경계에서 잘라낸다.
	// ⚠️ UMG로 쓸 때는 UWidget::SynchronizeProperties가 이 값을 덮어쓰므로
	//    래퍼(ULNPRadialCooldownWidget) 생성자에서도 같은 기본값을 준다.
	SetClipping(EWidgetClipping::ClipToBounds);
}

void SLNPRadialCooldown::StartCooldown(float InDurationSeconds)
{
	if (InDurationSeconds <= 0.f)
	{
		ClearCooldown();
		return;
	}

	Duration = InDurationSeconds;
	Elapsed  = 0.f;

	SetCanTick(true);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SLNPRadialCooldown::ClearCooldown()
{
	Duration = 0.f;
	Elapsed  = 0.f;

	SetCanTick(false);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SLNPRadialCooldown::SetStyle(const FLNPRadialCooldownStyle* InStyle)
{
	Style = InStyle ? InStyle : &FLNPRadialCooldownStyle::GetDefault();
	Invalidate(EInvalidateWidgetReason::Layout);   // DesiredSize가 스타일에서 나오므로 Layout이다
}

void SLNPRadialCooldown::SetSegmentCount(int32 InSegmentCount)
{
	const int32 NewCount = FMath::Max(3, InSegmentCount);
	if (NewCount == SegmentCount)
		return;

	SegmentCount = NewCount;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SLNPRadialCooldown::SetPreviewRemaining(float InRemaining)
{
	PreviewRemaining = InRemaining;
	Invalidate(EInvalidateWidgetReason::Paint);
}

float SLNPRadialCooldown::GetRemainingRatio() const
{
	if (PreviewRemaining >= 0.f)
		return FMath::Min(PreviewRemaining, 1.f);

	if (Duration <= 0.f)
		return 0.f;

	return FMath::Clamp(1.f - Elapsed / Duration, 0.f, 1.f);
}

void SLNPRadialCooldown::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (Duration <= 0.f)
	{
		SetCanTick(false);
		return;
	}

	Elapsed += InDeltaTime;

	// 부채꼴만 달라지고 희망 크기는 그대로이므로 Paint 사유로만 무효화한다.
	Invalidate(EInvalidateWidgetReason::Paint);

	if (Elapsed >= Duration)
	{
		Duration = 0.f;
		Elapsed  = 0.f;
		SetCanTick(false);
		OnCooldownFinished.ExecuteIfBound();
	}
}

FVector2D SLNPRadialCooldown::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return Style ? Style->DesiredSize : FVector2D(64.f, 64.f);
}

int32 SLNPRadialCooldown::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const float Remaining = GetRemainingRatio();
	if (!Style || Remaining <= 0.f)
		return LayerId;

	const FVector2f LocalSize = AllottedGeometry.GetLocalSize();
	if (LocalSize.X <= 0.f || LocalSize.Y <= 0.f)
		return LayerId;

	// 브러시에 리소스가 없으면 커스텀 정점을 배칭할 핸들을 못 얻으므로 엔진 기본 흰색 브러시로 대체한다.
	const FSlateBrush* Brush = &Style->OverlayBrush;
	if (!Brush->GetResourceObject() && Brush->GetResourceName().IsNone())
		Brush = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));

	const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
	if (!Handle.IsValid())
		return LayerId;

	// 부모의 색·투명도를 곱하지 않으면 부모를 페이드아웃해도 이 위젯만 남는다.
	const FLinearColor Tint = Style->OverlayTint
		* Brush->GetTint(InWidgetStyle)
		* InWidgetStyle.GetColorAndOpacityTint();
	const FColor VertexColor = Tint.ToFColor(true);

	const FVector2f Center = LocalSize * 0.5f;
	const float Radius     = Center.Size();   // 반대각선 절반 — 사각형 아이콘의 네 모서리까지 덮는다

	// 가림막은 "아직 남은 각"을 차지한다. 시작 모서리는 StartAngle에 고정되고 반대 모서리가 쓸려 나간다.
	const float Direction  = Style->bClockwise ? 1.f : -1.f;   // 화면 좌표는 Y가 아래로 향해 +각이 시계 방향
	const float SweepDeg   = 360.f * Remaining * Direction;
	const float BeginDeg   = Style->StartAngleDegrees + 360.f * (1.f - Remaining) * Direction;

	const int32 Segments = FMath::Max(1, FMath::CeilToInt(SegmentCount * Remaining));

	const FSlateRenderTransform& RenderTransform = AllottedGeometry.ToPaintGeometry().GetAccumulatedRenderTransform();
	const FVector2f UV(0.5f, 0.5f);

	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex>   Indices;
	Vertices.Reserve(Segments + 2);
	Indices.Reserve(Segments * 3);

	Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RenderTransform, Center, UV, VertexColor));

	for (int32 i = 0; i <= Segments; ++i)
	{
		const float Radians = FMath::DegreesToRadians(BeginDeg + SweepDeg * (static_cast<float>(i) / Segments));
		const FVector2f Point = Center + FVector2f(FMath::Cos(Radians), FMath::Sin(Radians)) * Radius;
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RenderTransform, Point, UV, VertexColor));
	}

	for (int32 i = 0; i < Segments; ++i)
	{
		Indices.Add(0);
		Indices.Add(i + 1);
		Indices.Add(i + 2);
	}

	FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, Handle, Vertices, Indices, nullptr, 0, 0);

	return LayerId + 1;
}
