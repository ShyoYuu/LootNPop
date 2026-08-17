// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateWidgetStyle.h"
#include "LNPRadialCooldownStyle.generated.h"

/**
 * SLNPRadialCooldown의 룩을 정의하는 스타일.
 *
 * 색·크기를 위젯에 하드코딩하지 않기 위해 분리했다 — 위젯은 "어떻게 보일지"만 알고,
 * 실제 색·크기는 이 구조체를 통해 밖에서 주입된다.
 */
USTRUCT(BlueprintType)
struct LNPUI_API FLNPRadialCooldownStyle : public FSlateWidgetStyle
{
	GENERATED_BODY()

	FLNPRadialCooldownStyle();
	virtual ~FLNPRadialCooldownStyle() override;

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; }
	static const FLNPRadialCooldownStyle& GetDefault();

	/** 브러시를 참조로 노출한다. 누락 시 쿠킹에서 텍스처가 빠질 수 있다. */
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;

	/** 부채꼴을 그릴 브러시. 기본값은 흰색 단색이며, 실제 색은 OverlayTint가 결정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush OverlayBrush;

	/** 가림막 색·투명도. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor OverlayTint = FLinearColor(0.f, 0.f, 0.f, 0.6f);

	/** 스윕이 시작되는 각도 (도). -90 = 12시 방향. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float StartAngleDegrees = -90.f;

	/** true면 시계 방향으로 걷힌다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	bool bClockwise = true;

	/** ComputeDesiredSize가 돌려줄 희망 크기. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FVector2D DesiredSize = FVector2D(64.f, 64.f);
};
