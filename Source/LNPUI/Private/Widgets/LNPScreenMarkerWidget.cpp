// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Widgets/LNPScreenMarkerWidget.h"

#define LOCTEXT_NAMESPACE "LNPUI"

ULNPScreenMarkerWidget::ULNPScreenMarkerWidget()
{
	// 화면 가장자리에 걸친 마커가 위젯 밖으로 넘치지 않게 한다.
	// UWidget::SynchronizeProperties가 이 값을 Slate 위젯에 밀어 넣으므로, Slate 쪽에만 걸면 UMG에서 안 먹는다.
	SetClipping(EWidgetClipping::ClipToBounds);

	// 마커는 클릭 대상이 아니다 — 아래 위젯의 입력을 가로채지 않게 한다.
	SetVisibilityInternal(ESlateVisibility::HitTestInvisible);
}

void ULNPScreenMarkerWidget::SetMarkers(TArray<FLNPScreenMarker>&& InMarkers)
{
	if (MyMarkers.IsValid())
		MyMarkers->SetMarkers(MoveTemp(InMarkers));
}

void ULNPScreenMarkerWidget::ClearMarkers()
{
	if (MyMarkers.IsValid())
		MyMarkers->ClearMarkers();
}

TSharedRef<SWidget> ULNPScreenMarkerWidget::RebuildWidget()
{
	MyMarkers = SNew(SLNPScreenMarkers)
		.Style(&Style);

	return MyMarkers.ToSharedRef();
}

void ULNPScreenMarkerWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!MyMarkers.IsValid())
		return;

	MyMarkers->SetStyle(&Style);

#if WITH_EDITORONLY_DATA
	// 실행 중에는 건드리지 않는다 — 매 프레임 밀어 넣는 마커를 지워버리게 된다.
	if (!IsDesignTime())
		return;

	TArray<FLNPScreenMarker> PreviewMarkers;
	PreviewMarkers.Reserve(FMath::Max(PreviewMarkerCount, 0));
	for (int32 i = 0; i < PreviewMarkerCount; ++i)
	{
		FLNPScreenMarker& Marker = PreviewMarkers.AddDefaulted_GetRef();
		Marker.LocalPos = FVector2f(120.f + i * 160.f, 120.f);
		Marker.Ratio    = PreviewRatio;
	}

	MyMarkers->SetMarkers(MoveTemp(PreviewMarkers));
#endif
}

void ULNPScreenMarkerWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyMarkers.Reset();
}

#if WITH_EDITOR
const FText ULNPScreenMarkerWidget::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "LNP UI");
}
#endif

#undef LOCTEXT_NAMESPACE
