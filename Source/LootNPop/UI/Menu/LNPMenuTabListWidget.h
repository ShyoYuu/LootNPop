// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonTabListWidgetBase.h"
#include "LNPMenuTabListWidget.generated.h"

class UPanelWidget;

/**
 * 상단 카테고리 탭 바.
 *
 * L1/R1 이동은 베이스가 Next/PreviousTabInputActionData(DT_LNPCommonInputActions의 행)로 처리한다.
 * 이 클래스가 더하는 것은 두 가지다:
 *  1) 생성된 탭 버튼을 실제 패널에 붙이기 — ⚠️ 베이스의 HandleTabCreation_Implementation은 **비어 있어서**,
 *     이걸 구현하지 않으면 버튼이 만들어지기만 하고 화면에 나타나지 않는다.
 *  2) 선택된 탭의 스케일 확대 강조 (기획 §3)
 *
 * ⚠️ BindWidget을 쓰므로 이 클래스는 **위젯 BP(WBP_LNPMenuTabList)로 감싸서** 사용해야 한다.
 * C++ 클래스를 위젯 트리에 직접 넣으면 트리가 비어 있어 TabButtonContainer가 항상 null이다.
 */
UCLASS()
class LOOTNPOP_API ULNPMenuTabListWidget : public UCommonTabListWidgetBase
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * 탭 이동 액션 행. 하단 힌트 바가 "L1/R1" 글리프를 뽑는 데 쓴다.
	 * 베이스의 해당 멤버가 protected라 여기서 열어 준다.
	 */
	const FDataTableRowHandle& GetPreviousTabActionRow() const { return PreviousTabInputActionData; }
	const FDataTableRowHandle& GetNextTabActionRow() const { return NextTabInputActionData; }

protected:
	virtual void NativeOnInitialized() override;

	virtual void HandleTabCreation_Implementation(FName TabNameID, UCommonButtonBase* TabButton) override;
	virtual void HandleTabRemoval_Implementation(FName TabNameID, UCommonButtonBase* TabButton) override;

	/** 탭 버튼이 채워질 가로 컨테이너. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> TabButtonContainer;

	/** 탭 ID → 버튼에 표시할 이름. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu")
	TMap<FName, FText> TabDisplayNames;

	/** 선택 탭에 적용할 렌더 스케일. 1.0이면 강조 없음. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Menu", meta = (ClampMin = "1.0", ClampMax = "2.0"))
	float SelectedTabScale = 1.15f;

private:
	/** 선택이 바뀔 때마다 전체 탭 버튼의 스케일을 다시 칠한다. */
	UFUNCTION()
	void HandleTabSelectedForEmphasis(FName TabId);
};
