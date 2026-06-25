#include "Camera/LNPGravityRollCorrectionCameraNode.h"
#include "Gravity/LNPPawnGravityComponent.h"

#include "Core/CameraEvaluationContext.h"
#include "Core/CameraNodeEvaluator.h"
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

	if (!OutResult.CameraPose.GetChangedFlags().Rotation)
		return;

	ULNPPawnGravityComponent* GravComp = CachedGravityComponent.Get();
	if (!GravComp)
		return;

	const FVector UpDir   = GravComp->GetUpDirection();
	const FVector Forward = OutResult.CameraPose.GetRotation().Quaternion().GetForwardVector();
	const FVector Right   = FVector::CrossProduct(UpDir, Forward).GetSafeNormal();
	const FVector Up      = FVector::CrossProduct(Forward, Right).GetSafeNormal();

	OutResult.CameraPose.SetRotation(FMatrix(Forward, Right, Up, FVector::ZeroVector).Rotator());
}

} // namespace UE::Cameras

FCameraNodeEvaluatorPtr ULNPGravityRollCorrectionCameraNode::OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const
{
	using namespace UE::Cameras;
	return Builder.BuildEvaluator<FLNPGravityRollCorrectionNodeEvaluator>();
}
