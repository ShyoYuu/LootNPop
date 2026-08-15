// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPMenuHintEntryWidget.h"

#include "CommonTextBlock.h"

void ULNPMenuHintEntryWidget::SetHint(const FText& InGlyph, const FText& InLabel)
{
	if (GlyphText)
	{
		GlyphText->SetText(InGlyph);
	}

	if (LabelText)
	{
		LabelText->SetText(InLabel);
	}
}
