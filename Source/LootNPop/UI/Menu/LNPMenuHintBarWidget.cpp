// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPMenuHintBarWidget.h"

#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/DynamicEntryBox.h"
#include "UI/LNPInputGlyph.h"
#include "UI/Menu/LNPMenuHintEntryWidget.h"

void ULNPMenuHintBarWidget::SetHints(TArray<FLNPMenuHint> InHints)
{
	CachedHints = MoveTemp(InHints);
	RebuildEntries();
}

void ULNPMenuHintBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 입력 기기가 바뀌면 라벨은 그대로 두고 글리프만 다시 그린다.
	if (UCommonInputSubsystem* InputSubsystem = GetInputSubsystem())
	{
		InputSubsystem->OnInputMethodChangedNative.AddUObject(this, &ULNPMenuHintBarWidget::HandleInputMethodChanged);
	}

	RebuildEntries();
}

void ULNPMenuHintBarWidget::NativeDestruct()
{
	if (UCommonInputSubsystem* InputSubsystem = GetInputSubsystem())
	{
		InputSubsystem->OnInputMethodChangedNative.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void ULNPMenuHintBarWidget::HandleInputMethodChanged(ECommonInputType NewInputType)
{
	RebuildEntries();
}

FText ULNPMenuHintBarWidget::ResolveGlyph(const FLNPMenuHint& Hint, const UCommonInputSubsystem* InputSubsystem, bool bGamepad) const
{
	if (Hint.ActionRows.IsEmpty())
	{
		return bGamepad ? Hint.FixedGamepadGlyph : Hint.FixedKeyboardGlyph;
	}

	TArray<FString> Parts;
	Parts.Reserve(Hint.ActionRows.Num());

	for (const FDataTableRowHandle& ActionRow : Hint.ActionRows)
	{
		const FText Glyph = LNPInputGlyph::GetActionRowGlyph(InputSubsystem, ActionRow);
		if (!Glyph.IsEmpty())
		{
			Parts.Add(Glyph.ToString());
		}
	}

	// 행이 지정되지 않았거나 현재 입력 타입에 키가 없으면 칸을 아예 만들지 않는다 (빈 칩 방지).
	return Parts.IsEmpty() ? FText::GetEmpty() : FText::FromString(FString::Join(Parts, TEXT("/")));
}

void ULNPMenuHintBarWidget::RebuildEntries()
{
	if (HintContainer == nullptr)
	{
		return;
	}

	// 기본값(bDeleteWidgets = false)이라 엔트리 위젯은 파괴되지 않고 풀에 남아 재사용된다.
	HintContainer->Reset();

	const UCommonInputSubsystem* InputSubsystem = GetInputSubsystem();
	const bool bGamepad = InputSubsystem != nullptr && InputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad;

	for (const FLNPMenuHint& Hint : CachedHints)
	{
		const FText Glyph = ResolveGlyph(Hint, InputSubsystem, bGamepad);
		if (Glyph.IsEmpty())
		{
			continue;
		}

		if (ULNPMenuHintEntryWidget* Entry = HintContainer->CreateEntry<ULNPMenuHintEntryWidget>())
		{
			Entry->SetHint(Glyph, Hint.Label);
		}
	}
}
