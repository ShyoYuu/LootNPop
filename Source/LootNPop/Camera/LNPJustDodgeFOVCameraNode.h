// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Core/CameraNode.h"
#include "LNPJustDodgeFOVCameraNode.generated.h"

/** 기본 FOV 설정 뒤에 배치한다. 성공한 본인의 시야만 잠깐 넓히며 ADS에서는 적용하지 않는다. */
UCLASS(meta=(CameraNodeCategories="LNP"))
class LOOTNPOP_API ULNPJustDodgeFOVCameraNode : public UCameraNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "LNP", meta = (ClampMin = "0.0"))
	float PunchDegrees = 3.f;

	UPROPERTY(EditAnywhere, Category = "LNP", meta = (ClampMin = "0.001"))
	float RiseDuration = 0.04f;

	UPROPERTY(EditAnywhere, Category = "LNP", meta = (ClampMin = "0.001"))
	float FallDuration = 0.16f;

protected:
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;
};
