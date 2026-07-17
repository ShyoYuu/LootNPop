// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LNPInventoryWidget.generated.h"

class ULNPInventoryComponent;
class UListView;

/**
 * 인벤토리 패널 위젯 C++ 기반 클래스.
 *
 * ListView 항목 소스는 MVVM으로 바인딩할 수 없어(UListView::ListItems가 런타임 쓰기 불가)
 * C++가 직접 채운다: InventoryComponent의 OnInventoryChanged를 구독해 보관/버프 목록을 SetItems.
 *
 * BP 서브클래스(WBP_Inventory) 요구 사항:
 *  - ListView 위젯 이름 "StorageList" / "BuffList" (BindWidgetOptional로 자동 연결)
 *  - 각 ListView의 EntryWidgetClass = ULNPInventoryEntryWidget 파생 WBP
 */
UCLASS()
class LOOTNPOP_API ULNPInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** PlayerState의 InventoryComponent를 전달해 목록 갱신을 구독하고 초기 목록을 채운다. */
	void InitViewModel(ULNPInventoryComponent* InInventory);

	/** 목록 갱신 구독을 해제한다. 빙의 해제 또는 위젯 소멸 시 호출. */
	void DeinitViewModel();

protected:
	virtual void NativeDestruct() override;

	/** OnInventoryChanged 핸들러 — 두 ListView를 컴포넌트 내용으로 다시 채운다. */
	UFUNCTION()
	void RefreshLists();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> StorageList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> BuffList;

private:
	TWeakObjectPtr<ULNPInventoryComponent> BoundInventory;
};
