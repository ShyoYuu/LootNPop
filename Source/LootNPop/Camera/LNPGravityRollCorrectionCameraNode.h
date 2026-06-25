#pragma once

#include "Core/CameraNode.h"
#include "LNPGravityRollCorrectionCameraNode.generated.h"

/**
 * 구면 세계 중력에 맞춰 카메라 Roll을 보정하는 Camera Rig 노드.
 * 앞선 노드가 설정한 Rotation의 ForwardVector를 유지하면서
 * Right/Up 축을 현재 GravityComponent의 UpDirection 기준으로 재정렬한다.
 * Camera Rig 그래프의 가장 마지막 노드로 배치한다.
 */
UCLASS(meta=(CameraNodeCategories="LNP"))
class LOOTNPOP_API ULNPGravityRollCorrectionCameraNode : public UCameraNode
{
	GENERATED_BODY()

protected:
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;
};
