#pragma once

#include "Core/CameraNode.h"
#include "Core/CameraParameters.h"
#include "LNPRagdollPivotOffsetCameraNode.generated.h"

/**
 * 폰이 랙돌일 때만 **카메라 궤도 회전의 기준점(피벗)을 시체 쪽으로 내리는** Camera Rig 노드.
 *
 * 사망 중 카메라 컴포넌트는 매 프레임 랙돌 골반으로 따라간다
 * (`ALNPPlayerCharacter::TickDeathCameraFollow`). 그런데 그 뒤의 `Offset`(붐암오프셋) 노드가
 * 피벗을 **선 캐릭터 기준 높이**로 들어올리기 때문에, 바닥에 누운 시체 위로 피벗이 붕 뜬다.
 * Look 입력으로 카메라를 돌리면 시체가 아니라 그 허공을 중심으로 도는 셈이다.
 * 이 노드가 랙돌 동안 그 높이를 상쇄한다.
 *
 * **위치를 옮기지 회전을 주지 않는다.** 피벗 중심으로 카메라를 굴리면(= 회전) 시선 각도만 바뀌고
 * 회전 기준점은 그대로다 — 초기 구현이 그 실수를 했다.
 *
 * 배치 위치: **`BoomArm` 바로 앞.** BoomArm이 이 결과를 피벗으로 삼아 궤도를 돌아야 한다.
 * (`GravityRollCorrection`처럼 BoomArm 뒤에 두면 피벗은 이미 확정된 뒤라 아무 효과가 없다.)
 */
UCLASS(meta=(CameraNodeCategories="LNP"))
class LOOTNPOP_API ULNPRagdollPivotOffsetCameraNode : public UCameraNode
{
	GENERATED_BODY()

public:
	/**
	 * 랙돌 중 피벗을 캐릭터 Up 방향으로 이동시킬 거리(cm). **음수 = 아래(시체 쪽).**
	 * 리그의 `BoomArmOffset` 높이만큼 빼주는 값이 출발점이다. 0이면 노드가 아무것도 하지 않는다.
	 *
	 * 월드 Z가 아니라 캐릭터의 중력 Up 기준이라 구 내벽 어디서든 같게 동작한다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP")
	FDoubleCameraParameter PivotUpOffset = FDoubleCameraParameter(-60.0);

protected:
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;
};
