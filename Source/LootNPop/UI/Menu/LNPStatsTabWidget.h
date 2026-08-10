// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menu/LNPMenuTabContentWidget.h"
#include "LNPStatsTabWidget.generated.h"

class ULNPBuffChipWidget;
class ULNPInventoryComponent;
class ULNPStatsViewModel;
class UImage;
class UPanelWidget;

/**
 * 캐릭터 스탯 탭 (기획 §5). 세 영역을 위에서 아래로 표시한다.
 *  1) 스탯 리드아웃 — ULNPStatsViewModel이 만든 RichText를 MVVM으로 바인딩
 *  2) 장착 장비   — 무기 슬롯 아이콘 1칸
 *  3) 적용 중인 버프 — 아이콘 + 남은 시간 칩 나열
 *
 * 스탯은 ASC 델리게이트로, 장비·버프는 InventoryComponent의 OnInventoryChanged로 갱신한다.
 * (장착 상태 변경도 OnInventoryChanged를 브로드캐스트한다 — TechDesign_Inventory §4)
 *
 * BP 서브클래스(WBP_MenuTab_Stats) 요구 사항 — 모두 Is Variable 켜기:
 *  - View Model 패널에 ULNPStatsViewModel 등록(생성 모드 Manual) 후
 *    CommonRichTextBlock.Text ← StatsRichText 바인딩
 *  - "WeaponIcon" (UImage)
 *  - "BuffContainer" (UHorizontalBox 등 UPanelWidget) + Details의 Buff Chip Class 지정
 */
UCLASS()
class LOOTNPOP_API ULNPStatsTabWidget : public ULNPMenuTabContentWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/** 무기 슬롯 아이콘. 비어 있으면 EmptySlotIcon으로 대체한다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> WeaponIcon;

	/** 버프 칩이 채워질 컨테이너. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> BuffContainer;

	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	TSubclassOf<ULNPBuffChipWidget> BuffChipClass;

	/** 무기 슬롯이 비었을 때 표시할 플레이스홀더 텍스처 (선택). */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	TObjectPtr<UTexture2D> EmptySlotIcon;

private:
	/** OnInventoryChanged 핸들러 — 장비 아이콘과 버프 칩을 다시 만든다. */
	UFUNCTION()
	void RefreshEquipmentAndBuffs();

	void UpdateWeaponIcon();
	void RebuildBuffChips();

	UPROPERTY(Transient)
	TObjectPtr<ULNPStatsViewModel> StatsViewModel;

	TWeakObjectPtr<ULNPInventoryComponent> BoundInventory;
};
