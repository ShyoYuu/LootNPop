// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFUpperBodyMontageTrait.h"

#include "Animation/Skeleton.h"
#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "EvaluationVM/Tasks/BlendKeyframesPerBone.h"
#include "EvaluationVM/Tasks/ConvertRotationsToLocalSpace.h"
#include "EvaluationVM/Tasks/ConvertRotationsToMeshSpace.h"
#include "EvaluationVM/Tasks/NormalizeRotations.h"
#include "EvaluationVM/Tasks/StoreKeyframe.h"
#include "HAL/IConsoleManager.h"
#include "Injection/MontageTrait.h"
#include "LootNPop.h"

namespace UE::UAF
{
	AUTO_REGISTER_ANIM_TRAIT(FLNPUAFUpperBodyMontageTrait)

#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
	GeneratorMacro(IEvaluate)
	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FLNPUAFUpperBodyMontageTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
#undef TRAIT_INTERFACE_ENUMERATOR

	void FLNPUAFUpperBodyMontageTrait::PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
	{
		FInstanceData* Instance = Binding.GetInstanceData<FInstanceData>();
		FTraitBinding Base;
		const bool bMontageBase = Binding.GetStackBaseTrait(Base) && Base.GetTrait()->GetTraitUID() == FMontageTrait::TraitUID;
		const auto* Shared = bMontageBase ? Base.GetSharedData<FMontageTrait::FSharedData>() : nullptr;
		const auto* State = bMontageBase ? Base.GetInstanceData<FMontageTrait::FInstanceData>() : nullptr;
		if (!Shared || Shared->GetSlotName(Base) != TEXT("UpperBody") || !Shared->GetbAlwaysUpdateSource(Base) || Shared->GetbDisableSynchronization(Base)
			|| !State->Source.IsValid() || UseAbstractSkeletonRuntime())
		{
			if (!Instance->bReportedConfigurationError)
			{
				Context.Log(ELogVerbosity::Error,
					TEXT("Requires a native UpperBody Montage Trait with a connected Source, Always Update Source=true, Disable Synchronization=false and abstract skeleton runtime disabled. Montage evaluation skipped."),
					TEXT("LNPUpperBodyMontage"));
				Instance->bReportedConfigurationError = true;
			}
			return;
		}

		// 자식은 이미 한 번 평가됐다. native가 소비할 사본만 추가하며 별도 자식을 만들지 않는다.
		const bool bHasMontage = !State->EvalTaskDataForSlot.IsEmpty() || !State->AdditiveEvalTaskDataForSlot.IsEmpty();
		if (bHasMontage)
			Context.AppendTask(FAnimNextDuplicateTopKeyframeTask::Make());
		IEvaluate::PostEvaluate(Context, Binding);
		if (bHasMontage)
		{
			FLNPUAFBlendUpperBodyTask Task;
			const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.Anim.DebugLocomotion"));
			Task.bLogEvaluation = Debug && Debug->GetInt() >= 2 && Instance->EvaluationCount++ % 60 == 0;
			Context.AppendTask(MoveTemp(Task));
		}
	}
}

void FLNPUAFBlendUpperBodyTask::Execute(UE::UAF::FEvaluationVM& VM) const
{
	using namespace UE::UAF;
	if (!ensureMsgf(!VM.GetActiveNamedSet(), TEXT("LNP UpperBody mask does not support named sets.")))
		return;
	const auto* Base = VM.PeekValue<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 1);
	const auto* Montage = VM.PeekValue<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0);
	if (!ensureMsgf(Base && Montage && Base->IsValid() && Montage->IsValid(), TEXT("LNP UpperBody mask requires source and montage keyframes.")))
		return;

	const USkeleton* Skeleton = (*Base)->Pose.GetSkeletonAsset();
	const FReferenceSkeleton& Bones = Skeleton->GetReferenceSkeleton();
	const int32 SpineIndex = Bones.FindBoneIndex(TEXT("spine_01"));
	if (!ensureMsgf(SpineIndex != INDEX_NONE && Skeleton == (*Montage)->Pose.GetSkeletonAsset(), TEXT("LNP UpperBody mask requires matching skeletons with spine_01.")))
	{
		TUniquePtr<FKeyframeState> DiscardedMontage;
		ensure(VM.PopValue(KEYFRAME_STACK_NAME, DiscardedMontage));
		return;
	}

	UUAFBlendMask::FSkeletonBoneWeightArray Weights;
	Weights.SetNumZeroed(Bones.GetNum());
	int32 UpperBones = 0;
	for (int32 Index = SpineIndex; Index < Bones.GetNum(); ++Index)
	{
		if (Bones.GetDepthBetweenBones(Index, SpineIndex) != INDEX_NONE)
		{
			Weights[Index] = 1.0f;
			++UpperBones;
		}
	}

	// 속성은 compact pose 인덱스를 쓴다. 스켈레톤 순서와 다른 LOD에서도 루트/하체 속성을 보존한다.
	UE::Anim::FStackAttributeContainer Attributes;
	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		TArray<float> CompactWeights;
		for (const FBoneIndexType Index : (*Base)->Pose.GetLODBoneIndexToSkeletonBoneIndexMap())
			CompactWeights.Add(Weights[Index]);
		UE::Anim::Attributes::BlendAttributesPerBone((*Base)->Attributes, (*Montage)->Attributes, CompactWeights, Attributes);
	}
	FAnimNextConvertRotationsToMeshSpaceTask::Make(2).Execute(VM);
	// 가중치1의 커브 합성은 기존 Override와 같이 몽타주 값을 우선하고 원본에만 있는 커브는 유지한다.
	FAnimNextBlendKeyframePerBoneWithScaleTask::Make(Skeleton, &Weights, nullptr, nullptr, 1.0f).Execute(VM);
	(*VM.PeekValueMutable<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0))->Attributes.MoveFrom(Attributes);
	FAnimNextNormalizeKeyframeRotationsTask().Execute(VM);
	FAnimNextConvertRotationsToLocalSpaceTask::Make(1).Execute(VM);
	if (bLogEvaluation)
		UE_LOG(LogLootNPop, Log, TEXT("[UAFUpperBody] skeleton=%s bones=%d upperBones=%d rootWeight=%.1f meshRotation=1"),
			*GetNameSafe(Skeleton), Bones.GetNum(), UpperBones, Weights[0]);
}
