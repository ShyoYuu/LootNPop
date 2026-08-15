// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "LNPMenuRootWidget.generated.h"

class UCommonActivatableWidgetSwitcher;
class UCommonButtonBase;
class ULNPMenuHintBarWidget;
class ULNPMenuTabContentWidget;
class ULNPMenuTabListWidget;

/**
 * 인게임 메뉴의 루트 위젯. 상단 탭 리스트 + 탭 컨텐츠 스위처 + 하단 액션 바를 조립한다.
 *
 * ULNPUILayoutWidget의 UCommonActivatableWidgetStack에 push되며, Back(○) 처리로
 * 스스로 Deactivate하면 스택이 자동으로 pop한다.
 *
 * BP 서브클래스(WBP_LNPMenuRoot) 요구 사항:
 *  - ULNPMenuTabListWidget 파생 위젯 "TabList"
 *  - UCommonActivatableWidgetSwitcher "ContentSwitcher"
 *  - 스위처 자식으로 탭 컨텐츠 3종: "StatsTab" / "InventoryTab" / "SettingsTab"
 *  - ULNPMenuHintBarWidget 파생 위젯 "HintBar" (선택)
 *  - Details의 Tab Button Class에 UCommonButtonBase 파생 WBP 지정
 *  - 모든 바인딩 대상 위젯은 Is Variable을 켜야 한다 (이 프로젝트는 기본 off인 경우가 잦다)
 */
UCLASS()
class LOOTNPOP_API ULNPMenuRootWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	ULNPMenuRootWidget();

	/** 탭 식별자 — 등록·선택·마지막 탭 기억에 공통으로 쓴다. */
	static FName TabId_Stats() { return FName(TEXT("Stats")); }
	static FName TabId_Inventory() { return FName(TEXT("Inventory")); }
	static FName TabId_Settings() { return FName(TEXT("Settings")); }

	/** 활성화 전에 진입할 탭을 지정한다. 이미 활성 상태면 즉시 전환한다. */
	void SetInitialTab(FName TabId);

	/**
	 * 다음 진입 시 복원할 탭. 환경설정은 기억 대상이 아니므로(기획 §2)
	 * 현재 탭이 환경설정이면 캐릭터 스탯을 돌려준다.
	 */
	FName GetRememberableTabId() const;

protected:
	/** 메뉴 전체를 포커스 링 스코프로 감싼다 (구현부 주석 참조). */
	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULNPMenuTabListWidget> TabList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetSwitcher> ContentSwitcher;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULNPMenuTabContentWidget> StatsTab;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULNPMenuTabContentWidget> InventoryTab;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULNPMenuTabContentWidget> SettingsTab;

	/** 하단 조작 안내 바 (기획 §3·§8). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<ULNPMenuHintBarWidget> HintBar;

	/** 탭 버튼으로 생성할 CommonButton 파생 WBP. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	TSubclassOf<UCommonButtonBase> TabButtonClass;

private:
	/**
	 * 탭이 비어 있으면 다시 등록한다. 메뉴를 열 때마다 호출한다 —
	 * 스택이 위젯을 재사용하는 반면 닫을 때 탭이 지워지기 때문(구현부 주석 참조).
	 */
	void EnsureTabsRegistered();

	/** 활성 탭의 힌트 + 공통 힌트(Back·탭 이동)를 모아 하단 바에 넣는다. */
	void RebuildHints();

	/** 포커스 링 강제 여부 — 활성 탭에 위임한다 (구현부 주석 참조). */
	bool IsFocusRingForced() const;

	/** 현재 스위처가 표시 중인 탭 컨텐츠. */
	ULNPMenuTabContentWidget* GetActiveTabContent() const;

	/** 활성화 시점에 선택할 탭. NativeOnInitialized 이전에 지정될 수 있어 보류해 둔다. */
	FName PendingInitialTabId;
};
