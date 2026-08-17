// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Widgets/LNPRadialCooldownStyle.h"
#include "LNPRadialCooldownWidget.generated.h"

class SLNPRadialCooldown;

/**
 * SLNPRadialCooldown의 UMG 래퍼.
 * 이 래퍼가 있어야 디자이너 팔레트에 나타나 위젯 BP에서 배치할 수 있다.
 */
UCLASS()
class LNPUI_API ULNPRadialCooldownWidget : public UWidget
{
	GENERATED_BODY()

public:
	ULNPRadialCooldownWidget();

	/** 쿨다운을 시작한다. 진행 중에 다시 호출하면 처음부터 재시작한다. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Cooldown")
	void StartCooldown(float DurationSeconds);

	/** 진행 중인 쿨다운을 즉시 지운다. */
	UFUNCTION(BlueprintCallable, Category = "LNP|Cooldown")
	void ClearCooldown();

	UPROPERTY(EditAnywhere, Category = Style, meta = (ShowOnlyInnerProperties))
	FLNPRadialCooldownStyle Style;

	/** 호를 근사할 최대 세그먼트 수. 남은 각에 비례해 실제 사용량은 줄어든다. */
	UPROPERTY(EditAnywhere, Category = Appearance, meta = (ClampMin = "3", UIMin = "3", UIMax = "128"))
	int32 SegmentCount = 64;

#if WITH_EDITORONLY_DATA
	/** 디자이너에서 형태를 확인하기 위한 표시 비율. 실행 중에는 쓰이지 않는다. */
	UPROPERTY(EditAnywhere, Category = Appearance, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreviewRemaining = 0.75f;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	TSharedPtr<SLNPRadialCooldown> MyCooldown;
};
