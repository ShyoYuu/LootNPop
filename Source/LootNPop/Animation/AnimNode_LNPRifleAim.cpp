// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/AnimNode_LNPRifleAim.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/LNPRifleAimSolver.h"
#include "HAL/IConsoleManager.h"
#include "Math/SpringMath.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDisplayRifleAim(TEXT("LNP.Anim.RifleAim"), 1,
		TEXT("Enable precise barrel alignment in the display ABP. 0: off, 1: on."));

	TAutoConsoleVariable<float> CVarRifleAimSmoothing(TEXT("LNP.Anim.RifleAimSmoothing"), -1.0f,
		TEXT("Spring time (seconds) for the barrel correction to follow its solution. <0: node value, 0: snap every frame (old behavior)."));

	TAutoConsoleVariable<int32> CVarRifleAimKeepBodyDelta(TEXT("LNP.Anim.RifleAimKeepBodyDelta"), 1,
		TEXT("1: barrel correction leaves the upper-body locomotion delta in place (the rifle dips with the body). 0: correct it away."));

	// 평가가 오래 건너뛰어졌다 돌아오면 스프링을 한 번에 크게 밀지 않는다.
	constexpr float MaxSpringDeltaTime = 0.1f;
}

FAnimNode_LNPRifleAim::FAnimNode_LNPRifleAim()
	: PivotBone(FName(TEXT("spine_03")))
{
}

void FAnimNode_LNPRifleAim::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
	bHasSmoothedCorrection = false;
	PendingDeltaTime = 0.0f;
}

void FAnimNode_LNPRifleAim::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);
	PendingDeltaTime += Context.GetDeltaTime();
}

void FAnimNode_LNPRifleAim::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const float DeltaTime = FMath::Min(PendingDeltaTime, MaxSpringDeltaTime);
	PendingDeltaTime = 0.0f;

	const float AimAlpha = FMath::Clamp(Inputs.Alpha, 0.0f, 1.0f);
	if (AimAlpha <= 0.0f || CVarDisplayRifleAim.GetValueOnAnyThread() == 0)
	{
		// 장전·대시로 꺼졌다 돌아오면 그때의 해에서 다시 시작한다.
		bHasSmoothedCorrection = false;
		return;
	}

	// 앵커는 무기마다 다를 수 있어 이름으로 찾는다. 이번 LOD에 없거나 피벗 아래가 아니면 보정하지 않는다.
	const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
	const int32 AnchorMeshIndex = Bones.GetPoseBoneIndexForBoneName(Inputs.AnchorBone);
	if (AnchorMeshIndex == INDEX_NONE)
		return;
	const FCompactPoseBoneIndex Anchor = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(AnchorMeshIndex));
	const FCompactPoseBoneIndex Pivot = PivotBone.GetCompactPoseIndex(Bones);
	if (!Anchor.IsValid() || !Bones.BoneIsChildOf(Anchor, Pivot))
		return;

	const FTransform& MeshWorld = Output.AnimInstanceProxy->GetComponentTransform();
	const FTransform PivotComponent = Output.Pose.GetComponentSpaceTransform(Pivot);
	const FTransform MuzzleWorld = Inputs.MuzzleInAnchor * Output.Pose.GetComponentSpaceTransform(Anchor) * MeshWorld;
	const FLNPRifleAimSolver Solver((PivotComponent * MeshWorld).GetLocation(), MuzzleWorld, Inputs.AimTarget, Inputs.ViewDirection,
		Inputs.MuzzleOffsetWorld, Inputs.ActorLocation, Inputs.WorldCollision);
	// 월드 회전 해를 컴포넌트 공간으로 옮긴 뒤 스프링으로 따라간다.
	const FQuat MeshRotation = MeshWorld.GetRotation();
	FQuat TargetCorrection = (MeshRotation.Inverse() * Solver.Solve() * MeshRotation).GetNormalized();
	// 해는 델타로 숙여진 만큼(D)까지 되돌린다(≈ R·D⁻¹). D를 다시 곱해 델타가 만든 이탈은 남기고 조준 잔차 R만 고친다.
	if (CVarRifleAimKeepBodyDelta.GetValueOnAnyThread() != 0 && !Inputs.BodyDeltaRotation.IsNearlyZero())
		TargetCorrection = (TargetCorrection * FQuat::MakeFromRotationVector(Inputs.BodyDeltaRotation)).GetNormalized();

	const float SmoothingOverride = CVarRifleAimSmoothing.GetValueOnAnyThread();
	const float Smoothing = (SmoothingOverride >= 0.0f) ? SmoothingOverride : SmoothingTime;
	if (!bHasSmoothedCorrection || Smoothing <= 0.0f)
	{
		SmoothedCorrection = TargetCorrection;
		SmoothedAngularVelocity = FVector::ZeroVector;
		bHasSmoothedCorrection = true;
	}
	else
	{
		SpringMath::CriticalSpringDamperQuat(SmoothedCorrection, SmoothedAngularVelocity, TargetCorrection, Smoothing, DeltaTime);
	}

	// 피벗에만 건다. 자식 본은 로컬 변환을 유지한 채 따라간다.
	const FQuat Applied = FQuat::Slerp(FQuat::Identity, SmoothedCorrection, AimAlpha).GetNormalized();
	FTransform Corrected = PivotComponent;
	Corrected.SetRotation((Applied * PivotComponent.GetRotation()).GetNormalized());
	OutBoneTransforms.Add(FBoneTransform(Pivot, Corrected));
}

bool FAnimNode_LNPRifleAim::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return PivotBone.IsValidToEvaluate(RequiredBones);
}

void FAnimNode_LNPRifleAim::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	PivotBone.Initialize(RequiredBones);
}
