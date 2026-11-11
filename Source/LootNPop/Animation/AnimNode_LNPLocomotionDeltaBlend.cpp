// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/AnimNode_LNPLocomotionDeltaBlend.h"

#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequenceBase.h"
#include "HAL/IConsoleManager.h"
#include "Math/SpringMath.h"

const FName FAnimNode_LNPLocomotionDeltaBlend::ChestDeltaCurveNames[3] = {
	FName(TEXT("LNP_UpperBodyDelta_X")), FName(TEXT("LNP_UpperBodyDelta_Y")), FName(TEXT("LNP_UpperBodyDelta_Z")) };

namespace
{
	TAutoConsoleVariable<float> CVarUpperBodyLocomotionDelta(TEXT("LNP.Anim.UpperBodyLocomotionDelta"), 1.0f,
		TEXT("Scale for the locomotion delta added to weapon upper-body poses. 0: plain mesh-space overlay (old look), 1: node alpha as authored."));

	TAutoConsoleVariable<float> CVarUpperBodyDeltaResponse(TEXT("LNP.Anim.UpperBodyDeltaResponse"), -1.0f,
		TEXT("Spring time (seconds) for the upper-body locomotion delta to follow locomotion. <0: node value, 0: no filtering."));

	TAutoConsoleVariable<float> CVarUpperBodyDeltaSettle(TEXT("LNP.Anim.UpperBodyDeltaSettle"), -1.0f,
		TEXT("Time (seconds) over which a sustained upper-body delta is removed, keeping only transient motion. <0: node value, 0: keep sustained delta."));

	TAutoConsoleVariable<float> CVarArmedPelvisSwingKeep(TEXT("LNP.Anim.ArmedPelvisSwingKeep"), -1.0f,
		TEXT("Fraction of transient pelvis pitch/roll kept while a weapon overlay is active. <0: node value, 1: locomotion as-is, 0: fully stabilized."));

	// 평가가 오래 건너뛰어졌다 돌아오면 스프링을 한 번에 크게 밀지 않는다.
	constexpr float MaxFilterDeltaTime = 0.1f;

	/** 로컬 회전을 부모부터 누적해 컴포넌트 공간 회전을 만든다. 컴팩트 인덱스는 부모가 자식보다 앞이다. */
	template <typename AllocatorType>
	void BuildComponentRotations(const FCompactPose& Pose, TArray<FQuat, AllocatorType>& OutRotations)
	{
		const int32 NumBones = Pose.GetNumBones();
		OutRotations.SetNumUninitialized(NumBones);
		for (int32 Index = 0; Index < NumBones; ++Index)
		{
			const FCompactPoseBoneIndex Bone(Index);
			const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone);
			const FQuat Local = Pose[Bone].GetRotation();
			OutRotations[Index] = Parent.IsValid() ? (OutRotations[Parent.GetInt()] * Local).GetNormalized() : Local;
		}
	}
}

FAnimNode_LNPLocomotionDeltaBlend::FAnimNode_LNPLocomotionDeltaBlend()
	: ChainStartBone(FName(TEXT("spine_01")))
	, ChainEndBone(FName(TEXT("spine_05")))
	, PelvisBone(FName(TEXT("pelvis")))
{
}

float FAnimNode_LNPLocomotionDeltaBlend::GetSettleTime() const
{
	const float SettleOverride = CVarUpperBodyDeltaSettle.GetValueOnAnyThread();
	return (SettleOverride >= 0.0f) ? SettleOverride : SettleTime;
}

void FAnimNode_LNPLocomotionDeltaBlend::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	BasePose.Initialize(Context);
	OverlayPose.Initialize(Context);
	ResetFilter();
}

void FAnimNode_LNPLocomotionDeltaBlend::ResetFilter()
{
	bFilterValid = false;
	bPelvisSettledValid = false;
	PendingDeltaTime = 0.0f;
}

void FAnimNode_LNPLocomotionDeltaBlend::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	BasePose.CacheBones(Context);
	OverlayPose.CacheBones(Context);
	RebuildCachedBoneData(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());
}

void FAnimNode_LNPLocomotionDeltaBlend::RebuildCachedBoneData(const FBoneContainer& RequiredBones, const USkeleton* Skeleton)
{
	if (Skeleton == nullptr || RequiredBones.GetSerialNumber() == CachedRequiredBonesSerial)
		return;

	const int32 NumBones = RequiredBones.GetBoneIndicesArray().Num();

	// 덮기 가중치 — LayeredBoneBlend::UpdateCachedBoneData와 같은 경로(스켈레톤 기준 마스크 → 컴팩트 인덱스).
	TArray<FPerBoneBlendWeight> SkeletonWeights;
	FAnimationRuntime::CreateMaskWeights(SkeletonWeights, TArray<FInputBlendPose>{ LayerSetup }, Skeleton);
	TArray<FPerBoneBlendWeight> DesiredWeights;
	DesiredWeights.SetNumZeroed(NumBones);
	for (int32 Index = 0; Index < NumBones; ++Index)
	{
		const int32 SkeletonIndex = RequiredBones.GetSkeletonIndex(FCompactPoseBoneIndex(Index));
		if (SkeletonWeights.IsValidIndex(SkeletonIndex))
			DesiredWeights[Index] = SkeletonWeights[SkeletonIndex];
	}
	BoneBlendWeights.Reset(NumBones);
	BoneBlendWeights.AddZeroed(NumBones);
	TArray<float> LayerWeights{ 1.0f };
	FAnimationRuntime::UpdateDesiredBoneWeight(DesiredWeights, BoneBlendWeights, LayerWeights);

	// 델타 그룹 — 시작~끝 경로의 본은 자기 델타(Chain), 끝 본의 자손은 끝 본의 델타(Follow).
	ChainStartBone.Initialize(RequiredBones);
	ChainEndBone.Initialize(RequiredBones);
	const FCompactPoseBoneIndex Start = ChainStartBone.GetCompactPoseIndex(RequiredBones);
	const FCompactPoseBoneIndex End = ChainEndBone.GetCompactPoseIndex(RequiredBones);
	DeltaGroups.Init(EDeltaGroup::None, NumBones);
	ChainEndIndex = FCompactPoseBoneIndex(INDEX_NONE);
	if (Start.IsValid() && End.IsValid() && (Start == End || RequiredBones.BoneIsChildOf(End, Start)))
	{
		ChainEndIndex = End;
		for (int32 Index = 0; Index < NumBones; ++Index)
		{
			const FCompactPoseBoneIndex Bone(Index);
			const bool bFromStart = (Bone == Start) || RequiredBones.BoneIsChildOf(Bone, Start);
			if (bFromStart && ((Bone == End) || RequiredBones.BoneIsChildOf(End, Bone)))
				DeltaGroups[Index] = EDeltaGroup::Chain;
			else if (RequiredBones.BoneIsChildOf(Bone, End))
				DeltaGroups[Index] = EDeltaGroup::Follow;
		}
	}

	// 골반과, 척추 체인이 아닌 골반의 자식(다리) — 안정화 때 다리는 메시 공간 회전을 유지한다.
	PelvisBone.Initialize(RequiredBones);
	PelvisIndex = PelvisBone.GetCompactPoseIndex(RequiredBones);
	PelvisLegChildren.Reset();
	if (PelvisIndex.IsValid())
	{
		for (int32 Index = PelvisIndex.GetInt() + 1; Index < NumBones; ++Index)
		{
			const FCompactPoseBoneIndex Bone(Index);
			if (RequiredBones.GetParentBoneIndex(Bone) == PelvisIndex && DeltaGroups[Index] == EDeltaGroup::None)
				PelvisLegChildren.Add(Bone);
		}
	}

	// 기준 포즈 — 고정 프레임이라 필요 본이 바뀔 때만 다시 뽑는다.
	ReferenceComponentRotations.Reset();
	if (ReferencePose)
	{
		// 포즈·커브 컨테이너가 스레드 메모리 스택을 쓰므로 이 블록이 끝나면 되돌린다.
		FMemMark Mark(FMemStack::Get());
		FCompactPose Pose;
		Pose.SetBoneContainer(&RequiredBones);
		FBlendedCurve Curve;
		Curve.InitFrom(RequiredBones);
		UE::Anim::FStackAttributeContainer Attributes;
		FAnimationPoseData PoseData(Pose, Curve, Attributes);
		ReferencePose->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(ReferenceTime)));

		BuildComponentRotations(Pose, ReferenceComponentRotations);
	}

	FilteredDeltas.Init(FVector::ZeroVector, NumBones);
	FilteredVelocities.Init(FVector::ZeroVector, NumBones);
	SettledDeltas.Init(FVector::ZeroVector, NumBones);
	ResetFilter();
	CachedRequiredBonesSerial = RequiredBones.GetSerialNumber();
}

void FAnimNode_LNPLocomotionDeltaBlend::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	GetEvaluateGraphExposedInputs().Execute(Context);
	RebuildCachedBoneData(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());

	// 루트 모션은 로코모션만 낸다(LayeredBoneBlend의 루트 본 기준 루트 모션 블렌드와 같은 결과 — 루트 가중치 0).
	BasePose.Update(Context);
	OverlayPose.Update(Context.FractionalWeightAndRootMotion(1.0f, 0.0f));
	PendingDeltaTime += Context.GetDeltaTime();
}

FVector FAnimNode_LNPLocomotionDeltaBlend::ApplyLocomotionDelta(const FCompactPose& Base, FCompactPose& Overlay, float Alpha, float DeltaTime)
{
	const int32 NumBones = Overlay.GetNumBones();

	TArray<FQuat, TMemStackAllocator<>> BaseRotations;
	BuildComponentRotations(Base, BaseRotations);
	TArray<FQuat, TMemStackAllocator<>> OverlayRotations;
	BuildComponentRotations(Overlay, OverlayRotations);

	const float ResponseOverride = CVarUpperBodyDeltaResponse.GetValueOnAnyThread();
	const float Response = (ResponseOverride >= 0.0f) ? ResponseOverride : ResponseTime;
	const float Settle = GetSettleTime();

	// 체인 본마다 메시 공간 델타(로코모션 × 기준⁻¹)에서 수직축 비틀기를 떼고 숙임·기울임만 남겨 스프링으로 거른다.
	// 작은 각이라 회전 벡터로 거른다(쿼터니언 스프링과 실질 차이 없음, 벡터 연산으로 단순).
	TArray<FQuat, TMemStackAllocator<>> Deltas;
	Deltas.Init(FQuat::Identity, NumBones);
	for (int32 Index = 0; Index < NumBones; ++Index)
	{
		if (DeltaGroups[Index] != EDeltaGroup::Chain)
			continue;

		const FQuat Delta = (BaseRotations[Index] * ReferenceComponentRotations[Index].Inverse()).GetNormalized();
		FQuat Swing, Twist;
		Delta.ToSwingTwist(FVector::UpVector, Swing, Twist);
		const FVector Target = Swing.ToRotationVector();

		FVector& Filtered = FilteredDeltas[Index];
		FVector& Settled = SettledDeltas[Index];
		if (!bFilterValid)
		{
			// 시작 자세의 차이는 이미 "오래 유지된" 것으로 본다 — 무기를 든 순간 상체가 튀지 않게.
			Filtered = Target;
			FilteredVelocities[Index] = FVector::ZeroVector;
			Settled = Target;
		}
		else
		{
			SpringMath::CriticalSpringDamper(Filtered, FilteredVelocities[Index], Target, Response, DeltaTime);
			FMath::ExponentialSmoothingApprox(Settled, Filtered, DeltaTime, Settle);
		}
		const FVector Transient = (Settle > 0.0f) ? (Filtered - Settled) : Filtered;
		Deltas[Index] = FQuat::MakeFromRotationVector(Transient * Alpha);
	}
	bFilterValid = true;
	const FQuat EndDelta = Deltas[ChainEndIndex.GetInt()];

	// 새 컴포넌트 회전을 부모부터 정하고 로컬로 되돌린다. 델타가 없는 본은 로컬을 유지한 채 부모를 따라간다.
	TArray<FQuat, TMemStackAllocator<>> NewRotations;
	NewRotations.SetNumUninitialized(NumBones);
	for (int32 Index = 0; Index < NumBones; ++Index)
	{
		const FCompactPoseBoneIndex Bone(Index);
		const FCompactPoseBoneIndex Parent = Overlay.GetParentBoneIndex(Bone);
		const FQuat ParentRotation = Parent.IsValid() ? NewRotations[Parent.GetInt()] : FQuat::Identity;
		FTransform& Local = Overlay[Bone];

		const EDeltaGroup Group = DeltaGroups[Index];
		if (Group == EDeltaGroup::None)
		{
			NewRotations[Index] = (ParentRotation * Local.GetRotation()).GetNormalized();
			continue;
		}

		const FQuat& Delta = (Group == EDeltaGroup::Chain) ? Deltas[Index] : EndDelta;
		NewRotations[Index] = (Delta * OverlayRotations[Index]).GetNormalized();
		Local.SetRotation((ParentRotation.Inverse() * NewRotations[Index]).GetNormalized());
	}

	return EndDelta.ToRotationVector();
}

void FAnimNode_LNPLocomotionDeltaBlend::StabilizePelvis(FCompactPose& Base, float DeltaTime)
{
	const float KeepOverride = CVarArmedPelvisSwingKeep.GetValueOnAnyThread();
	const float Keep = FMath::Clamp((KeepOverride >= 0.0f) ? KeepOverride : PelvisSwingKeep, 0.0f, 1.0f);
	if (Keep >= 1.0f || !PelvisIndex.IsValid() || ReferenceComponentRotations.Num() != Base.GetNumBones())
	{
		bPelvisSettledValid = false;
		return;
	}

	TArray<FQuat, TMemStackAllocator<>> Rotations;
	BuildComponentRotations(Base, Rotations);
	const int32 Pelvis = PelvisIndex.GetInt();

	// 기준 포즈 대비 골반의 메시 공간 숙임·기울임(수직축 비틀기 제외). 오래 유지되는 성분은 그대로 둔다.
	FQuat Swing, Twist;
	(Rotations[Pelvis] * ReferenceComponentRotations[Pelvis].Inverse()).GetNormalized().ToSwingTwist(FVector::UpVector, Swing, Twist);
	const FVector Target = Swing.ToRotationVector();
	const float Settle = GetSettleTime();
	if (!bPelvisSettledValid)
	{
		PelvisSettled = Target;
		bPelvisSettledValid = true;
	}
	else
	{
		FMath::ExponentialSmoothingApprox(PelvisSettled, Target, DeltaTime, Settle);
	}
	const FVector Transient = (Settle > 0.0f) ? (Target - PelvisSettled) : Target;

	// 순간 성분의 (1 − Keep)만큼 되돌린다. 골반 높이(로컬 위치)는 건드리지 않는다.
	const FQuat NewPelvis = (FQuat::MakeFromRotationVector(Transient * (Keep - 1.0f)) * Rotations[Pelvis]).GetNormalized();
	const FCompactPoseBoneIndex Parent = Base.GetParentBoneIndex(PelvisIndex);
	const FQuat ParentRotation = Parent.IsValid() ? Rotations[Parent.GetInt()] : FQuat::Identity;
	Base[PelvisIndex].SetRotation((ParentRotation.Inverse() * NewPelvis).GetNormalized());

	// 다리는 원래 메시 공간 회전을 유지한다 — 발 위치·무릎 굽힘은 로코모션 그대로.
	const FQuat InvNewPelvis = NewPelvis.Inverse();
	for (const FCompactPoseBoneIndex Leg : PelvisLegChildren)
		Base[Leg].SetRotation((InvNewPelvis * Rotations[Leg.GetInt()]).GetNormalized());
}

void FAnimNode_LNPLocomotionDeltaBlend::Evaluate_AnyThread(FPoseContext& Output)
{
	FPoseContext BaseContext(Output);
	BasePose.Evaluate(BaseContext);

	FPoseContext OverlayContext(Output);
	OverlayPose.Evaluate(OverlayContext);

	const float DeltaTime = FMath::Min(PendingDeltaTime, MaxFilterDeltaTime);
	PendingDeltaTime = 0.0f;

	// 골반을 먼저 잡는다 — 상체 델타와 마스크 아래쪽 블렌드가 안정화된 로코모션을 기준으로 삼는다.
	StabilizePelvis(BaseContext.Pose, DeltaTime);

	const int32 NumBones = BaseContext.Pose.GetNumBones();
	const float Alpha = FMath::Max(0.0f, DeltaAlpha) * FMath::Max(0.0f, CVarUpperBodyLocomotionDelta.GetValueOnAnyThread());
	FVector ChestDelta = FVector::ZeroVector;
	if (Alpha > 0.0f && ChainEndIndex.IsValid() && ReferenceComponentRotations.Num() == NumBones && DeltaGroups.Num() == NumBones)
		ChestDelta = ApplyLocomotionDelta(BaseContext.Pose, OverlayContext.Pose, Alpha, DeltaTime);
	else
		bFilterValid = false;

	TArray<FCompactPose, TMemStackAllocator<>> BlendPoses;
	BlendPoses.SetNum(1);
	BlendPoses[0].MoveBonesFrom(OverlayContext.Pose);

	TArray<FBlendedCurve, TMemStackAllocator<>> BlendCurves;
	BlendCurves.SetNum(1);
	BlendCurves[0].MoveFrom(OverlayContext.Curve);

	TArray<UE::Anim::FStackAttributeContainer, TMemStackAllocator<>> BlendAttributes;
	BlendAttributes.SetNum(1);
	BlendAttributes[0].MoveFrom(OverlayContext.CustomAttributes);

	const FAnimationRuntime::EBlendPosesPerBoneFilterFlags Flags = bMeshSpaceRotationBlend
		? FAnimationRuntime::EBlendPosesPerBoneFilterFlags::MeshSpaceRotation
		: FAnimationRuntime::EBlendPosesPerBoneFilterFlags::None;

	FAnimationPoseData OutputData(Output);
	FAnimationRuntime::BlendPosesPerBoneFilter(BaseContext.Pose, BlendPoses, BaseContext.Curve, BlendCurves,
		BaseContext.CustomAttributes, BlendAttributes, OutputData, BoneBlendWeights, Flags, CurveBlendOption);

	LastChestDelta = ChestDelta;

	// 표시 ABP 총열 보정이 이 기울기는 되돌리지 않도록 내보낸다(꺼져 있으면 0).
	Output.Curve.Set(ChestDeltaCurveNames[0], static_cast<float>(ChestDelta.X));
	Output.Curve.Set(ChestDeltaCurveNames[1], static_cast<float>(ChestDelta.Y));
	Output.Curve.Set(ChestDeltaCurveNames[2], static_cast<float>(ChestDelta.Z));
}

void FAnimNode_LNPLocomotionDeltaBlend::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha %.2f, Scale %.2f, Chest %.1f deg)"), DeltaAlpha,
		CVarUpperBodyLocomotionDelta.GetValueOnAnyThread(), FMath::RadiansToDegrees(LastChestDelta.Size()));
	DebugData.AddDebugItem(DebugLine);
	BasePose.GatherDebugData(DebugData.BranchFlow(1.0f));
	OverlayPose.GatherDebugData(DebugData.BranchFlow(1.0f));
}
