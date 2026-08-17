// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Widgets/LNPRadialCooldownStyle.h"

const FName FLNPRadialCooldownStyle::TypeName(TEXT("FLNPRadialCooldownStyle"));

FLNPRadialCooldownStyle::FLNPRadialCooldownStyle()
{
	// 브러시는 색만 내는 단색으로 둔다 — 실제 색은 OverlayTint가 결정한다.
	OverlayBrush.DrawAs = ESlateBrushDrawType::Image;
	OverlayBrush.TintColor = FSlateColor(FLinearColor::White);
}

FLNPRadialCooldownStyle::~FLNPRadialCooldownStyle() = default;

const FLNPRadialCooldownStyle& FLNPRadialCooldownStyle::GetDefault()
{
	static FLNPRadialCooldownStyle Default;
	return Default;
}

void FLNPRadialCooldownStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	OutBrushes.Add(&OverlayBrush);
}
