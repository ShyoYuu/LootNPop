// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "LNPMenuHint.generated.h"

/**
 * 하단 힌트 바에 한 칸으로 그려질 조작 안내 (기획 §8의 표 한 칸).
 *
 * 글리프 출처는 둘 중 하나다:
 *  - ActionRows가 있으면 CommonUI 입력 액션 행에서 현재 입력 타입의 키를 뽑는다 (○, L1/R1 …)
 *  - 비어 있으면 고정 글리프를 쓴다. 방향 이동처럼 **CommonUI 바인딩이 존재하지 않는** 조작용이다
 *    (방향키·L3는 Slate 네비게이션이라 액션 라우터에 바인딩이 없다).
 */
USTRUCT()
struct FLNPMenuHint
{
	GENERATED_BODY()

	/** 글리프를 뽑을 CommonUI 액션 행. 2개 이상이면 "L1/R1"처럼 이어 붙인다. */
	UPROPERTY()
	TArray<FDataTableRowHandle> ActionRows;

	/** ActionRows가 비었을 때 키보드에서 쓸 고정 글리프. */
	UPROPERTY()
	FText FixedKeyboardGlyph;

	/** ActionRows가 비었을 때 게임패드에서 쓸 고정 글리프. */
	UPROPERTY()
	FText FixedGamepadGlyph;

	/** 조작 설명. 번역 대상이다. */
	UPROPERTY()
	FText Label;
};
