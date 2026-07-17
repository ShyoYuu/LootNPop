// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "LNPInventoryEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UButton;
class ULNPInventoryItemInstance;

/**
 * 인벤토리 ListView 엔트리 위젯 (보관 아이템·버프 공용).
 *
 * ListView가 항목(ULNPInventoryItemInstance)을 설정하면 정의의 아이콘·이름을 채우고,
 * 드랍/장착 버튼을 소유 폰의 함수에 ItemId/인스턴스로 연결한다.
 * BP 서브클래스에서 아래 이름의 위젯을 배치하면 자동 바인딩된다 (모두 Optional):
 *  IconImage(Image) / NameText(TextBlock) / DetailText(TextBlock, 버프 잔여·레벨) /
 *  DropButton(Button) / EquipButton(Button)
 */
UCLASS()
class LOOTNPOP_API ULNPInventoryEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UFUNCTION()
	void OnDropClicked();

	UFUNCTION()
	void OnEquipClicked();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DropButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> EquipButton;

private:
	/** 이 엔트리가 대표하는 아이템 인스턴스 (드랍/장착 대상, ItemId로 서버에 전달). */
	UPROPERTY()
	TObjectPtr<ULNPInventoryItemInstance> BoundInstance;
};
