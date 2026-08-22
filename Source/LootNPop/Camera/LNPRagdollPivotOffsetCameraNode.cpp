#include "Camera/LNPRagdollPivotOffsetCameraNode.h"
#include "Camera/LNPCameraNodeUtils.h"

#include "Core/CameraNodeEvaluator.h"
#include "Core/CameraParameterReader.h"
#include "Core/CameraSystemEvaluator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LNPRagdollPivotOffsetCameraNode)

namespace UE::Cameras
{

class FLNPRagdollPivotOffsetNodeEvaluator : public FCameraNodeEvaluator
{
	UE_DECLARE_CAMERA_NODE_EVALUATOR(LOOTNPOP_API, FLNPRagdollPivotOffsetNodeEvaluator)

protected:
	virtual void OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult) override;
	virtual void OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult) override;

private:
	TCameraParameterReader<double> OffsetReader;
};

UE_DEFINE_CAMERA_NODE_EVALUATOR(FLNPRagdollPivotOffsetNodeEvaluator)

void FLNPRagdollPivotOffsetNodeEvaluator::OnInitialize(
	const FCameraNodeEvaluatorInitializeParams& Params,
	FCameraNodeEvaluationResult& OutResult)
{
	SetNodeEvaluatorFlags(ECameraNodeEvaluatorFlags::None);

	OffsetReader.Initialize(GetCameraNodeAs<ULNPRagdollPivotOffsetCameraNode>()->PivotUpOffset);
}

void FLNPRagdollPivotOffsetNodeEvaluator::OnRun(
	const FCameraNodeEvaluationParams& Params,
	FCameraNodeEvaluationResult& OutResult)
{
#if WITH_EDITOR
	if (Params.Evaluator && Params.Evaluator->GetRole() == ECameraSystemEvaluatorRole::EditorPreview)
		return;
#endif

	const ALNPCharacterBase* Character = LNPCamera::ResolveOwningCharacter(Params.EvaluationContext);
	if (!Character || !Character->IsRagdollActive())
		return;

	const double UpOffset = OffsetReader.Get(OutResult.VariableTable);
	if (FMath::IsNearlyZero(UpOffset))
		return;

	// BoomArm이 아직 돌지 않았으므로, 여기서 옮긴 위치가 곧 궤도 회전의 중심이 된다.
	OutResult.CameraPose.SetLocation(OutResult.CameraPose.GetLocation() + Character->GetUpDirection() * UpOffset);
}

} // namespace UE::Cameras

FCameraNodeEvaluatorPtr ULNPRagdollPivotOffsetCameraNode::OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const
{
	using namespace UE::Cameras;
	return Builder.BuildEvaluator<FLNPRagdollPivotOffsetNodeEvaluator>();
}
