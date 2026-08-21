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
 *
 * 배지는 세 모서리로 나뉜다 — 한 칸에 몰아넣으면 레벨과 잔여 시간이 서로를 가린다:
 *  - 좌상단: 장착 표시("E")  - 우상단: 버프 잔여 시간(초, 1초 갱신)  - 우하단: 아이템 레벨
 *
 * BP 서브클래스(WBP_MenuItemCell) 요구 사항 — Is Variable 켜기:
 *  - "IconImage" (UImage)
 *  - "EquipMarkText" · "DurationText" · "LevelText" (UTextBlock) — 아이콘 위 Overlay의 각 모서리에 배치
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

	/** 좌상단 — 장착 중이면 EquippedBadgeText, 아니면 비운다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EquipMarkText;

	/** 우상단 — 버프 잔여 초. 버프가 아니거나 영구 버프면 비운다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DurationText;

	/** 우하단 — 아이템 레벨. 레벨 개념이 없는 아이템(버프)은 비운다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText;

	/** 장착 중인 무기 셀의 배지 문구. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	FText EquippedBadgeText = NSLOCTEXT("LNPMenu", "EquippedBadge", "E");

	/** 레벨 배지 서식. {0} = 레벨. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	FText LevelFormat = NSLOCTEXT("LNPMenu", "ItemLevelBadge", "Lv.{0}");

private:
	/** 세 배지를 모두 다시 쓴다 (항목 바인딩 시). */
	void UpdateBadges();

	/** 잔여 시간 배지만 다시 쓴다. 버프 셀의 1초 반복 타이머가 호출한다. */
	void UpdateDurationText();

	UPROPERTY(Transient)
	TObjectPtr<ULNPInventoryItemInstance> BoundInstance;

	FTimerHandle CountdownTimerHandle;
};
