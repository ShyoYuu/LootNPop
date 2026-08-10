#pragma once

#include "Core/CameraNode.h"
#include "LNPGravityRollCorrectionCameraNode.generated.h"

/**
 * 구면 세계 중력에 맞춰 카메라 자세를 보정하는 Camera Rig 노드.
 *
 * Boom Arm 노드는 붐 피벗 회전을 항상 Roll=0인 월드 Yaw/Pitch로 만들기 때문에,
 * 붐 오프셋과 CameraPose 공간의 카메라 오프셋이 월드 Z-Up 평면 기준으로 적용된다.
 * 이 노드는 붐 피벗의 ForwardVector를 유지한 채 Up 축을 GravityComponent의
 * UpDirection 기준으로 재정렬하고, 그 Roll 델타를 카메라의 회전과
 * (피벗 기준) 위치에 함께 적용해 오프셋이 캐릭터 로컬 평면 위에 놓이게 한다.
 *
 * 배치 위치: Boom Arm 노드 **바로 뒤**. 피벗 조인트를 읽어야 하므로 Boom Arm보다
 * 앞에 두면 안 되고, Dampen Position·Offset 노드보다는 앞에 두어야 한다.
 * 그래야 그 노드들이 기준으로 삼는 CameraPose 회전이 이미 중력 정렬된 상태가 되어
 * 감쇠 축(Vertical)과 카메라 오프셋이 월드 Z-Up이 아닌 중력 Up 기준으로 계산된다.
 */
UCLASS(meta=(CameraNodeCategories="LNP"))
class LOOTNPOP_API ULNPGravityRollCorrectionCameraNode : public UCameraNode
{
	GENERATED_BODY()

protected:
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;
};
