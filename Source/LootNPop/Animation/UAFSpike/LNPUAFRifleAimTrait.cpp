// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFRifleAimTrait.h"

#include "Animation/UAFSpike/LNPUAFMontageIKTrait.h"
#include "Animation/UAFSpike/LNPUAFRootFeedback.h"
#include "Character/LNPCharacterBase.h"
#include "Component/AnimNextComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "HAL/IConsoleManager.h"
#include "HitDetection/LNPFireGeometry.h"
#include "HitDetection/LNPProjectileMotion.h"
#include "LootNPop.h"
#include "Module/AnimNextModuleInstance.h"
#include "Module/UAFWeakSystemReference.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

namespace
{
	TAutoConsoleVariable<int32> CVarRifleAim(TEXT("LNP.UAFSpike.RifleAim"), 1,
		TEXT("Enable precise barrel alignment on the UAF rifle trial. 0: off, 1: on."));

	FTransform ComponentTransform(const UE::UAF::FLODPose& Pose, FBoneIndexType Bone)
	{
		FTransform Result = FTransform::Identity;
		for (; Bone != INVALID_BONE_INDEX; Bone = Pose.GetLODBoneParentIndex(Bone))
			Result = Result * FTransform(Pose.LocalTransformsView[Bone]);
		return Result;
	}
}

namespace UE::UAF
{
	AUTO_REGISTER_ANIM_TRAIT(FLNPUAFRifleAimTrait)

#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
	GeneratorMacro(IEvaluate)
	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FLNPUAFRifleAimTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
#undef TRAIT_INTERFACE_ENUMERATOR

	void FLNPUAFRifleAimTrait::PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
	{
		IEvaluate::PostEvaluate(Context, Binding);
		FLNPUAFRifleAimTask Task;
		Task.Inputs = Binding.GetSharedData<FSharedData>()->GetInputs(Binding);
		if (FAnimNextModuleInstance* Module = Context.GetRootGraphInstance().GetModuleInstance())
		{
			if (const auto* Feedback = Module->TryGetComponent<FLNPUAFHandIKFeedback>())
			{
				const FLNPUAFHandIKSnapshot Snapshot = Feedback->ReadCurrentUpdate();
				if (Snapshot.bValid)
					Task.MontageDemand = Snapshot.GetDemand(0.0f);
			}
			const IConsoleVariable* FireDebug = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.Projectile.DebugFireAim"));
			if (FireDebug && FireDebug->GetInt() != 0)
				Task.Feedback = Module->TryGetComponent<FLNPUAFRootFeedback>();
		}
		const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.Anim.DebugLocomotion"));
		Task.bLog = Debug && Debug->GetInt() >= 2 && GFrameCounter % 60 == 0;
		Context.AppendTask(MoveTemp(Task));
	}
}

void FLNPUAFRifleAimTask::Execute(UE::UAF::FEvaluationVM& VM) const
{
	using namespace UE::UAF;
	if (!ensureMsgf(!VM.GetActiveNamedSet(), TEXT("LNP Rifle Aim requires the ordinary skeleton runtime."))
		|| !EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
		return;
	const auto* Keyframe = VM.PeekValue<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0);
	if (!Keyframe || !Keyframe->IsValid())
		return;
	const float Curve = (*Keyframe)->Curves.Get(TEXT("DisableLHandIK"));
	const float Alpha = Inputs.bEnabled && CVarRifleAim.GetValueOnAnyThread() != 0
		? 1.0f - FMath::Clamp(FMath::Max(MontageDemand, Curve), 0.0f, 1.0f) : 0.0f;
	FLNPUAFRifleAimSnapshot Snapshot;
	Snapshot.EvaluationFrame = GFrameCounter;
	Snapshot.Alpha = Alpha;
	Snapshot.MeshWorld = Inputs.MeshWorld;
	Snapshot.AimTarget = Inputs.AimTarget;
	Snapshot.ViewDirection = Inputs.ViewDirection;
	if (bLog)
		UE_LOG(LogLootNPop, Log, TEXT("[UAFRifleAim] enabled=%d alpha=%.6f montage=%.6f curve=%.6f"),
			Inputs.bEnabled, Alpha, MontageDemand, Curve);
	if (Alpha <= 0.0f)
	{
		if (Feedback)
			Feedback->PublishRifleAim(Snapshot);
		return;
	}

	auto& Pose = (*Keyframe)->Pose;
	const FBoneIndexType Spine = Pose.FindLODBoneIndexFromBoneName(TEXT("spine_03"));
	const FBoneIndexType Weapon = Pose.FindLODBoneIndexFromBoneName(TEXT("weapon_r"));
	if (!ensureMsgf(Spine != INVALID_BONE_INDEX && Weapon != INVALID_BONE_INDEX
		&& Spine < Pose.GetNumBones() && Weapon < Pose.GetNumBones() && Pose.IsBoneChildOf(Weapon, Spine),
		TEXT("LNP Rifle Aim requires weapon_r below spine_03 in the evaluated LOD.")))
		return;
	const FTransform SpineWorld = ComponentTransform(Pose, Spine) * Inputs.MeshWorld;
	const FTransform MuzzleWorld = Inputs.MuzzleInWeaponBone * ComponentTransform(Pose, Weapon) * Inputs.MeshWorld;
	const FVector Pivot = SpineWorld.GetLocation();
	const FVector Lever = MuzzleWorld.GetLocation() - Pivot;
	const FVector Barrel = MuzzleWorld.GetUnitAxis(EAxis::X);
	const auto SpawnPosition = [&](const FQuat& Rotation)
	{
		return LNPProjectileMotion::ClipMuzzleToWorld(Inputs.WorldCollision, Inputs.ActorLocation,
			Pivot + Rotation.RotateVector(Lever) + Inputs.MuzzleOffsetWorld);
	};
	const auto AimDirection = [&](const FQuat& Rotation)
	{
		return LNPFireGeometry::ResolveAimDirection(SpawnPosition(Rotation), Inputs.ViewDirection, &Inputs.AimTarget);
	};

	// 피벗에서 표적까지의 구와 총열 반직선의 교점을 회전시킨다. 총구 이동까지 함께 푼다.
	// 평행 시선 폴백으로 시작하고, 보정 후에도 실제 발사 게이트를 통과하는 표적 해만 채택한다.
	FQuat Correction = FQuat::FindBetweenNormals(Barrel, Inputs.ViewDirection);
	const FVector ToTarget = Inputs.AimTarget - Pivot - Inputs.MuzzleOffsetWorld;
	const double Projection = FVector::DotProduct(Lever, Barrel);
	const double Discriminant = Projection * Projection + ToTarget.SizeSquared() - Lever.SizeSquared();
	if (Discriminant >= 0.0)
	{
		const double AlongBarrel = FMath::Sqrt(Discriminant) - Projection;
		if (AlongBarrel >= 0.0 && !ToTarget.IsNearlyZero())
		{
			const FQuat Candidate = FQuat::FindBetweenNormals((Lever + AlongBarrel * Barrel).GetSafeNormal(), ToTarget.GetSafeNormal());
			if (Candidate.RotateVector(Barrel).Equals(AimDirection(Candidate), 1.e-7))
				Correction = Candidate;
		}
	}
	// 벽에 의해 발사 원점이 잘린 경우도 같은 발사 기하로 다시 푼다. 이전 프레임 보정은 누적하지 않는다.
	for (int32 Iteration = 0; Iteration < 8; ++Iteration)
	{
		const FVector Current = Correction.RotateVector(Barrel);
		const FVector Desired = AimDirection(Correction);
		if (Current.Equals(Desired, 1.e-7))
			break;
		Correction = (FQuat::FindBetweenNormals(Current, Desired) * Correction).GetNormalized();
	}
	const FQuat Applied = FQuat::Slerp(FQuat::Identity, Correction, Alpha).GetNormalized();
	const FBoneIndexType Parent = Pose.GetLODBoneParentIndex(Spine);
	const FQuat ParentWorld = (ComponentTransform(Pose, Parent) * Inputs.MeshWorld).GetRotation();
	Pose.LocalTransforms.Rotations[Spine] = (ParentWorld.Inverse() * Applied * SpineWorld.GetRotation()).GetNormalized();
	if (Feedback)
	{
		const FTransform OutputMuzzle = Inputs.MuzzleInWeaponBone * ComponentTransform(Pose, Weapon) * Inputs.MeshWorld;
		Snapshot.bHasPose = true;
		Snapshot.MuzzleWorld = OutputMuzzle.GetLocation();
		Snapshot.BarrelWorld = OutputMuzzle.GetUnitAxis(EAxis::X);
		Snapshot.SpawnWorld = SpawnPosition(Applied);
		Feedback->PublishRifleAim(Snapshot);
	}
	if (bLog)
	{
		const double Error = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
			FVector::DotProduct(Applied.RotateVector(Barrel), AimDirection(Applied)), -1.0, 1.0)));
		UE_LOG(LogLootNPop, Log, TEXT("[UAFRifleAimResult] correctionDeg=%.6f residualDeg=%.6f"),
			FMath::RadiansToDegrees(Applied.GetAngle()), Error);
	}
}

void LNPUAFRifleAim::LogFireComparison(const ALNPCharacterBase& Character, const FVector& Muzzle, const FVector& Barrel,
	const FVector& Spawn, const FVector& Shot, const FVector& View, const FVector* AimTarget)
{
#if !UE_BUILD_SHIPPING
	const UUAFComponent* UAF = Character.FindComponentByClass<UUAFComponent>();
	if (!UAF || !AimTarget)
		return;
	const FUAFWeakSystemReference Reference = UAF->GetSystemReference();
	if (!Reference.IsValid())
		return;
	FLNPUAFRifleAimSnapshot Snapshot;
	Reference.ReadComponent<FLNPUAFRootFeedback>([&Snapshot](TConstStructView<FLNPUAFRootFeedback> Feedback)
	{
		Snapshot = Feedback.Get().ReadRifleAim();
	});
	const auto* Parent = Cast<USkeletalMeshComponent>(Character.GetWeaponMesh()->GetAttachParent());
	if (!Snapshot.bHasPose || !Parent)
		return;
	const FTransform MeshNow = Parent->GetComponentTransform();
	const FVector RebasedMuzzle = MeshNow.TransformPosition(Snapshot.MeshWorld.InverseTransformPosition(Snapshot.MuzzleWorld));
	const FVector RebasedBarrel = MeshNow.TransformVectorNoScale(Snapshot.MeshWorld.InverseTransformVectorNoScale(Snapshot.BarrelWorld));
	const auto Angle = [](const FVector& A, const FVector& B)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(A.GetSafeNormal(), B.GetSafeNormal()), -1.0, 1.0)));
	};
	const FVector StoredAimDirection = LNPFireGeometry::ResolveAimDirection(Spawn, Snapshot.ViewDirection, &Snapshot.AimTarget);
	UE_LOG(LogLootNPop, Display, TEXT("[FireAimPose] owner=%s frame=%llu evalFrame=%llu frameDelta=%lld serial=%lld alpha=%.6f targetDeltaCm=%.6f viewDeltaDeg=%.6f meshDeltaCm=%.6f meshDeltaDeg=%.6f muzzleRebasedDeltaCm=%.6f barrelRebasedDeltaDeg=%.6f rebasedShotDeg=%.6f storedAimShotDeg=%.6f spawnDeltaCm=%.6f"),
		*Character.GetName(), GFrameCounter, Snapshot.EvaluationFrame, static_cast<int64>(GFrameCounter) - static_cast<int64>(Snapshot.EvaluationFrame),
		Snapshot.InputSerial, Snapshot.Alpha, FVector::Dist(*AimTarget, Snapshot.AimTarget), Angle(View, Snapshot.ViewDirection),
		FVector::Dist(MeshNow.GetLocation(), Snapshot.MeshWorld.GetLocation()), FMath::RadiansToDegrees(MeshNow.GetRotation().AngularDistance(Snapshot.MeshWorld.GetRotation())),
		FVector::Dist(Muzzle, RebasedMuzzle), Angle(Barrel, RebasedBarrel), Angle(Shot, RebasedBarrel), Angle(Shot, StoredAimDirection), FVector::Dist(Spawn, Snapshot.SpawnWorld));
	UE_LOG(LogLootNPop, Display, TEXT("[FireAimPoseInputs] frame=%llu evalTarget=%s fireTarget=%s evalView=%s fireView=%s evalMuzzle=%s fireMuzzle=%s evalMesh=%s fireMesh=%s evalBarrel=%s fireBarrel=%s shot=%s"),
		GFrameCounter, *Snapshot.AimTarget.ToString(), *AimTarget->ToString(), *Snapshot.ViewDirection.ToString(), *View.ToString(),
		*Snapshot.MuzzleWorld.ToString(), *Muzzle.ToString(), *Snapshot.MeshWorld.GetLocation().ToString(), *MeshNow.GetLocation().ToString(),
		*Snapshot.BarrelWorld.ToString(), *Barrel.ToString(), *Shot.ToString());
#endif
}
