// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/AnimNode_LNPRifleAim.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/LNPRifleAimSolver.h"
#include "HAL/IConsoleManager.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDisplayRifleAim(TEXT("LNP.Anim.RifleAim"), 1,
		TEXT("Enable precise barrel alignment in the display ABP. 0: off, 1: on."));
}

FAnimNode_LNPRifleAim::FAnimNode_LNPRifleAim()
	: PivotBone(FName(TEXT("spine_03")))
{
}

void FAnimNode_LNPRifleAim::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const float AimAlpha = FMath::Clamp(Inputs.Alpha, 0.0f, 1.0f);
	if (AimAlpha <= 0.0f || CVarDisplayRifleAim.GetValueOnAnyThread() == 0)
		return;

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
	const FQuat Applied = FQuat::Slerp(FQuat::Identity, Solver.Solve(), AimAlpha).GetNormalized();

	// 월드 회전 Applied를 컴포넌트 공간으로 옮겨 피벗에만 건다. 자식 본은 로컬 변환을 유지한 채 따라간다.
	const FQuat MeshRotation = MeshWorld.GetRotation();
	FTransform Corrected = PivotComponent;
	Corrected.SetRotation((MeshRotation.Inverse() * Applied * MeshRotation * PivotComponent.GetRotation()).GetNormalized());
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
