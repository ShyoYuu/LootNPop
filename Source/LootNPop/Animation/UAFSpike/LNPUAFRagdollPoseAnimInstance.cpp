// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFRagdollPoseAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "LootNPop.h"

static TAutoConsoleVariable<int32> CVarRagdollHoldPose(
	TEXT("LNP.UAFSpike.RagdollHoldPose"), 1,
	TEXT("1: during ragdoll the UAF spike VisualMesh evaluates the pose held at death (default).\n")
	TEXT("0: previous behavior, engine evaluation without an anim instance (reference pose). Applies at the next ragdoll entry."));

static TAutoConsoleVariable<int32> CVarDebugRagdoll(
	TEXT("LNP.UAFSpike.DebugRagdoll"), 0,
	TEXT("1: log UAF spike VisualMesh hand span (bone buffer vs physics bodies) for the first frames of each ragdoll and on every teleport move.\n")
	TEXT("Reference pose hand span is about 104cm."));

namespace
{
	constexpr uint64 RagdollProbeFrames = 30;

	float BoneBufferHandSpan(const USkeletalMeshComponent& Mesh)
	{
		const TArray<FTransform>& Pose = Mesh.GetComponentSpaceTransforms();
		const int32 Left = Mesh.GetBoneIndex(TEXT("hand_l"));
		const int32 Right = Mesh.GetBoneIndex(TEXT("hand_r"));
		return Pose.IsValidIndex(Left) && Pose.IsValidIndex(Right) ? FVector::Dist(Pose[Left].GetLocation(), Pose[Right].GetLocation()) : -1.f;
	}

	float BodyHandSpan(const USkeletalMeshComponent& Mesh)
	{
		const FBodyInstance* Left = Mesh.GetBodyInstance(TEXT("hand_l"));
		const FBodyInstance* Right = Mesh.GetBodyInstance(TEXT("hand_r"));
		return Left && Right ? FVector::Dist(Left->GetUnrealWorldTransform().GetLocation(), Right->GetUnrealWorldTransform().GetLocation()) : -1.f;
	}

	void LogRagdollProbe(const USkeletalMeshComponent& Mesh, uint64 StartFrame, const TCHAR* Phase)
	{
		const UAnimInstance* Instance = Mesh.GetAnimInstance();
		UE_LOG(LogLootNPop, Log, TEXT("[UAFRagdollProbe] %s %s frame=%llu +%llu %s anim=%d inst=%s bufferHandSpan=%.0f bodyHandSpan=%.0f"),
			*GetNameSafe(Mesh.GetOwner()), Mesh.GetNetMode() == NM_Client ? TEXT("guest") : TEXT("host"),
			GFrameCounter, GFrameCounter - StartFrame, Phase, Mesh.bEnableAnimation ? 1 : 0,
			Instance ? *Instance->GetClass()->GetName() : TEXT("None"), BoneBufferHandSpan(Mesh), BodyHandSpan(Mesh));
	}

	struct FLNPUAFRagdollPoseProxy final : FAnimInstanceProxy
	{
		using FAnimInstanceProxy::FAnimInstanceProxy;

		virtual void Initialize(UAnimInstance* InAnimInstance) override
		{
			FAnimInstanceProxy::Initialize(InAnimInstance);

			// InitAnim은 첫 평가 전에 인스턴스를 만든다. 이때 읽기 버퍼는 아직 UAF가 마지막으로 쓴 사망 순간 포즈다
			// (애니가 꺼진 동안의 물리 블렌드는 버퍼를 뒤집지 않는다).
			HeldLocalPose.Reset();
			const USkeletalMeshComponent* Mesh = InAnimInstance->GetSkelMeshComponent();
			const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
			if (!Asset)
				return;
			const TArray<FTransform>& ComponentPose = Mesh->GetComponentSpaceTransforms();
			const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
			if (ComponentPose.Num() != RefSkeleton.GetNum())
				return;

			HeldLocalPose.SetNumUninitialized(ComponentPose.Num());
			for (int32 BoneIndex = 0; BoneIndex < ComponentPose.Num(); ++BoneIndex)
			{
				const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
				HeldLocalPose[BoneIndex] = ParentIndex == INDEX_NONE
					? ComponentPose[BoneIndex]
					: ComponentPose[BoneIndex].GetRelativeTransform(ComponentPose[ParentIndex]);
			}
		}

		virtual bool Evaluate(FPoseContext& Output) override
		{
			// 잡아 둔 포즈가 없으면 엔진 기본(참조 포즈)으로 둔다.
			if (HeldLocalPose.IsEmpty())
				return false;

			const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
			for (const FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
			{
				const int32 MeshIndex = Bones.MakeMeshPoseIndex(BoneIndex).GetInt();
				Output.Pose[BoneIndex] = HeldLocalPose.IsValidIndex(MeshIndex) ? HeldLocalPose[MeshIndex] : Output.Pose.GetRefPose(BoneIndex);
			}
			return true;
		}

		/** 메시 본 인덱스 순서의 로컬 포즈. */
		TArray<FTransform> HeldLocalPose;
	};
}

FAnimInstanceProxy* ULNPUAFRagdollPoseAnimInstance::CreateAnimInstanceProxy()
{
	return new FLNPUAFRagdollPoseProxy(this);
}

UClass* LNPUAFRagdollPose::GetRagdollAnimClass()
{
	return CVarRagdollHoldPose.GetValueOnGameThread() > 0 ? ULNPUAFRagdollPoseAnimInstance::StaticClass() : nullptr;
}

void LNPUAFRagdollPose::BindTeleportProbe(USkeletalMeshComponent& VisualMesh, UObject& Owner, const uint64* RagdollStartFrame)
{
	// 부모 이동이 TeleportPhysics로 오면 엔진이 시뮬 바디를 본 버퍼 포즈로 옮긴다(UpdateKinematicBonesToAnim). 옮겨진 뒤의 값을 남긴다.
	VisualMesh.TransformUpdated.AddWeakLambda(&Owner, [RagdollStartFrame](USceneComponent* Component, EUpdateTransformFlags, ETeleportType Teleport)
	{
		if (Teleport != ETeleportType::None && *RagdollStartFrame != 0 && CVarDebugRagdoll.GetValueOnGameThread() > 0)
			LogRagdollProbe(*CastChecked<USkeletalMeshComponent>(Component), *RagdollStartFrame,
				Teleport == ETeleportType::TeleportPhysics ? TEXT("TeleportPhysics") : TEXT("ResetPhysics"));
	});
}

void LNPUAFRagdollPose::TickProbe(const USkeletalMeshComponent& VisualMesh, uint64 RagdollStartFrame)
{
	if (CVarDebugRagdoll.GetValueOnGameThread() > 0 && RagdollStartFrame != 0 && GFrameCounter - RagdollStartFrame < RagdollProbeFrames)
		LogRagdollProbe(VisualMesh, RagdollStartFrame, TEXT("PrePhysics"));
}
