// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPUILayoutWidget.h"
#include "UI/Menu/LNPMenuRootWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

ULNPMenuRootWidget* ULNPUILayoutWidget::OpenMenu(TSubclassOf<ULNPMenuRootWidget> MenuClass, FName InitialTabId)
{
	if (MenuStack == nullptr || MenuClass == nullptr)
		return nullptr;

	if (CurrentMenu)
	{
		// 이미 열려 있으면 새로 push하지 않고 탭만 바꾼다 (메뉴 중 옵션 버튼 등).
		CurrentMenu->SetInitialTab(InitialTabId);
		return CurrentMenu;
	}

	CurrentMenu = MenuStack->AddWidget<ULNPMenuRootWidget>(MenuClass);
	if (CurrentMenu)
	{
		CurrentMenu->SetInitialTab(InitialTabId);
		CurrentMenu->OnDeactivated().AddUObject(this, &ULNPUILayoutWidget::HandleMenuDeactivated);
	}

	return CurrentMenu;
}

void ULNPUILayoutWidget::CloseMenu()
{
	if (MenuStack && CurrentMenu)
	{
		// RemoveWidget이 Deactivate를 유발하므로 HandleMenuDeactivated가 뒷정리를 맡는다.
		MenuStack->RemoveWidget(*CurrentMenu);
	}
}

void ULNPUILayoutWidget::HandleMenuDeactivated()
{
	if (CurrentMenu == nullptr)
		return;

	// ⚠️ CurrentMenu를 유지한 채 통지한다 — 구독자(PlayerController)가 GetMenu()로
	// 마지막으로 보던 탭을 읽어야 하기 때문. 먼저 비우면 탭 기억이 항상 실패한다.
	OnMenuClosed.Broadcast();
	CurrentMenu = nullptr;
}
