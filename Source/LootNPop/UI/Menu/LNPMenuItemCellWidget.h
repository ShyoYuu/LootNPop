// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "LNPMenuItemCellWidget.generated.h"

class ULNPInventoryItemInstance;
class UImage;
class UTextBlock;

/**
 * 인벤토리 Grid의 아이템 셀 (기획 §6-1).
 *
 * CommonButtonBase 파생이므로 방향키·L3 스틱 네비게이션과 ✕(Click) 입력을 CommonUI가 처리한다.
 * 아이콘 하단 배지는 무기면 장착 표시, 버프면 남은 시간(초, 1초 갱신)이다.
 *
 * BP 서브클래스(WBP_MenuItemCell) 요구 사항 — Is Variable 켜기:
 *  - "IconImage" (UImage), "BadgeText" (UTextBlock)
 */
UCLASS()
class LOOTNPOP_API ULNPMenuItemCellWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	/** 이 셀이 클릭(✕)되었을 때 발송 — 인벤토리 탭이 디테일 패널로 포커스를 옮긴다. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCellActivated, ULNPInventoryItemInstance* /*Instance*/);
	FOnCellActivated OnCellActivated;

	ULNPInventoryItemInstance* GetBoundInstance() const { return BoundInstance; }

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnClicked() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	/** 무기: 장착 표시 / 버프: 남은 초. 해당 없으면 비운다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BadgeText;

	/** 장착 중인 무기 셀의 배지 문구. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	FText EquippedBadgeText = NSLOCTEXT("LNPMenu", "EquippedBadge", "E");

private:
	/** 배지를 다시 쓴다. 버프 셀은 1초 반복 타이머가 호출한다. */
	void UpdateBadge();

	UPROPERTY(Transient)
	TObjectPtr<ULNPInventoryItemInstance> BoundInstance;

	FTimerHandle CountdownTimerHandle;
};
