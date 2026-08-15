// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/LNPInputGlyph.h"

#include "CommonInputSubsystem.h"
#include "CommonUITypes.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"

namespace
{
	/**
	 * 엔진 기본 표기를 덮어쓸 심볼 테이블.
	 *
	 * 게임패드는 기획 §1대로 PlayStation 표기를 쓴다 — 엔진의 FKey::GetDisplayName은
	 * "Gamepad Face Button Bottom"처럼 길고 Xbox 계열 명명이라 힌트 바에 쓸 수 없다.
	 * 키보드는 엔진이 이미 짧은 이름을 주므로(Esc, Space) 표기 통일이 필요한 것만 덮는다.
	 *
	 * ⚠️ 심볼은 번역 대상이 아니다. NSLOCTEXT로 만들면 ○·L1 같은 기호가 번역 항목으로 수집되어
	 * 로컬라이제이션 매니페스트를 오염시킨다. 반드시 FText::FromString을 쓴다.
	 *
	 * ⚠️ ✕는 U+2715(Dingbats)가 아니라 U+00D7(×)을 쓴다. U+2715는 Roboto에 없어 폴백 폰트에만
	 * 존재하는데, U+00D7은 Roboto 네이티브라 폰트 폴백 없이도 안전하게 렌더된다.
	 * ○ □ △(Geometric Shapes)는 Roboto에 없지만 /Engine/EngineFonts/Roboto의 CompositeFallbackFont
	 * (DroidSansFallback)가 받아 준다.
	 */
	const TMap<FKey, FText>& GetGlyphOverrides()
	{
		static const TMap<FKey, FText> Overrides =
		{
			// 게임패드 — PlayStation 표기
			{ EKeys::Gamepad_FaceButton_Bottom,  FText::FromString(TEXT("×")) },   // ×
			{ EKeys::Gamepad_FaceButton_Right,   FText::FromString(TEXT("○")) },   // ○
			{ EKeys::Gamepad_FaceButton_Left,    FText::FromString(TEXT("□")) },   // □
			{ EKeys::Gamepad_FaceButton_Top,     FText::FromString(TEXT("△")) },   // △
			{ EKeys::Gamepad_LeftShoulder,       FText::FromString(TEXT("L1")) },
			{ EKeys::Gamepad_RightShoulder,      FText::FromString(TEXT("R1")) },
			{ EKeys::Gamepad_LeftTrigger,        FText::FromString(TEXT("L2")) },
			{ EKeys::Gamepad_RightTrigger,       FText::FromString(TEXT("R2")) },
			{ EKeys::Gamepad_LeftThumbstick,     FText::FromString(TEXT("L3")) },
			{ EKeys::Gamepad_RightThumbstick,    FText::FromString(TEXT("R3")) },
			{ EKeys::Gamepad_Special_Left,       FText::FromString(TEXT("SHARE")) },
			{ EKeys::Gamepad_Special_Right,      FText::FromString(TEXT("OPTIONS")) },
			{ EKeys::Gamepad_DPad_Up,            FText::FromString(TEXT("↑")) },   // ↑
			{ EKeys::Gamepad_DPad_Down,          FText::FromString(TEXT("↓")) },   // ↓
			{ EKeys::Gamepad_DPad_Left,          FText::FromString(TEXT("←")) },   // ←
			{ EKeys::Gamepad_DPad_Right,         FText::FromString(TEXT("→")) },   // →

			// 키보드 — 대문자 표기 통일
			{ EKeys::Escape,                     FText::FromString(TEXT("ESC")) },
			{ EKeys::SpaceBar,                   FText::FromString(TEXT("SPACE")) },
			{ EKeys::Enter,                      FText::FromString(TEXT("ENTER")) },
		};

		return Overrides;
	}
}

FText LNPInputGlyph::GetKeyGlyph(const FKey& Key)
{
	if (!Key.IsValid())
	{
		return FText::GetEmpty();
	}

	if (const FText* Override = GetGlyphOverrides().Find(Key))
	{
		return *Override;
	}

	return Key.GetDisplayName(/*bLongDisplayName=*/false);
}

FText LNPInputGlyph::GetActionGlyph(const ULocalPlayer* LocalPlayer, const UInputAction* Action)
{
	if (LocalPlayer == nullptr || Action == nullptr)
	{
		return FText::GetEmpty();
	}

	const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(LocalPlayer);
	if (InputSubsystem == nullptr)
	{
		return FText::GetEmpty();
	}

	// ⚠️ 이 경로는 QueryKeysMappedToAction으로 **현재 적용 중인** 매핑 컨텍스트만 읽는다.
	// 덕분에 키 리매핑을 자동으로 따라가지만, 컨텍스트가 제거된 동안에는 무효 키가 나온다
	// (메뉴가 열리면 폰의 IMC_Pawn이 통째로 제거된다). 그래서 빈 값을 돌려주고 호출부가 판단하게 한다.
	const FKey Key = CommonUI::GetFirstKeyForInputType(LocalPlayer, InputSubsystem->GetCurrentInputType(), Action);
	return GetKeyGlyph(Key);
}

FText LNPInputGlyph::GetActionRowGlyph(const UCommonInputSubsystem* InputSubsystem, const FDataTableRowHandle& ActionRow)
{
	if (InputSubsystem == nullptr)
	{
		return FText::GetEmpty();
	}

	const FCommonInputActionDataBase* ActionData = CommonUI::GetInputActionData(ActionRow);
	if (ActionData == nullptr)
	{
		return FText::GetEmpty();
	}

	// GetCurrentInputTypeInfo가 게임패드 종류별 오버라이드 맵까지 내부에서 처리한다.
	return GetKeyGlyph(ActionData->GetCurrentInputTypeInfo(InputSubsystem).GetKey());
}
