// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPSphericalOffsetRootTrait.h"

#include "Animation/AnimRootMotionProvider.h"
#include "EvaluationVM/EvaluationVM.h"
#include "HAL/IConsoleManager.h"
#include "OffsetRootBoneTrait.h" // UE 5.8 내부 상태 의존성. 기본 task 구현은 복사하지 않는다.

namespace
{
	EUAFOffsetRootBoneMode ToUAFMode(EOffsetRootBoneMode Mode)
	{
		switch (Mode)
		{
		case EOffsetRootBoneMode::Accumulate: return EUAFOffsetRootBoneMode::Accumulate;
		case EOffsetRootBoneMode::Interpolate: return EUAFOffsetRootBoneMode::Interpolate;
		default: return EUAFOffsetRootBoneMode::Release;
		}
	}

	bool NeedsBaseDelta(EOffsetRootBoneMode Mode)
	{
		return Mode == EOffsetRootBoneMode::Accumulate || Mode == EOffsetRootBoneMode::Interpolate;
	}
}

namespace UE::UAF
{
	AUTO_REGISTER_ANIM_TRAIT(FLNPSphericalOffsetRootTrait)

#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
	GeneratorMacro(IEvaluate)
	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FLNPSphericalOffsetRootTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
#undef TRAIT_INTERFACE_ENUMERATOR

	void FLNPSphericalOffsetRootTrait::PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
	{
		FInstanceData* Instance = Binding.GetInstanceData<FInstanceData>();
		FTraitBinding Base;
		if (!Binding.GetStackBaseTrait(Base) || !Base.IsValid() || Base.GetTrait()->GetTraitUID() != FOffsetRootBoneTrait::TraitUID)
		{
			if (!Instance->bReportedConfigurationError)
			{
				Context.Log(ELogVerbosity::Error, TEXT("Add LNP Spherical Offset Root to the same stack as the UAFWarping Offset Root Bone base trait."), TEXT("LNPSphericalOffsetRoot"));
				Instance->bReportedConfigurationError = true;
			}
			IEvaluate::PostEvaluate(Context, Binding);
			return;
		}

		const FSharedData* Shared = Binding.GetSharedData<FSharedData>();
		FLNPPrepareSphericalOffsetRootTask Task;
		Task.RootState = Base.GetInstanceData<FOffsetRootBoneTrait::FInstanceData>();
		Task.LastAppliedSerial = &Instance->LastAppliedSerial;
		Task.Inputs = Shared->GetRootInputs(Binding);
		if (!Task.Inputs.bValid && !Instance->bReportedConfigurationError)
		{
			Context.Log(ELogVerbosity::Error, TEXT("Missing valid OffsetRoot shared input; root offset evaluation is disabled."), TEXT("LNPSphericalOffsetRoot"));
			Instance->bReportedConfigurationError = true;
		}

		// 기본 trait는 아래 호출에서 핀 값을 복사한다. VM에서는 이 보정 task가 먼저 실행된다.
		const IConsoleVariable* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("a.UAF.OffsetRootBone.Enable"));
		if (!Enabled || Enabled->GetInt() != 0)
			Context.AppendTask(Task);
		IEvaluate::PostEvaluate(Context, Binding);
	}
}

void FLNPPrepareSphericalOffsetRootTask::Execute(UE::UAF::FEvaluationVM& VM) const
{
	using namespace UE::UAF;
	FOffsetRootBoneTrait::FInstanceData& State = *static_cast<FOffsetRootBoneTrait::FInstanceData*>(RootState);
	if (!Inputs.bValid)
	{
		State.Alpha = 0.0f;
		State.bIsFirstUpdate = true;
		return;
	}
	if (State.DeltaTime <= 0.0f || State.Alpha == 0.0f)
	{
		State.bIsFirstUpdate = true;
		return;
	}

	State.MeshComponentTransformWorld = Inputs.MeshComponentTransform;
	State.TranslationMode = ToUAFMode(Inputs.TranslationMode);
	State.RotationMode = ToUAFMode(Inputs.RotationMode);
	State.bOnGround = Inputs.bOnGround;
	State.AnimatedGroundNormal = Inputs.MeshComponentTransform.GetUnitAxis(EAxis::Z);
	State.MaxTranslationError = 30.0f;
	State.MaxRotationErrorDegrees = -1.0f;
	const auto SmoothingTime = [&State](float HalfLife)
	{
		return (FMath::Max(HalfLife, State.DeltaTime) + 1e-8f) / 0.69314718056f;
	};
	State.TranslationSmoothingTime = SmoothingTime(Inputs.TranslationHalfLife);
	State.RotationSmoothingTime = SmoothingTime(0.1f);

	// 기본 task는 루트모션 속성이 없을 때도 초기 상태를 만들므로 메시 변환은 먼저 공급한다.
	// 누적 상태와 base 소비 번호는 실제 루트모션을 평가할 수 있을 때만 갱신한다.
	TUniquePtr<FKeyframeState>* Keyframe = VM.PeekValueMutable<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0);
	const UE::Anim::IAnimRootMotionProvider* Provider = UE::Anim::IAnimRootMotionProvider::Get();
	FTransform RootMotion;
	if (!Keyframe || !Provider || !Provider->ExtractRootMotion(Keyframe->Get()->Attributes, RootMotion))
		return;

	const bool bNewInput = *LastAppliedSerial != Inputs.UpdateSerial;
	if (bNewInput && Inputs.bReset)
		State.bIsFirstUpdate = true;
	if (State.bIsFirstUpdate)
	{
		*LastAppliedSerial = Inputs.UpdateSerial;
		return; // 기본 task가 현재 메시 변환으로 초기화한다.
	}

	if (bNewInput)
	{
		if (NeedsBaseDelta(Inputs.TranslationMode))
			State.SimulatedTranslation += Inputs.BasedMovementDelta.GetTranslation();
		if (NeedsBaseDelta(Inputs.RotationMode))
			State.SimulatedRotation = Inputs.BasedMovementDelta.GetRotation() * State.SimulatedRotation;
		*LastAppliedSerial = Inputs.UpdateSerial;
	}

	FQuat Rotation = (FQuat::FindBetweenNormals(State.SimulatedRotation.GetAxisZ(), State.AnimatedGroundNormal) * State.SimulatedRotation).GetNormalized();
	const FQuat ComponentRotation = Inputs.MeshComponentTransform.GetRotation();
	FVector Axis;
	float Angle;
	(ComponentRotation.Inverse() * Rotation).GetNormalized().ToAxisAndAngle(Axis, Angle);
	const float Yaw = FMath::UnwindRadians(Axis.Z >= 0.0f ? Angle : -Angle);
	const float MaxYaw = FMath::DegreesToRadians(90.0f);
	if (FMath::Abs(Yaw) > MaxYaw)
		Rotation = (ComponentRotation * FQuat(FVector::UpVector, FMath::Sign(Yaw) * MaxYaw)).GetNormalized();
	State.SimulatedRotation = Rotation;
}
