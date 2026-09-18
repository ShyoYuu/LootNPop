// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Camera/LNPLobbedADSPitchCameraNode.h"
#include "Camera/LNPCameraNodeUtils.h"
#include "HitDetection/LNPTrajectoryGuideComponent.h"

#include "Core/CameraNodeEvaluator.h"
#include "Core/CameraSystemEvaluator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LNPLobbedADSPitchCameraNode)

namespace UE::Cameras
{

class FLNPLobbedADSPitchNodeEvaluator : public FCameraNodeEvaluator
{
	UE_DECLARE_CAMERA_NODE_EVALUATOR(LOOTNPOP_API, FLNPLobbedADSPitchNodeEvaluator)

protected:
	virtual void OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult) override;
	virtual void OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult) override;

private:
	/** 이번 프레임에 도달해야 할 기울기(도, 양수 = 아래) — 착탄점에서 역산한다. */
	static double ResolveTargetPitchDown(const ULNPLobbedADSPitchCameraNode& Node,
		const ALNPCharacterBase& Character, const FCameraNodeEvaluationResult& OutResult);

	/** 현재 적용 중인 기울기 (도, 양수 = 아래). 목표가 매 프레임 달라지므로 일정 속도로 따라간다. */
	double CurrentPitchDown = 0.0;
};

UE_DEFINE_CAMERA_NODE_EVALUATOR(FLNPLobbedADSPitchNodeEvaluator)

void FLNPLobbedADSPitchNodeEvaluator::OnInitialize(
	const FCameraNodeEvaluatorInitializeParams& Params,
	FCameraNodeEvaluationResult& OutResult)
{
	SetNodeEvaluatorFlags(ECameraNodeEvaluatorFlags::None);

	// 폰은 캐싱하지 않는다 — 근거는 LNPCameraNodeUtils.h의 ResolveOwningCharacter 주석.
}

double FLNPLobbedADSPitchNodeEvaluator::ResolveTargetPitchDown(
	const ULNPLobbedADSPitchCameraNode& Node,
	const ALNPCharacterBase& Character,
	const FCameraNodeEvaluationResult& OutResult)
{
	// 착탄점은 가이드가 이미 푼 값을 당겨 읽는다 — 카메라가 궤적을 다시 적분하면 카메라가 겨냥하는
	// 곳과 장판이 놓인 곳이 갈린다 (TechDesign_HitDetection.md §7.6과 같은 교훈).
	//
	// 장판을 모르는 프레임(SurfaceCache 베이킹 전 등)에는 기울이지 않는다 — 이 노드는 장판을
	// 프레임 안에 붙잡기 위해 존재하므로, 붙잡을 것이 없으면 기울일 이유도 없다.
	const ULNPTrajectoryGuideComponent* Guide = Character.FindComponentByClass<ULNPTrajectoryGuideComponent>();
	FVector ImpactPoint = FVector::ZeroVector;  // UE 수학 타입은 기본 생성자가 초기화하지 않는다 (C4701)
	if (!Guide || !Guide->GetImpactPoint(ImpactPoint))
		return 0.0;

	const FVector ToImpact = (ImpactPoint - OutResult.CameraPose.GetLocation()).GetSafeNormal();
	if (ToImpact.IsNearlyZero())
		return 0.0;

	// ⚠️ **우리 Pitch가 적용되기 전의 축을 읽어야 한다.** 리그는 매 프레임 처음부터 다시 평가되므로
	//    이 시점의 CameraPose 회전이 곧 조준축(ControlRotation)이고, 앞선 노드가 중력 정렬까지
	//    끝내 둔 상태다(§2.4의 4번). 기울인 뒤의 축을 읽으면 되먹임이 생겨 각이 발산한다.
	const FQuat    AimRot     = OutResult.CameraPose.GetRotation().Quaternion();
	const FVector  AimForward = AimRot.GetForwardVector();
	const FVector  AimUp      = AimRot.GetUpVector();

	// 조준축 기준 착탄점의 상하각 (양수 = 위). 유탄은 늘 아래에 있으므로 보통 음수다.
	const double ImpactPitchDeg = FMath::RadiansToDegrees(FMath::Atan2(
		FVector::DotProduct(ToImpact, AimUp), FVector::DotProduct(ToImpact, AimForward)));

	// 조준축에서 아래로 벌어진 각에서 "원하는 화면 위치"를 빼면, 그 차이만큼만 내리면 된다.
	return FMath::Clamp(-ImpactPitchDeg - Node.DesiredImpactBelowCenterDeg,
		0.0, static_cast<double>(Node.MaxPitchDownDegrees));
}

void FLNPLobbedADSPitchNodeEvaluator::OnRun(
	const FCameraNodeEvaluationParams& Params,
	FCameraNodeEvaluationResult& OutResult)
{
#if WITH_EDITOR
	if (Params.Evaluator && Params.Evaluator->GetRole() == ECameraSystemEvaluatorRole::EditorPreview)
		return;
#endif

	const ULNPLobbedADSPitchCameraNode* Node = GetCameraNodeAs<ULNPLobbedADSPitchCameraNode>();
	const ALNPCharacterBase* Character = LNPCamera::ResolveOwningCharacter(Params.EvaluationContext);

	const double TargetPitchDown = (Character && Character->IsLobbedADSActive())
		? ResolveTargetPitchDown(*Node, *Character, OutResult)
		: 0.0;

	CurrentPitchDown = FMath::FInterpConstantTo(CurrentPitchDown, TargetPitchDown,
		Params.DeltaTime, Node->BlendSpeedDegreesPerSecond);

	if (FMath::IsNearlyZero(CurrentPitchDown))
		return;

	// 카메라 로컬 축 기준 가산 회전 — 위치는 그대로 두고 시선만 내린다.
	// (엔진 USetRotationCameraNode의 CameraPose 공간 처리와 같은 식이다.)
	// 앞선 노드가 이미 중력 정렬을 끝냈으므로(§2.4의 4번) 이 Pitch 축은 구 내벽 어디서든 중력 기준이다.
	const FTransform3d PitchDown(FRotator3d(-CurrentPitchDown, 0.0, 0.0));
	OutResult.CameraPose.SetTransform(PitchDown * OutResult.CameraPose.GetTransform());
}

} // namespace UE::Cameras

FCameraNodeEvaluatorPtr ULNPLobbedADSPitchCameraNode::OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const
{
	using namespace UE::Cameras;
	return Builder.BuildEvaluator<FLNPLobbedADSPitchNodeEvaluator>();
}
