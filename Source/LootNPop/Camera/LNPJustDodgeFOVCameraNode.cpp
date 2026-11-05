// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Camera/LNPJustDodgeFOVCameraNode.h"
#include "Camera/LNPCameraNodeUtils.h"
#include "Character/LNPPlayerCharacter.h"
#include "Core/CameraNodeEvaluator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LNPJustDodgeFOVCameraNode)

namespace UE::Cameras
{

class FLNPJustDodgeFOVNodeEvaluator : public FCameraNodeEvaluator
{
	UE_DECLARE_CAMERA_NODE_EVALUATOR(LOOTNPOP_API, FLNPJustDodgeFOVNodeEvaluator)

protected:
	virtual void OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult) override
	{
		SetNodeEvaluatorFlags(ECameraNodeEvaluatorFlags::None);
	}

	virtual void OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult) override
	{
		const ALNPPlayerCharacter* Player = Cast<ALNPPlayerCharacter>(LNPCamera::ResolveOwningCharacter(Params.EvaluationContext));
		if (!Player)
			return;

		const ULNPJustDodgeFOVCameraNode* Node = GetCameraNodeAs<ULNPJustDodgeFOVCameraNode>();
		const float Elapsed = Player->GetJustDodgeCameraPunchElapsed();
		if (Elapsed < 0.f || Elapsed >= Node->RiseDuration + Node->FallDuration)
			return;

		// 양 끝을 부드럽게 잇고, 기준 FOV를 저장·복원하지 않고 매 프레임의 리그 결과에 가산한다.
		const float Alpha = Elapsed < Node->RiseDuration
			? FMath::SmoothStep(0.f, Node->RiseDuration, Elapsed)
			: 1.f - FMath::SmoothStep(0.f, Node->FallDuration, Elapsed - Node->RiseDuration);
		OutResult.CameraPose.SetFieldOfView(OutResult.CameraPose.GetEffectiveFieldOfView(false) + Node->PunchDegrees * Alpha);
		OutResult.CameraPose.SetFocalLength(-1.f);
	}
};

UE_DEFINE_CAMERA_NODE_EVALUATOR(FLNPJustDodgeFOVNodeEvaluator)

} // namespace UE::Cameras

FCameraNodeEvaluatorPtr ULNPJustDodgeFOVCameraNode::OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const
{
	using namespace UE::Cameras;
	return Builder.BuildEvaluator<FLNPJustDodgeFOVNodeEvaluator>();
}
