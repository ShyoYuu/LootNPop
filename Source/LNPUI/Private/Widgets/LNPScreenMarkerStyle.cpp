// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Widgets/LNPScreenMarkerStyle.h"

const FName FLNPScreenMarkerStyle::TypeName(TEXT("FLNPScreenMarkerStyle"));

FLNPScreenMarkerStyle::FLNPScreenMarkerStyle()
{
	BackBrush.DrawAs = ESlateBrushDrawType::Image;
	BackBrush.TintColor = FSlateColor(FLinearColor::White);

	FillBrush.DrawAs = ESlateBrushDrawType::Image;
	FillBrush.TintColor = FSlateColor(FLinearColor::White);
}

FLNPScreenMarkerStyle::~FLNPScreenMarkerStyle() = default;

const FLNPScreenMarkerStyle& FLNPScreenMarkerStyle::GetDefault()
{
	static FLNPScreenMarkerStyle Default;
	return Default;
}

void FLNPScreenMarkerStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	OutBrushes.Add(&BackBrush);
	OutBrushes.Add(&FillBrush);
}
