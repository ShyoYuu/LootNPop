// Copyright (c) 2026 LootNPop. All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/UAFSpike/LNPUAFRifleAimTrait.h"
#include "Animation/Skeleton.h"
#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "HitDetection/LNPFireGeometry.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPUAFRifleAimTest, "LootNPop.Animation.UAF.RifleAim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPUAFRifleAimTest::RunTest(const FString& Parameters)
{
	using namespace UE::UAF;
	USkeleton* Skeleton = NewObject<USkeleton>();
	{
		FReferenceSkeletonModifier Modifier(Skeleton);
		const TCHAR* Names[] = { TEXT("root"), TEXT("pelvis"), TEXT("spine_03"), TEXT("hand_l"), TEXT("hand_r"), TEXT("weapon_r"), TEXT("foot_l") };
		const int32 Parents[] = { INDEX_NONE, 0, 1, 2, 2, 4, 1 };
		for (int32 Index = 0; Index < 7; ++Index)
			Modifier.Add(FMeshBoneInfo(Names[Index], Names[Index], Parents[Index]), FTransform::Identity);
	}
	for (const bool bReordered : { false, true })
	{
		FMemMark Mark(FMemStack::Get());
		const TArray<FBoneIndexType> ToSkeleton = bReordered ? TArray<FBoneIndexType>{ 0, 1, 6, 2, 4, 5, 3 } : TArray<FBoneIndexType>{ 0, 1, 2, 3, 4, 5, 6 };
		TArray<FBoneIndexType> ToCompact;
		ToCompact.SetNum(7);
		for (int32 Index = 0; Index < 7; ++Index)
			ToCompact[ToSkeleton[Index]] = Index;
		TArray<FBoneIndexType> Parents;
		for (int32 Index = 0; Index < 7; ++Index)
		{
			const int32 Parent = Skeleton->GetReferenceSkeleton().GetParentIndex(ToSkeleton[Index]);
			Parents.Add(Parent == INDEX_NONE ? INVALID_BONE_INDEX : ToCompact[Parent]);
		}
		UE::UAF::FReferencePose Ref;
		TMap<FName, FBoneIndexType> Names;
		for (int32 Index = 0; Index < 7; ++Index)
			Names.Add(Skeleton->GetReferenceSkeleton().GetBoneName(Index), ToCompact[Index]);
		Ref.Initialize(Skeleton->GetReferenceSkeleton(), { Parents }, { ToSkeleton }, { ToSkeleton }, ToCompact, ToCompact, { 7 }, Names, {}, false);
		Ref.Skeleton = Skeleton;
		FEvaluationVM VM(EEvaluationFlags::All, Ref, 0);
		const auto GetWorld = [](const FKeyframeState& Keyframe, FBoneIndexType Index, const FTransform& Mesh)
		{
			FTransform Result = FTransform::Identity;
			for (; Index != INVALID_BONE_INDEX; Index = Keyframe.Pose.GetLODBoneParentIndex(Index))
				Result = Result * FTransform(Keyframe.Pose.LocalTransformsView[Index]);
			return Result * Mesh;
		};
		for (const FRotator MeshRotation : { FRotator::ZeroRotator, FRotator(23, 71, 90), FRotator(-75, -120, 34) })
		{
			for (const double Distance : { 100.0, 1064.0, 14220.0, -100.0, 10.0, 2000.0 })
			{
				auto Base = MakeUnique<FKeyframeState>(VM.MakeReferenceKeyframe(false));
				Base->Pose.LocalTransforms.Rotations[0] = FRotator(7, -9, 5).Quaternion();
				Base->Pose.LocalTransforms.Rotations[ToCompact[2]] = FRotator(-2, 3, 1).Quaternion();
				Base->Pose.LocalTransforms.Translations[ToCompact[2]] = FVector(0, 0, 130);
				Base->Pose.LocalTransforms.Translations[ToCompact[3]] = FVector(40, -18, 0);
				Base->Pose.LocalTransforms.Translations[ToCompact[4]] = FVector(40, 18, 0);
				Base->Pose.LocalTransforms.Translations[ToCompact[5]] = FVector(12, 0, 0);
				Base->Pose.LocalTransforms.Translations[ToCompact[6]] = FVector(0, 15, 0);
				Base->Curves.Add(TEXT("Sentinel"), 0.37f);
				FLNPUAFRifleAimTask Task;
				Task.MontageDemand = 0;
				Task.Inputs.bEnabled = true;
				Task.Inputs.MeshWorld = FTransform(MeshRotation, FVector(45000, -20000, 8000));
				Task.Inputs.MuzzleInWeaponBone = FTransform(FRotator(1, -2, 0), FVector(45, 0, 0));
				Task.Inputs.MuzzleOffsetWorld = Task.Inputs.MeshWorld.TransformVector(FVector(3, -2, 1));
				const FTransform InitialMuzzle = Task.Inputs.MuzzleInWeaponBone * GetWorld(*Base, ToCompact[5], Task.Inputs.MeshWorld);
				Task.Inputs.ViewDirection = Task.Inputs.MeshWorld.GetUnitAxis(EAxis::X);
				Task.Inputs.AimTarget = InitialMuzzle.GetLocation() + Task.Inputs.ViewDirection * Distance
					+ Task.Inputs.MeshWorld.TransformVector(FVector(0, Distance == 2000.0 ? 250 : 35, 20));
				for (const float Demand : { 0.0f, 0.5f, 1.0f })
				{
					// 슬롯 차단과 독립 커브 차단을 같은 포즈에 각각 적용한다.
					for (const bool bCurve : { false, true })
					{
						auto Frame = MakeUnique<FKeyframeState>(VM.MakeReferenceKeyframe(false));
						Frame->Pose.CopyFrom(Base->Pose);
						Frame->Curves.CopyFrom(Base->Curves);
						Frame->Curves.Add(TEXT("DisableLHandIK"), bCurve ? Demand : 0.0f);
						Task.MontageDemand = bCurve ? 0.0f : Demand;
						VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(Frame));
						Task.Execute(VM);
						TUniquePtr<FKeyframeState> Output;
						TestTrue(TEXT("One output remains"), VM.PopValue(KEYFRAME_STACK_NAME, Output));
						for (int32 Bone = 0; Bone < 7; ++Bone)
						{
							const FTransform Before = Base->Pose.LocalTransformsView[Bone];
							const FTransform After = Output->Pose.LocalTransformsView[Bone];
							if (Bone != ToCompact[2] || Demand == 1.0f)
								TestTrue(TEXT("Only the spine rotation may change; full suppression is exact"), Before.Equals(After, 1.e-6));
							TestTrue(TEXT("All local translations and scales preserved"), Before.GetLocation().Equals(After.GetLocation(), 1.e-6)
								&& Before.GetScale3D().Equals(After.GetScale3D(), 1.e-6));
						}
						TestEqual(TEXT("Unrelated curve preserved"), Output->Curves.Get(TEXT("Sentinel")), 0.37f);
						const FTransform Muzzle = Task.Inputs.MuzzleInWeaponBone * GetWorld(*Output, ToCompact[5], Task.Inputs.MeshWorld);
						if (Demand == 0.0f)
						{
							const FVector Desired = LNPFireGeometry::ResolveAimDirection(Muzzle.GetLocation() + Task.Inputs.MuzzleOffsetWorld,
								Task.Inputs.ViewDirection, &Task.Inputs.AimTarget);
							if (Distance < 50.0 || Distance == 2000.0)
								TestTrue(TEXT("Rejected targets preserve the view direction fallback"), Desired.Equals(Task.Inputs.ViewDirection, 1.e-6));
							const double Error = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Muzzle.GetUnitAxis(EAxis::X), Desired), -1.0, 1.0)));
							TestTrue(*FString::Printf(TEXT("Full alignment distance=%.0f error=%.9fdeg"), Distance, Error), Error < 0.01);
						}
						const FTransform LeftBefore = GetWorld(*Base, ToCompact[3], Task.Inputs.MeshWorld).GetRelativeTransform(InitialMuzzle);
						const FTransform LeftAfter = GetWorld(*Output, ToCompact[3], Task.Inputs.MeshWorld).GetRelativeTransform(Muzzle);
						TestTrue(TEXT("Left hand relative to muzzle preserved"), LeftBefore.Equals(LeftAfter, 1.e-5));
						const FTransform RightBefore = GetWorld(*Base, ToCompact[4], Task.Inputs.MeshWorld).GetRelativeTransform(InitialMuzzle);
						const FTransform RightAfter = GetWorld(*Output, ToCompact[4], Task.Inputs.MeshWorld).GetRelativeTransform(Muzzle);
						TestTrue(TEXT("Right hand relative to muzzle preserved"), RightBefore.Equals(RightAfter, 1.e-5));
					}
				}
			}
		}
	}
	return true;
}

#endif
