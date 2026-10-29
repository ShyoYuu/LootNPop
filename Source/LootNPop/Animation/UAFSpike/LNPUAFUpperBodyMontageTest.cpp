// Copyright (c) 2026 LootNPop. All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/UAFSpike/LNPUAFUpperBodyMontageTrait.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "Animation/Skeleton.h"
#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "EvaluationVM/Tasks/StoreKeyframe.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPUAFUpperBodyMontageTest, "LootNPop.Animation.UAF.UpperBodyMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPUAFUpperBodyMontageTest::RunTest(const FString& Parameters)
{
	using namespace UE::UAF;
	using namespace UE::Anim;
	USkeleton* Skeleton = NewObject<USkeleton>();
	{
		FReferenceSkeletonModifier Modifier(Skeleton);
		const TCHAR* Names[] = { TEXT("root"), TEXT("pelvis"), TEXT("spine_01"), TEXT("hand_l"), TEXT("foot_l") };
		const int32 Parents[] = { INDEX_NONE, 0, 1, 2, 1 };
		for (int32 Index = 0; Index < 5; ++Index)
			Modifier.Add(FMeshBoneInfo(Names[Index], Names[Index], Parents[Index]), FTransform::Identity);
	}

	// 두 번째는 발이 척추보다 먼저인 평가 순서다. 속성에 스켈레톤 인덱스를 잘못 쓰면 실패한다.
	for (const bool bReordered : { false, true })
	{
		FMemMark Mark(FMemStack::Get());
		const TArray<FBoneIndexType> ToSkeleton = bReordered ? TArray<FBoneIndexType>{ 0, 1, 4, 2, 3 } : TArray<FBoneIndexType>{ 0, 1, 2, 3, 4 };
		TArray<FBoneIndexType> ToCompact;
		ToCompact.SetNum(5);
		for (int32 Index = 0; Index < 5; ++Index)
			ToCompact[ToSkeleton[Index]] = Index;
		TArray<FBoneIndexType> Parents;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const int32 Parent = Skeleton->GetReferenceSkeleton().GetParentIndex(ToSkeleton[Index]);
			Parents.Add(Parent == INDEX_NONE ? static_cast<FBoneIndexType>(INDEX_NONE) : ToCompact[Parent]);
		}
		UE::UAF::FReferencePose Ref;
		Ref.Initialize(Skeleton->GetReferenceSkeleton(), { Parents }, { ToSkeleton }, { ToSkeleton }, ToCompact, ToCompact, { 5 }, {}, {}, false);
		Ref.Skeleton = Skeleton;
		FEvaluationVM VM(EEvaluationFlags::All, Ref, 0);
		auto Source = MakeUnique<FKeyframeState>(VM.MakeReferenceKeyframe(false));
		auto Montage = MakeUnique<FKeyframeState>(VM.MakeReferenceKeyframe(false));
		const FQuat BaseRoot(FVector::ForwardVector, 0.9), BasePelvis(FVector::RightVector, 0.2);
		const FQuat MontageRoot(FVector::ForwardVector, -0.3), MontagePelvis(FVector::UpVector, 0.4), MontageSpine(FVector::RightVector, 0.7);
		Source->Pose.LocalTransforms.Rotations[0] = BaseRoot;
		Source->Pose.LocalTransforms.Rotations[1] = BasePelvis;
		Montage->Pose.LocalTransforms.Rotations[0] = MontageRoot;
		Montage->Pose.LocalTransforms.Rotations[1] = MontagePelvis;
		Montage->Pose.LocalTransforms.Rotations[ToCompact[2]] = MontageSpine;
		Source->Pose.LocalTransforms.Translations[ToCompact[4]] = FVector(11, 0, 0);
		Montage->Pose.LocalTransforms.Translations[ToCompact[4]] = FVector(-7, 0, 0);
		Montage->Pose.LocalTransforms.Translations[ToCompact[3]] = FVector(0, 13, 0);
		Source->Curves.Add(TEXT("BaseOnly"), 2.0f);
		Source->Curves.Add(TEXT("Shared"), 1.0f);
		Montage->Curves.Add(TEXT("Shared"), 7.0f);
		Montage->Curves.Add(TEXT("DisableLHandIK"), 0.8f);
		for (const int32 Bone : { 0, 3, 4 })
		{
			const FAttributeId Id(TEXT("MaskProbe"), FCompactPoseBoneIndex(ToCompact[Bone]));
			FFloatAnimationAttribute BaseValue, MontageValue;
			BaseValue.Value = 1.0f;
			MontageValue.Value = 7.0f;
			Source->Attributes.Add<FFloatAnimationAttribute>(Id, BaseValue);
			Montage->Attributes.Add<FFloatAnimationAttribute>(Id, MontageValue);
		}

		VM.PushValue(KEYFRAME_STACK_NAME, MakeUnique<FKeyframeState>(VM.MakeReferenceKeyframe(false)));
		VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(Source));
		FAnimNextDuplicateTopKeyframeTask::Make().Execute(VM);
		// native 몽타주가 원본 사본을 소비하고 결과 하나를 남긴 상태를 재현한다.
		TUniquePtr<FKeyframeState> SourceCopy;
		TestTrue(TEXT("Source copy exists"), VM.PopValue(KEYFRAME_STACK_NAME, SourceCopy));
		TestEqual(TEXT("Source copy preserves curves"), SourceCopy->Curves.Get(TEXT("BaseOnly")), 2.0f);
		VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(Montage));
		FLNPUAFBlendUpperBodyTask().Execute(VM);
		TUniquePtr<FKeyframeState> Output;
		TestTrue(TEXT("One blended output"), VM.PopValue(KEYFRAME_STACK_NAME, Output));
		TestTrue(TEXT("Tilted source root preserved"), Output->Pose.LocalTransforms.Rotations[0].Equals(BaseRoot, 1.e-6));
		TestTrue(TEXT("Source pelvis preserved"), Output->Pose.LocalTransforms.Rotations[1].Equals(BasePelvis, 1.e-6));
		TestTrue(TEXT("Lower body translation preserved"), Output->Pose.LocalTransforms.Translations[ToCompact[4]].Equals(FVector(11, 0, 0), 1.e-6));
		TestTrue(TEXT("Upper body local translation applied"), Output->Pose.LocalTransforms.Translations[ToCompact[3]].Equals(FVector(0, 13, 0), 1.e-6));
		TestTrue(TEXT("Spine mesh space rotation matches montage"),
			(BaseRoot * BasePelvis * Output->Pose.LocalTransforms.Rotations[ToCompact[2]]).Equals(MontageRoot * MontagePelvis * MontageSpine, 1.e-6));
		TestEqual(TEXT("Source only curve preserved"), Output->Curves.Get(TEXT("BaseOnly")), 2.0f);
		TestEqual(TEXT("Montage curve overrides source"), Output->Curves.Get(TEXT("Shared")), 7.0f);
		TestEqual(TEXT("Hand IK curve preserved"), Output->Curves.Get(TEXT("DisableLHandIK")), 0.8f);
		for (const int32 Bone : { 0, 3, 4 })
		{
			const auto* Value = Output->Attributes.Find<FFloatAnimationAttribute>(FAttributeId(TEXT("MaskProbe"), FCompactPoseBoneIndex(ToCompact[Bone])));
			TestNotNull(TEXT("Attribute preserved"), Value);
			if (Value)
				TestEqual(TEXT("Root and foot attributes use source; hand uses montage"), Value->Value, Bone == 3 ? 7.0f : 1.0f);
		}
		TestTrue(TEXT("Lower stack sentinel preserved"), VM.PopValue(KEYFRAME_STACK_NAME, Output));
		TestFalse(TEXT("No extra keyframes"), VM.PopValue(KEYFRAME_STACK_NAME, Output));
	}
	return true;
}

#endif
