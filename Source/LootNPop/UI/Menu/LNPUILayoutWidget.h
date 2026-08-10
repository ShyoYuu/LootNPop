// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "LNPUILayoutWidget.generated.h"

class UCommonActivatableWidgetStack;
class ULNPMenuRootWidget;

/**
 * 뷰포트에 상주하는 얇은 UI 레이아웃. 메뉴를 담는 UCommonActivatableWidgetStack 하나만 갖는다.
 *
 * 스택을 두는 이유: CommonUI가 활성화/비활성화·포커스 복원·Back 전파를 스택 단위로 관리하므로,
 * 메뉴가 Back으로 스스로 Deactivate하면 스택이 알아서 pop하고 게임으로 포커스를 되돌린다.
 *
 * BP 서브클래스(WBP_LNPUILayout) 요구 사항:
 *  - UCommonActivatableWidgetStack 위젯 이름 "MenuStack" (Is Variable 켜기)
 */
UCLASS()
class LOOTNPOP_API ULNPUILayoutWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 메뉴가 스택에서 내려갔을 때 발송 — PlayerController가 입력·일시정지를 복원한다. */
	DECLARE_MULTICAST_DELEGATE(FOnMenuClosed);
	FOnMenuClosed OnMenuClosed;

	/**
	 * 메뉴를 열고 지정한 탭으로 진입한다. 이미 열려 있으면 탭만 전환한다.
	 * @return 열린 메뉴 위젯. 스택/클래스가 없으면 nullptr.
	 */
	ULNPMenuRootWidget* OpenMenu(TSubclassOf<ULNPMenuRootWidget> MenuClass, FName InitialTabId);

	/** 메뉴를 스택에서 제거한다. 열려 있지 않으면 아무 일도 하지 않는다. */
	void CloseMenu();

	bool IsMenuOpen() const { return CurrentMenu != nullptr; }

	/** 현재 열린 메뉴. OnMenuClosed 통지 중에도 아직 유효하다 (마지막 탭 조회용). */
	ULNPMenuRootWidget* GetMenu() const { return CurrentMenu; }

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> MenuStack;

private:
	/** 메뉴가 Deactivate되어 스택에서 내려갈 때 호출된다 (○ 닫기 경로 포함). */
	void HandleMenuDeactivated();

	UPROPERTY(Transient)
	TObjectPtr<ULNPMenuRootWidget> CurrentMenu;
};
