// Copyright (c) 2026 LootNPop. All rights reserved.

#include "UI/Menu/LNPMenuTabContentWidget.h"

#define LOCTEXT_NAMESPACE "LNPMenu"

void ULNPMenuTabContentWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// ⚠️ 힌트 갱신 신호는 **탭 활성화 시점**에 보낸다. UCommonTabListWidgetBase::OnTabSelected를 쓰면 안 된다 —
	// 그 델리게이트는 LinkedSwitcher->SetActiveWidget() 직후에 발화하지만(CommonTabListWidgetBase.cpp:488-509),
	// 새 탭의 ActivateWidget()은 SCommonAnimatedSwitcher 트랜지션(기본 0.4s)이 끝난 뒤
	// HandleSlateActiveIndexChanged에서 비동기로 돈다. 즉 OnTabSelected 시점에는 탭의 내부 포커스 상태가
	// **직전 방문의 잔값**이라, 실제로는 Grid에 포커스가 가는데 "Back to Grid"가 뜨는 버그가 난다.
	OnMenuHintsChanged.Broadcast();
}

FText ULNPMenuTabContentWidget::GetMenuBackHintLabel() const
{
	return LOCTEXT("HintClose", "Close");
}

#undef LOCTEXT_NAMESPACE
