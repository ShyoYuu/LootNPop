// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Widgets/SLNPScreenMarkers.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace LNPScreenMarkers
{
	/** 브러시에 리소스가 없으면 커스텀 정점을 배칭할 핸들을 못 얻으므로 엔진 기본 흰색 브러시로 대체한다. */
	static const FSlateBrush* ResolveBrush(const FSlateBrush& InBrush)
	{
		if (!InBrush.GetResourceObject() && InBrush.GetResourceName().IsNone())
			return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));

		return &InBrush;
	}

	/**
	 * 사각형 하나를 정점 버퍼에 밀어 넣는다.
	 * UMax는 U 방향으로 브러시의 어디까지 쓸지 — 채움 사각형이 잘린 만큼 텍스처도 함께 잘리게 한다.
	 */
	static void AppendQuad(TArray<FSlateVertex>& Vertices, TArray<SlateIndex>& Indices,
		const FSlateRenderTransform& RenderTransform, const FVector2f& TopLeft, const FVector2f& Size,
		const float UMax, const FColor& Color)
	{
		const SlateIndex Base = static_cast<SlateIndex>(Vertices.Num());

		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
			RenderTransform, TopLeft, FVector2f(0.f, 0.f), Color));
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
			RenderTransform, FVector2f(TopLeft.X + Size.X, TopLeft.Y), FVector2f(UMax, 0.f), Color));
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
			RenderTransform, TopLeft + Size, FVector2f(UMax, 1.f), Color));
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
			RenderTransform, FVector2f(TopLeft.X, TopLeft.Y + Size.Y), FVector2f(0.f, 1.f), Color));

		Indices.Add(Base);
		Indices.Add(Base + 1);
		Indices.Add(Base + 2);
		Indices.Add(Base);
		Indices.Add(Base + 2);
		Indices.Add(Base + 3);
	}
}

SLNPScreenMarkers::SLNPScreenMarkers()
{
	// 값을 밖에서 매 프레임 밀어 넣으므로 위젯이 스스로 셀 것이 없다.
	SetCanTick(false);
}

void SLNPScreenMarkers::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;

	// 화면 가장자리에 걸친 마커가 위젯 밖으로 넘치지 않게 한다.
	// ⚠️ UMG로 쓸 때는 UWidget::SynchronizeProperties가 래퍼의 Clipping으로 이 값을 덮어쓰므로,
	//    래퍼 생성자에서도 같은 값을 줘야 한다.
	SetClipping(EWidgetClipping::ClipToBounds);
}

void SLNPScreenMarkers::SetMarkers(TArray<FLNPScreenMarker>&& InMarkers)
{
	Markers = MoveTemp(InMarkers);

	// 위치·개수만 달라지고 희망 크기는 그대로다.
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SLNPScreenMarkers::ClearMarkers()
{
	if (Markers.IsEmpty())
		return;

	Markers.Reset();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SLNPScreenMarkers::SetStyle(const FLNPScreenMarkerStyle* InStyle)
{
	Style = InStyle ? InStyle : &FLNPScreenMarkerStyle::GetDefault();
	Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SLNPScreenMarkers::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D::ZeroVector;
}

int32 SLNPScreenMarkers::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!Style || Markers.IsEmpty())
		return LayerId;

	const FSlateBrush* BackBrush = LNPScreenMarkers::ResolveBrush(Style->BackBrush);
	const FSlateResourceHandle BackHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*BackBrush);
	if (!BackHandle.IsValid())
		return LayerId;

	const bool bDrawFill = Style->bDrawFill;
	const FSlateBrush* FillBrush = bDrawFill ? LNPScreenMarkers::ResolveBrush(Style->FillBrush) : nullptr;
	const FSlateResourceHandle FillHandle = bDrawFill
		? FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FillBrush)
		: FSlateResourceHandle();

	// 부모의 색·투명도를 곱하지 않으면 HUD 전체를 페이드아웃해도 이 위젯만 남는다.
	const FLinearColor ParentTint = InWidgetStyle.GetColorAndOpacityTint();
	const FLinearColor BackBase = Style->BackTint * BackBrush->GetTint(InWidgetStyle) * ParentTint;
	const FLinearColor FillBase = bDrawFill
		? Style->FillTint * FillBrush->GetTint(InWidgetStyle) * ParentTint
		: FLinearColor::White;

	const FVector2f BaseSize(Style->MarkerSize);
	const FVector2f Pivot(Style->Pivot);
	const FSlateRenderTransform& RenderTransform = AllottedGeometry.ToPaintGeometry().GetAccumulatedRenderTransform();

	TArray<FSlateVertex> BackVertices;
	TArray<SlateIndex>   BackIndices;
	TArray<FSlateVertex> FillVertices;
	TArray<SlateIndex>   FillIndices;

	BackVertices.Reserve(Markers.Num() * 4);
	BackIndices.Reserve(Markers.Num() * 6);
	if (bDrawFill)
	{
		FillVertices.Reserve(Markers.Num() * 4);
		FillIndices.Reserve(Markers.Num() * 6);
	}

	for (const FLNPScreenMarker& Marker : Markers)
	{
		const float Alpha = FMath::Clamp(Marker.Alpha, 0.f, 1.f);
		const FVector2f Size = BaseSize * FMath::Max(Marker.Scale, 0.f);
		if (Alpha <= 0.f || Size.X <= 0.f || Size.Y <= 0.f)
			continue;

		const FVector2f TopLeft = Marker.LocalPos - Size * Pivot;

		FLinearColor BackColor = BackBase;
		BackColor.A *= Alpha;
		LNPScreenMarkers::AppendQuad(BackVertices, BackIndices, RenderTransform, TopLeft, Size, 1.f, BackColor.ToFColor(true));

		if (!bDrawFill)
			continue;

		const float Inset = FMath::Min(FMath::Max(Style->FillInset, 0.f), FMath::Min(Size.X, Size.Y) * 0.5f);
		const FVector2f InnerSize(Size.X - Inset * 2.f, Size.Y - Inset * 2.f);
		const float Ratio = FMath::Clamp(Marker.Ratio, 0.f, 1.f);
		if (InnerSize.X <= 0.f || InnerSize.Y <= 0.f || Ratio <= 0.f)
			continue;

		FLinearColor FillColor = FillBase;
		FillColor.A *= Alpha;
		LNPScreenMarkers::AppendQuad(FillVertices, FillIndices, RenderTransform,
			TopLeft + FVector2f(Inset, Inset), FVector2f(InnerSize.X * Ratio, InnerSize.Y), Ratio,
			FillColor.ToFColor(true));
	}

	int32 Layer = LayerId;

	if (!BackIndices.IsEmpty())
	{
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, Layer, BackHandle, BackVertices, BackIndices, nullptr, 0, 0);
	}

	if (!FillIndices.IsEmpty() && FillHandle.IsValid())
	{
		// 채움은 배경 위에 얹힌다 — 레이어를 올리지 않으면 같은 레이어에서 순서가 보장되지 않는다.
		++Layer;
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, Layer, FillHandle, FillVertices, FillIndices, nullptr, 0, 0);
	}

	return Layer + 1;
}
