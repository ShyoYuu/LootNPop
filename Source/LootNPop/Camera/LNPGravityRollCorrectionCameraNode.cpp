#include "Camera/LNPGravityRollCorrectionCameraNode.h"
#include "Gravity/LNPPawnGravityComponent.h"

#include "Core/BuiltInCameraVariables.h"
#include "Core/CameraEvaluationContext.h"
#include "Core/CameraNodeEvaluator.h"
#include "Core/CameraRigJoints.h"
#include "Core/CameraSystemEvaluator.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LNPGravityRollCorrectionCameraNode)

namespace UE::Cameras
{

class FLNPGravityRollCorrectionNodeEvaluator : public FCameraNodeEvaluator
{
	UE_DECLARE_CAMERA_NODE_EVALUATOR(LOOTNPOP_API, FLNPGravityRollCorrectionNodeEvaluator)

protected:
	virtual void OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult) override;
	virtual void OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult) override;

private:
	TWeakObjectPtr<ULNPPawnGravityComponent> CachedGravityComponent;
};

UE_DEFINE_CAMERA_NODE_EVALUATOR(FLNPGravityRollCorrectionNodeEvaluator)

void FLNPGravityRollCorrectionNodeEvaluator::OnInitialize(
	const FCameraNodeEvaluatorInitializeParams& Params,
	FCameraNodeEvaluationResult& OutResult)
{
	SetNodeEvaluatorFlags(ECameraNodeEvaluatorFlags::None);

	APlayerController* PC = Params.EvaluationContext ? Params.EvaluationContext->GetPlayerController() : nullptr;
	if (APawn* Pawn = PC ? PC->GetPawnOrSpectator() : nullptr)
	{
		CachedGravityComponent = Pawn->FindComponentByClass<ULNPPawnGravityComponent>();
	}
}

void FLNPGravityRollCorrectionNodeEvaluator::OnRun(
	const FCameraNodeEvaluationParams& Params,
	FCameraNodeEvaluationResult& OutResult)
{
#if WITH_EDITOR
	if (Params.Evaluator && Params.Evaluator->GetRole() == ECameraSystemEvaluatorRole::EditorPreview)
		return;
#endif

	ULNPPawnGravityComponent* GravComp = CachedGravityComponent.Get();
	if (!GravComp)
		return;

	// Boom Arm 노드가 발행한 Yaw/Pitch 조인트(= 붐 피벗)를 찾는다.
	const FCameraVariableDefinition& YawPitchDef = FBuiltInCameraVariables::Get().YawPitchDefinition;
	const FCameraRigJoint* PivotJoint = OutResult.CameraRigJoints.GetJoints().FindByPredicate(
		[&YawPitchDef](const FCameraRigJoint& Joint) { return Joint.VariableID == YawPitchDef; });
	if (!PivotJoint)
		return;

	// FBoomArmCameraNodeEvaluator::ComputeBoomRotation은 붐 피벗 회전을 Roll=0인 월드 Yaw/Pitch로
	// 만든다. 즉 붐 오프셋과 (CameraPose 공간의) 카메라 오프셋이 항상 월드 Z-Up 평면 기준으로
	// 적용되므로, 구면 중력에서 중력 Up이 월드 Z와 어긋날수록 카메라가 캐릭터 기준 옆·아래로 밀린다.
	// 여기서 전방 축은 유지한 채 Up 축만 중력 기준으로 재정렬한 피벗 회전을 구한다.
	const FQuat PivotRot = PivotJoint->Transform.GetRotation();
	const FVector PivotForward = PivotRot.GetForwardVector();
	const FVector UpDir = GravComp->GetUpDirection();
	const FVector PivotRight = FVector::CrossProduct(UpDir, PivotForward).GetSafeNormal();
	if (PivotRight.IsNearlyZero())
		return;  // 시선이 중력축과 평행 (ControlRotation Pitch 클램프로 정상적으로는 발생하지 않음)

	const FVector PivotUp = FVector::CrossProduct(PivotForward, PivotRight);
	const FQuat CorrectedPivotRot = FMatrix(PivotForward, PivotRight, PivotUp, FVector::ZeroVector).ToQuat();

	// 피벗 전방 축을 중심으로 하는 순수 Roll 델타. 회전뿐 아니라 피벗 기준 위치에도 적용해야
	// 붐/카메라 오프셋이 캐릭터 로컬(중력) 평면 기준으로 놓인다.
	const FQuat RollDelta = CorrectedPivotRot * PivotRot.Inverse();
	const FVector PivotLoc = PivotJoint->Transform.GetLocation();

	OutResult.CameraPose.SetLocation(PivotLoc + RollDelta.RotateVector(OutResult.CameraPose.GetLocation() - PivotLoc));
	OutResult.CameraPose.SetRotation((RollDelta * OutResult.CameraPose.GetRotation().Quaternion()).Rotator());
}

} // namespace UE::Cameras

FCameraNodeEvaluatorPtr ULNPGravityRollCorrectionCameraNode::OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const
{
	using namespace UE::Cameras;
	return Builder.BuildEvaluator<FLNPGravityRollCorrectionNodeEvaluator>();
}
