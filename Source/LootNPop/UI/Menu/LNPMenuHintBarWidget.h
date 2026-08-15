// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/Menu/LNPMenuHint.h"
#include "LNPMenuHintBarWidget.generated.h"

enum class ECommonInputType : uint8;

class UCommonInputSubsystem;
class UDynamicEntryBox;

/**
 * 메뉴 하단 조작 안내 바 (기획 §3 레이아웃의 "하단 액션 바", §8 조작 요약).
 *
 * ⚠️ **UCommonBoundActionBar를 쓰지 않는 이유** — 그 위젯은 액션 라우터에 등록된 바인딩 중
 * bDisplayInActionBar가 켜진 것만 그린다(CommonBoundActionBar.cpp:184). 그런데 기획 §8이 요구하는
 * ✕(선택)와 방향 이동은 **CommonUI 바인딩이 아예 아니다** — Slate 네비게이션이 포커스된 위젯을 직접
 * 처리하는 경로라 액션 라우터에 등록될 바인딩 자체가 없다. 그래서 어떤 플래그를 켜도 표에 나오지 않는다.
 * 표를 전부 그리려면 힌트 목록을 우리가 직접 공급하는 수밖에 없다.
 *
 * 칸 하나는 ULNPMenuHintEntryWidget(WBP)이며 UDynamicEntryBox가 생성·풀링한다.
 * 엔트리 클래스·간격·정렬·배치 방향은 전부 디자이너가 UDynamicEntryBox에서 잡는다 —
 * 이 클래스에는 시각 관련 프로퍼티가 하나도 없다.
 *
 * BP 서브클래스(WBP_LNPMenuHintBar) 요구 사항 — ⚠️ Is Variable 켜기:
 *  - UDynamicEntryBox "HintContainer" (EntryWidgetClass = WBP_LNPMenuHintEntry)
 */
UCLASS()
class LOOTNPOP_API ULNPMenuHintBarWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 힌트 목록을 통째로 교체한다. 메뉴 루트가 탭 활성화·포커스 변경 때마다 호출한다. */
	void SetHints(TArray<FLNPMenuHint> InHints);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDynamicEntryBox> HintContainer;

private:
	/** 캐시된 힌트로 컨테이너를 다시 채운다. 입력 타입이 바뀌면 글리프만 달라지고 라벨은 그대로다. */
	void RebuildEntries();

	/** 힌트 하나의 글리프를 현재 입력 타입으로 해석한다. 해석 불가면 빈 FText. */
	FText ResolveGlyph(const FLNPMenuHint& Hint, const UCommonInputSubsystem* InputSubsystem, bool bGamepad) const;

	void HandleInputMethodChanged(ECommonInputType NewInputType);

	UPROPERTY()
	TArray<FLNPMenuHint> CachedHints;
};
