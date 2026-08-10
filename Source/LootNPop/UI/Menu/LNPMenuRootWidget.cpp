// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPMenuRootWidget.h"
#include "UI/Menu/LNPMenuTabContentWidget.h"
#include "UI/Menu/LNPMenuTabListWidget.h"
#include "CommonActivatableWidgetSwitcher.h"

ULNPMenuRootWidget::ULNPMenuRootWidget()
{
	// 메뉴 전체에서 Back(○)을 받는 유일한 지점. 탭 컨텐츠는 루트를 통해 위임받는다.
	bIsBackHandler = true;
	PendingInitialTabId = TabId_Stats();
}

void ULNPMenuRootWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	EnsureTabsRegistered();

	if (TabList)
		TabList->SelectTabByID(PendingInitialTabId);
}

void ULNPMenuRootWidget::EnsureTabsRegistered()
{
	if (!TabList || !ContentSwitcher)
		return;

	// ⚠️ 열 때마다 확인해야 한다. UCommonActivatableWidgetStack은 위젯을 클래스별로 **풀링·재사용**하는데
	// (CommonActivatableWidgetContainer.cpp의 GeneratedWidgetsPool), 메뉴를 닫으면
	// UCommonTabListWidgetBase::NativeDestruct가 RemoveAllTabs()로 탭을 전부 지운다.
	// 반면 NativeOnInitialized는 인스턴스당 한 번뿐이라, 거기서 등록하면
	// **두 번째 열기부터 탭 바가 빈 채로 남는다**(작은 사각형만 보이고 클릭도 안 먹음).
	if (TabList->GetTabCount() > 0)
		return;

	TabList->SetLinkedSwitcher(ContentSwitcher);

	// 등록 순서가 곧 상단 탭 바의 좌→우 순서이자 L1/R1 이동 순서다.
	TabList->RegisterTab(TabId_Stats(), TabButtonClass, StatsTab);
	TabList->RegisterTab(TabId_Inventory(), TabButtonClass, InventoryTab);
	TabList->RegisterTab(TabId_Settings(), TabButtonClass, SettingsTab);
}

void ULNPMenuRootWidget::SetInitialTab(FName TabId)
{
	PendingInitialTabId = TabId;

	// 이미 열려 있는 메뉴에 다시 진입 요청이 온 경우(예: 메뉴 중 옵션 버튼) 즉시 전환한다.
	if (IsActivated() && TabList)
		TabList->SelectTabByID(TabId);
}

FName ULNPMenuRootWidget::GetRememberableTabId() const
{
	const FName Current = TabList ? TabList->GetActiveTab() : TabId_Stats();
	return (Current == TabId_Settings() || Current.IsNone()) ? TabId_Stats() : Current;
}

bool ULNPMenuRootWidget::NativeOnHandleBackAction()
{
	// 활성 탭이 먼저 소비할 기회를 갖는다 (예: 인벤토리 디테일 → Grid 포커스 복귀).
	if (ULNPMenuTabContentWidget* Active = GetActiveTabContent())
	{
		if (Active->HandleMenuBack())
			return true;
	}

	// 소비되지 않았으면 기본 동작 = Deactivate → UILayout의 스택이 pop한다.
	return Super::NativeOnHandleBackAction();
}

UWidget* ULNPMenuRootWidget::NativeGetDesiredFocusTarget() const
{
	// 포커스는 탭 컨텐츠가 정한다. 스위처가 활성 탭을 Activate하면서 포커스를 넘겨준다.
	if (ULNPMenuTabContentWidget* Active = GetActiveTabContent())
	{
		if (UWidget* TabFocus = Active->GetDesiredFocusTarget())
			return TabFocus;
	}

	return Super::NativeGetDesiredFocusTarget();
}

TOptional<FUIInputConfig> ULNPMenuRootWidget::GetDesiredInputConfig() const
{
	// ⚠️ 기본값인 ECommonInputMode::Menu는 "UI만 입력을 받음"이라 **더 낮은 우선순위의 입력 컴포넌트를
	// 전부 차단**한다(CommonUIInputSettings.h 주석). 그러면 PlayerController의 상시 매핑 컨텍스트에 있는
	// IA_OpenMenu / IA_OpenSettings가 메뉴가 열린 동안 전혀 들어오지 않아 **같은 키로 닫을 수 없다.**
	//
	// All을 쓰면 게임 입력도 함께 살아나지만, 메뉴를 열 때 폰의 DefaultMappingContext를 이미 제거하므로
	// 실제로 살아 있는 게임 입력은 메뉴 열기/닫기 키뿐이다 — 의도한 그대로다.
	return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture, EMouseLockMode::DoNotLock);
}

ULNPMenuTabContentWidget* ULNPMenuRootWidget::GetActiveTabContent() const
{
	return ContentSwitcher ? Cast<ULNPMenuTabContentWidget>(ContentSwitcher->GetActiveWidget()) : nullptr;
}
