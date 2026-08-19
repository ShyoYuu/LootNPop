// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Widgets/LNPRadialCooldownWidget.h"

#include "Widgets/SLNPRadialCooldown.h"

#define LOCTEXT_NAMESPACE "LNPUI"

ULNPRadialCooldownWidget::ULNPRadialCooldownWidget()
{
	// 부채꼴이 위젯 사각형 밖으로 넘치므로 기본값을 잘라내기로 둔다.
	// UWidget::SynchronizeProperties가 이 값을 Slate 위젯에 밀어 넣는다.
	SetClipping(EWidgetClipping::ClipToBounds);
}

void ULNPRadialCooldownWidget::StartCooldown(float DurationSeconds)
{
	if (MyCooldown.IsValid())
		MyCooldown->StartCooldown(DurationSeconds);
}

void ULNPRadialCooldownWidget::ClearCooldown()
{
	if (MyCooldown.IsValid())
		MyCooldown->ClearCooldown();
}

TSharedRef<SWidget> ULNPRadialCooldownWidget::RebuildWidget()
{
	MyCooldown = SNew(SLNPRadialCooldown)
		.Style(&Style)
		.SegmentCount(SegmentCount);

	return MyCooldown.ToSharedRef();
}

void ULNPRadialCooldownWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!MyCooldown.IsValid())
		return;

	MyCooldown->SetStyle(&Style);
	MyCooldown->SetSegmentCount(SegmentCount);

#if WITH_EDITORONLY_DATA
	// 디자이너에서만 고정 비율로 그린다. 실행 중에는 실제 쿨다운 상태를 따르도록 -1을 준다.
	MyCooldown->SetPreviewRemaining(IsDesignTime() ? PreviewRemaining : -1.f);
#endif
}

void ULNPRadialCooldownWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyCooldown.Reset();
}

#if WITH_EDITOR
const FText ULNPRadialCooldownWidget::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "LNP UI");
}
#endif

#undef LOCTEXT_NAMESPACE
