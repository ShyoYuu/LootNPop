// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Widgets/LNPScreenMarkerStyle.h"
#include "Widgets/SLNPScreenMarkers.h"
#include "LNPScreenMarkerWidget.generated.h"

/**
 * SLNPScreenMarkers의 UMG 래퍼.
 * 이 래퍼가 있어야 디자이너 팔레트에 나타나 위젯 BP에서 배치할 수 있다.
 *
 * ⚠️ 화면 좌표를 그대로 받으므로 **Canvas 앵커 (0,0)-(1,1)에 오프셋 0**으로 배치해야 한다 —
 *    그래야 위젯 로컬 원점이 뷰포트 좌상단과 일치한다.
 */
UCLASS()
class LNPUI_API ULNPScreenMarkerWidget : public UWidget
{
	GENERATED_BODY()

public:
	ULNPScreenMarkerWidget();

	/** 이번 프레임에 그릴 마커 전체를 교체한다. 매 프레임 호출되는 것을 전제로 한다. */
	void SetMarkers(TArray<FLNPScreenMarker>&& InMarkers);

	/** 마커를 모두 지운다. */
	void ClearMarkers();

	UPROPERTY(EditAnywhere, Category = Style, meta = (ShowOnlyInnerProperties))
	FLNPScreenMarkerStyle Style;

#if WITH_EDITORONLY_DATA
	/** 디자이너에서 룩을 확인하기 위해 그려 볼 마커 수. 실행 중에는 쓰이지 않는다. */
	UPROPERTY(EditAnywhere, Category = Appearance, meta = (ClampMin = "0", UIMax = "8"))
	int32 PreviewMarkerCount = 3;

	/** 프리뷰 마커의 채움 비율. */
	UPROPERTY(EditAnywhere, Category = Appearance, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreviewRatio = 0.6f;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	TSharedPtr<SLNPScreenMarkers> MyMarkers;
};
