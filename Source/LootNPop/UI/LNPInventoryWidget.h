// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"          // FGameplayAttribute (값 타입으로 사용)
#include "Blueprint/UserWidget.h"
#include "LNPInventoryWidget.generated.h"

struct FOnAttributeChangeData;
class UAbilitySystemComponent;
class ULNPInventoryComponent;
class UListView;
class UTextBlock;

/**
 * 인벤토리 패널 위젯 C++ 기반 클래스.
 *
 * ListView 항목 소스는 MVVM으로 바인딩할 수 없어(UListView::ListItems가 런타임 쓰기 불가)
 * C++가 직접 채운다: InventoryComponent의 OnInventoryChanged를 구독해 보관/버프 목록을 SetItems.
 *
 * BP 서브클래스(WBP_Inventory) 요구 사항:
 *  - ListView 위젯 이름 "StorageList" / "BuffList" (BindWidgetOptional로 자동 연결)
 *  - 각 ListView의 EntryWidgetClass = ULNPInventoryEntryWidget 파생 WBP
 *  - TextBlock 위젯 이름 "StatsText" — 스탯 리드아웃 (BindWidgetOptional)
 *
 * 스탯 리드아웃은 ASC 어트리뷰트 변경 델리게이트를 구독해 갱신한다 — 버프가 모두 합산된
 * **최종값**을 플레이어에게 보여준다. 구독 대상은 GetDisplayedAttributes()가 정의하고,
 * 출력 서식은 UpdateStatsText()가 따로 쓴다 (Health/MaxHealth가 한 줄을 공유하는 등
 * 1:1 대응이 아니라서). 스탯을 추가할 땐 두 곳을 함께 고친다.
 */
UCLASS()
class LOOTNPOP_API ULNPInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * PlayerState의 InventoryComponent·ASC를 전달해 목록 갱신과 스탯 변경을 구독하고
	 * 초기 내용을 채운다.
	 */
	void InitViewModel(ULNPInventoryComponent* InInventory, UAbilitySystemComponent* InASC);

	/** 목록·스탯 구독을 모두 해제한다. 빙의 해제 또는 위젯 소멸 시 호출. */
	void DeinitViewModel();

protected:
	virtual void NativeDestruct() override;

	/** OnInventoryChanged 핸들러 — 두 ListView를 컴포넌트 내용으로 다시 채운다. */
	UFUNCTION()
	void RefreshLists();

	/** ASC의 현재 어트리뷰트 값(= 버프가 모두 합산된 최종값)을 StatsText에 출력한다. */
	void UpdateStatsText();

	/** 구독한 어트리뷰트 중 하나라도 바뀌면 리드아웃 전체를 다시 만든다. */
	void OnStatAttributeChanged(const FOnAttributeChangeData& Data);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> StorageList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> BuffList;

	/** 스탯 리드아웃 — 없으면 스탯 표시만 조용히 생략된다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatsText;

private:
	/** 리드아웃에 표시하며 변경을 구독하는 어트리뷰트 목록. 스탯 추가 시 여기만 고친다. */
	static TArray<FGameplayAttribute> GetDisplayedAttributes();

	TWeakObjectPtr<ULNPInventoryComponent> BoundInventory;

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	/** GetDisplayedAttributes()와 같은 순서의 구독 핸들 — 해제 시 짝을 맞춘다. */
	TArray<FDelegateHandle> AttributeHandles;
};
