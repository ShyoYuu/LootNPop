// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "InputCoreTypes.h"

class UCommonInputSubsystem;
class UInputAction;
class ULocalPlayer;

/**
 * 현재 입력 타입(키보드 / 게임패드)에 맞는 키 심볼을 돌려주는 해석기.
 *
 * 하단 힌트 바(ULNPMenuHintBarWidget)와 인터랙션 프롬프트(ULNPInteractionPromptWidget)가 공유한다.
 *
 * ⚠️ 이 프로젝트에는 UCommonInputBaseControllerData 에셋이 없어서 UCommonActionWidget이 쓸 키 아이콘
 * 브러시가 존재하지 않는다(브러시를 못 찾으면 스스로 Collapsed 된다). 그래서 아이콘 대신 **텍스트 심볼**로 그린다.
 * 나중에 키 글리프 텍스처 세트를 도입하면 **고칠 곳은 이 파일 하나뿐**이 되도록 격리해 두었다.
 */
namespace LNPInputGlyph
{
	/** FKey → 화면에 그릴 짧은 심볼. 큐레이션 테이블에 없으면 Key.GetDisplayName(false)로 폴백한다. */
	FText GetKeyGlyph(const FKey& Key);

	/**
	 * Enhanced Input 액션 → 현재 입력 타입에 실제로 바인딩된 키 → 심볼.
	 * 키를 찾지 못하면 빈 FText를 돌려준다 (호출부가 판단한다).
	 */
	FText GetActionGlyph(const ULocalPlayer* LocalPlayer, const UInputAction* Action);

	/**
	 * CommonUI 입력 액션 행(DT_LNPCommonInputActions) → 현재 입력 타입의 키 → 심볼.
	 * 행이 비었거나 키가 바인딩되지 않았으면 빈 FText를 돌려준다.
	 */
	FText GetActionRowGlyph(const UCommonInputSubsystem* InputSubsystem, const FDataTableRowHandle& ActionRow);
}
