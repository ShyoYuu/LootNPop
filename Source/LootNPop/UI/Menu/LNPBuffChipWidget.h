// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "LNPBuffChipWidget.generated.h"

class ULNPInventoryItemInstance;
class UImage;
class UTextBlock;

/**
 * 캐릭터 스탯 탭의 "적용 중인 버프" 한 칸 — 아이콘 + 남은 시간(초).
 *
 * 포커스를 받지 않는 순수 표시용이므로 CommonButtonBase가 아니라 CommonUserWidget이다.
 * 잔여 시간은 각 머신이 로컬로 세며(TechDesign_Inventory §5), 1초 반복 타이머로 갱신한다.
 *
 * BP 서브클래스(WBP_BuffChip) 요구 사항: "IconImage"(UImage), "TimeText"(UTextBlock) — Is Variable 켜기
 */
UCLASS()
class LOOTNPOP_API ULNPBuffChipWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 버프 인스턴스를 지정한다. nullptr이면 칩을 비운다. */
	void SetBuffInstance(ULNPInventoryItemInstance* InInstance);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeText;

private:
	/** 남은 시간 텍스트를 다시 쓴다. 무한 지속이면 비운다. */
	void UpdateTimeText();

	UPROPERTY(Transient)
	TObjectPtr<ULNPInventoryItemInstance> BoundInstance;

	FTimerHandle CountdownTimerHandle;
};
