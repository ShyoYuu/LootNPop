// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPMenuRootWidget.h"
#include "UI/Menu/LNPMenuHintBarWidget.h"
#include "UI/Menu/LNPMenuTabContentWidget.h"
#include "UI/Menu/LNPMenuTabListWidget.h"
#include "CommonActivatableWidgetSwitcher.h"
#include "ICommonInputModule.h"
#include "Widgets/SCompoundWidget.h"

#define LOCTEXT_NAMESPACE "LNPMenu"

namespace
{
	/**
	 * 자기 아래의 모든 포커스 이동에 대해 Slate 포커스 링을 그리게 만드는 패스스루 위젯.
	 *
	 * ⚠️ **왜 필요한가** — Slate는 포커스 링(파란 사각형, `FocusRectangle` 브러시)을 그릴지를
	 * `ShowFocus = (InCause == EFocusCause::Navigation)`으로 판정한다(`SlateApplication.cpp:3099`).
	 * 즉 **코드가 옮긴 포커스(`EFocusCause::SetDirectly`)에는 링이 그려지지 않는다.**
	 * 사용자가 방향키를 눌러 스스로 옮긴 뒤에야 나타난다.
	 *
	 * 이 메뉴는 포커스를 코드로 옮기는 지점이 여럿이다 — CommonUI가 탭 활성화 때 부르는
	 * `DesiredTarget->SetFocus()`(`UIActionRouterTypes.cpp:1872`), 인벤토리의 Grid↔디테일 전환 등.
	 * 그대로 두면 "포커스는 갔는데 어디 있는지 안 보이는" 상태가 된다.
	 *
	 * 다행히 판정 루프는 포커스 경로를 **말단→루트** 순으로 훑으며 값을 돌려주는 **첫 위젯**에 따르고,
	 * `SWidget::OnQueryShowFocus`의 기본 구현은 빈 `TOptional`이다(`SWidget.cpp:623`).
	 * 따라서 메뉴 최상단에 이 위젯 하나만 두면 그 아래 전부에 일괄 적용된다.
	 *
	 * `SetDirectly`만 손대고 나머지 원인은 엔진 기본에 위임한다 — 마우스 클릭 동작은 그대로다.
	 */
	class SLNPFocusRingScope : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SLNPFocusRingScope) {}
			/** false면 이번 포커스 이동에는 링을 강제하지 않고 엔진 기본에 맡긴다. */
			SLATE_ATTRIBUTE(bool, ForceShowFocus)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			ForceShowFocus = InArgs._ForceShowFocus;

			ChildSlot
			[
				InArgs._Content.Widget
			];
		}

		virtual TOptional<bool> OnQueryShowFocus(const EFocusCause InFocusCause) const override
		{
			if (InFocusCause == EFocusCause::SetDirectly && ForceShowFocus.Get(true))
				return true;

			return TOptional<bool>();
		}

	private:
		TAttribute<bool> ForceShowFocus;
	};
}

TSharedRef<SWidget> ULNPMenuRootWidget::RebuildWidget()
{
	return SNew(SLNPFocusRingScope)
		.ForceShowFocus(TAttribute<bool>::Create(
			TAttribute<bool>::FGetter::CreateUObject(this, &ULNPMenuRootWidget::IsFocusRingForced)))
	[
		Super::RebuildWidget()
	];
}

bool ULNPMenuRootWidget::IsFocusRingForced() const
{
	// 포커스 링을 강제할지는 활성 탭이 정한다 — 포커스가 앉을 대상이 없는 탭도 있다.
	const ULNPMenuTabContentWidget* Active = GetActiveTabContent();
	return Active == nullptr || Active->ShouldForceFocusRing();
}

ULNPMenuRootWidget::ULNPMenuRootWidget()
{
	// 메뉴 전체에서 Back(○)을 받는 유일한 지점. 탭 컨텐츠는 루트를 통해 위임받는다.
	bIsBackHandler = true;
	PendingInitialTabId = TabId_Stats();
}

void ULNPMenuRootWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// ⚠️ 힌트 구독은 반드시 여기서 한다. NativeOnActivated에서 하면 늦다 —
	// 루트의 Super::NativeOnActivated() 안에서 스위처가 탭을 활성화하며
	// 탭의 OnMenuHintsChanged 브로드캐스트가 이미 지나가 버린다.
	for (ULNPMenuTabContentWidget* Tab : { StatsTab.Get(), InventoryTab.Get(), SettingsTab.Get() })
	{
		if (Tab)
			Tab->OnMenuHintsChanged.AddUObject(this, &ULNPMenuRootWidget::RebuildHints);
	}
}

void ULNPMenuRootWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	EnsureTabsRegistered();

	if (TabList)
		TabList->SelectTabByID(PendingInitialTabId);

	// 재진입처럼 탭 인덱스가 그대로여서 스위처가 아무 이벤트도 내지 않는 경우의 안전망.
	RebuildHints();
}

void ULNPMenuRootWidget::RebuildHints()
{
	if (HintBar == nullptr)
		return;

	ULNPMenuTabContentWidget* Active = GetActiveTabContent();

	TArray<FLNPMenuHint> Hints;

	// 순서는 DT_LNPCommonInputActions의 NavBarPriority(Back 10 < Click 20 < Tab 30/40)와 기획 §3 목업을 따른다.
	FLNPMenuHint BackHint;
	BackHint.ActionRows = { ICommonInputModule::GetSettings().GetDefaultBackAction() };
	BackHint.Label = Active ? Active->GetMenuBackHintLabel() : LOCTEXT("HintClose", "Close");
	Hints.Add(MoveTemp(BackHint));

	if (Active)
		Active->GetMenuHints(Hints);

	// 탭 이동은 어느 탭에서나 가능하다 (기획 §8).
	if (TabList)
	{
		FLNPMenuHint TabHint;
		TabHint.ActionRows = { TabList->GetPreviousTabActionRow(), TabList->GetNextTabActionRow() };
		TabHint.Label = LOCTEXT("HintTabMove", "Tab");
		Hints.Add(MoveTemp(TabHint));
	}

	HintBar->SetHints(MoveTemp(Hints));
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

#undef LOCTEXT_NAMESPACE
