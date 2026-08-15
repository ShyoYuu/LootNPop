// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "UI/Menu/LNPMenuHint.h"
#include "LNPMenuTabContentWidget.generated.h"

/**
 * 인게임 메뉴 탭 컨텐츠의 공통 베이스.
 *
 * 탭 컨텐츠는 ULNPMenuRootWidget의 UCommonActivatableWidgetSwitcher 안에 놓이며,
 * 탭 전환 시 스위처가 활성/비활성을 자동 전환한다 (포커스 이동도 여기에 딸려온다).
 *
 * ⚠️ 탭 컨텐츠는 스스로 Back 액션을 바인딩하지 않는다(bIsBackHandler = false 유지).
 * ○ 버튼은 메뉴 루트 하나만 받고, 루트가 활성 탭에게 HandleMenuBack()을 먼저 물어본다.
 * 탭이 각자 Back을 바인딩하면 "Grid 포커스에서는 메뉴가 닫혀야 한다"는 규칙을
 * 액션 바인딩 스택이 가로채 버리기 때문이다.
 */
UCLASS(Abstract)
class LOOTNPOP_API ULNPMenuTabContentWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnMenuHintsChanged);

	/** 이 탭이 노출할 힌트가 바뀌었음을 알린다. 메뉴 루트가 듣고 하단 힌트 바를 다시 만든다. */
	FOnMenuHintsChanged OnMenuHintsChanged;

	/**
	 * 메뉴 루트가 Back(○)을 받았을 때 활성 탭에게 먼저 처리 기회를 준다.
	 * @return true면 탭이 소비했으므로 메뉴를 닫지 않는다. false면 메뉴가 닫힌다.
	 */
	virtual bool HandleMenuBack() { return false; }

	/**
	 * 이 탭에서만 유효한 조작 힌트. 기본은 없음 —
	 * 기획 §8 표에서 캐릭터 스탯·환경설정 탭 행은 탭 이동과 닫기를 빼면 전부 "—"다.
	 * 탭 이동·Back 힌트는 루트가 공통으로 덧붙이므로 여기에 넣지 않는다.
	 */
	virtual void GetMenuHints(TArray<FLNPMenuHint>& OutHints) const {}

	/** Back(○) 힌트에 붙일 라벨. 하위 포커스가 있는 탭은 "Back" 등으로 바꾼다. */
	virtual FText GetMenuBackHintLabel() const;

	/**
	 * 이 탭에서 포커스 링을 강제할지 (§3.6).
	 * 포커스가 실제로 앉을 대상이 없는 빈 상태에서는 꺼서, 링이 컨테이너 전체를 감싸는 걸 막는다.
	 */
	virtual bool ShouldForceFocusRing() const { return true; }

protected:
	virtual void NativeOnActivated() override;
};
