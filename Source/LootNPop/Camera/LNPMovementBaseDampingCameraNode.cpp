// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Camera/LNPMovementBaseDampingCameraNode.h"
#include "Camera/LNPCameraNodeUtils.h"
#include "DynamicTerrain/LNPMovingPanel.h"
#include "Movement/LNPCharacterMoverComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Core/CameraNodeEvaluator.h"
#include "Nodes/Common/DampenPositionCameraNode.h"
#include "UObject/ObjectKey.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LNPMovementBaseDampingCameraNode)

namespace UE::Cameras
{

class FLNPMovementBaseDampingEvaluator : public FCameraNodeEvaluator
{
	UE_DECLARE_CAMERA_NODE_EVALUATOR(LOOTNPOP_API, FLNPMovementBaseDampingEvaluator)

public:
	FLNPMovementBaseDampingEvaluator()
	{
		SetNodeEvaluatorFlags(ECameraNodeEvaluatorFlags::NeedsSerialize);
	}

protected:
	virtual void OnBuild(const FCameraNodeEvaluatorBuildParams& Params) override
	{
		DampingEvaluator = Params.BuildEvaluator(GetCameraNodeAs<ULNPMovementBaseDampingCameraNode>()->DampingNode);
	}

	virtual FCameraNodeEvaluatorChildrenView OnGetChildren() override
	{
		return { DampingEvaluator };
	}

	virtual void OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult) override
	{
		const ALNPCharacterBase* Character = LNPCamera::ResolveOwningCharacter(Params.EvaluationContext);
		UPrimitiveComponent* Base = Character ? Character->GetMoverComponent()->GetMovementBase() : nullptr;
		if (Base && !Base->GetOwner()->IsA<ALNPMovingPanel>())
		{
			Base = nullptr;
		}

		if (!DampingEvaluator)
		{
			return;
		}

		if (Params.bIsFirstFrame)
		{
			PreviousBase = FObjectKey();
			DampingToWorld = FTransform::Identity;
		}

		if (Base)
		{
			// 패널 메시의 비균일 스케일은 운반 변환이 아니다. 위치와 회전만 누적한다.
			const FTransform BaseTransform(Base->GetComponentQuat(), Base->GetComponentLocation());
			if (PreviousBase == FObjectKey(Base))
			{
				DampingToWorld = DampingToWorld * PreviousBaseTransform.Inverse() * BaseTransform;
				DampingToWorld.NormalizeRotation();
			}
			PreviousBaseTransform = BaseTransform;
		}
		PreviousBase = FObjectKey(Base);

		// 승하차·다른 패널로 교체할 때 누적 좌표계를 유지해 감쇠 이력에 위치 점프를 만들지 않는다.
		const FRotator WorldRotation = OutResult.CameraPose.GetRotation();
		OutResult.CameraPose.SetLocation(DampingToWorld.InverseTransformPosition(OutResult.CameraPose.GetLocation()));
		OutResult.CameraPose.SetRotation(DampingToWorld.InverseTransformRotation(WorldRotation.Quaternion()).Rotator());
		DampingEvaluator->Run(Params, OutResult);
		OutResult.CameraPose.SetLocation(DampingToWorld.TransformPosition(OutResult.CameraPose.GetLocation()));
		OutResult.CameraPose.SetRotation(WorldRotation);
	}

	virtual void OnSerialize(const FCameraNodeEvaluatorSerializeParams& Params, FArchive& Ar) override
	{
		Ar << PreviousBase;
		Ar << PreviousBaseTransform;
		Ar << DampingToWorld;
	}

private:
	FCameraNodeEvaluator* DampingEvaluator = nullptr;
	FObjectKey PreviousBase;
	FTransform PreviousBaseTransform = FTransform::Identity;
	FTransform DampingToWorld = FTransform::Identity;
};

UE_DEFINE_CAMERA_NODE_EVALUATOR(FLNPMovementBaseDampingEvaluator)

} // namespace UE::Cameras

FCameraNodeEvaluatorPtr ULNPMovementBaseDampingCameraNode::OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const
{
	return Builder.BuildEvaluator<UE::Cameras::FLNPMovementBaseDampingEvaluator>();
}
