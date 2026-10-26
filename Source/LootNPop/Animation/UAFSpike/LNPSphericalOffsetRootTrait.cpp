// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPSphericalOffsetRootTrait.h"

#include "Animation/AnimRootMotionProvider.h"
#include "Animation/AnimSequence.h"
#include "AnimationWarpingLibrary.h"
#include "EvaluationVM/EvaluationVM.h"
#include "HAL/IConsoleManager.h"
#include "Math/SpringMath.h"
#include "LootNPop.h"
#include "MotionMatchingTrait.h" // UE 5.8 내부 검색 상태. 숨김 ABP의 DB 태그는 사용하지 않는다.
#include "OffsetRootBoneTrait.h" // UE 5.8 내부 상태 의존성. 기본 task 구현은 복사하지 않는다.
#include "StrafeWarpingTrait.h" // 기본 Strafe Warping task에 입력만 전달한다.
#include "PoseSearch/PoseSearchDatabase.h"
#include "TraitInterfaces/IBlendStack.h"
#include "TraitInterfaces/IDiscreteBlend.h"
#include "TraitInterfaces/ITimeline.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
	TAutoConsoleVariable<int32> CVarSpikeSteering(TEXT("LNP.UAFSpike.Steering"), 1,
		TEXT("Enable steering before the UAF spike offset root. 0: off, 1: on."));
	TAutoConsoleVariable<int32> CVarSpikeStrafeWarping(TEXT("LNP.UAFSpike.StrafeWarping"), 1,
		TEXT("Enable native UAF strafe warping in the spike graph. 0: off, 1: on."));

	bool IsSpikeOffsetRootEnabled()
	{
		const IConsoleVariable* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("a.UAF.OffsetRootBone.Enable"));
		return !Enabled || Enabled->GetInt() != 0;
	}

	// UAF SteeringTrait의 회전 스케일·스프링 순서. 평가 출력에 적용하므로 샘플별 ABP와 동등하다고 간주하지 않는다.
	void SteerRootMotion(FTransform& RootMotion, const FQuat& RootRotation, const FQuat& Target,
		const FQuat& PredictedRotation, float ProceduralTime, float DeltaTime, FVector& AngularVelocity)
	{
		FQuat Delta = RootRotation.Inverse() * Target;
		FRotator Predicted = PredictedRotation.Rotator();
		if (FMath::Abs(Predicted.Yaw) > 1.0f)
		{
			float TargetYaw = Delta.Rotator().Yaw;
			if (Predicted.Yaw - TargetYaw > 180.0f) TargetYaw += 360.0f;
			else if (TargetYaw - Predicted.Yaw > 180.0f) TargetYaw -= 360.0f;
			const float Ratio = FMath::Clamp(TargetYaw / Predicted.Yaw, 0.5f, 1.5f);
			FRotator FrameRotation = RootMotion.GetRotation().Rotator();
			FrameRotation.Yaw *= Ratio;
			RootMotion.SetRotation(FrameRotation.Quaternion());
			Predicted.Yaw *= Ratio;
			Delta = Predicted.Quaternion().Inverse() * RootRotation.Inverse() * Target;
		}
		FQuat Correction = FQuat::Identity;
		SpringMath::CriticalSpringDamperQuat(Correction, AngularVelocity, Delta,
			SpringMath::HalfLifeToSmoothingTime(ProceduralTime), DeltaTime);
		RootMotion.SetRotation((RootMotion.GetRotation() * Correction).GetNormalized());
	}

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

	FQuat ConstrainSphericalRootRotation(const FQuat& Rotation, const FQuat& ComponentRotation)
	{
		const FQuat Aligned = (FQuat::FindBetweenNormals(Rotation.GetAxisZ(), ComponentRotation.GetAxisZ()) * Rotation).GetNormalized();
		const FQuat Local = (ComponentRotation.Inverse() * Aligned).GetNormalized();
		const double Yaw = FMath::UnwindRadians(2.0 * FMath::Atan2(Local.Z, Local.W));
		return (ComponentRotation * FQuat(FVector::UpVector, FMath::Clamp(Yaw, -UE_HALF_PI, UE_HALF_PI))).GetNormalized();
	}

	void FinalizeSphericalRootRotation(FQuat& StateRotation, FTransform& PoseRoot, const FQuat& ComponentRotation)
	{
		const FQuat Corrected = ConstrainSphericalRootRotation(StateRotation, ComponentRotation);
		// 출력을 상태 회전으로 덮지 않는다. 상태의 보정 차이만 로컬 공간으로 옮겨 원본 루트의 상대 회전을 보존한다.
		const FQuat LocalCorrection = ComponentRotation.Inverse() * Corrected * StateRotation.Inverse() * ComponentRotation;
		PoseRoot.SetRotation((LocalCorrection * PoseRoot.GetRotation()).GetNormalized());
		StateRotation = Corrected;
	}
}

namespace UE::UAF
{
	AUTO_REGISTER_ANIM_TRAIT(FLNPSphericalOffsetRootTrait)

#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
	GeneratorMacro(IEvaluate) \
	GeneratorMacro(ILNPSpikeRootScope)
	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FLNPSphericalOffsetRootTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
#undef TRAIT_INTERFACE_ENUMERATOR

	void FLNPSphericalOffsetRootTrait::PreEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
	{
		Binding.GetInstanceData<FInstanceData>()->bPreparationScheduled = false;
		FTraitBinding Base;
		if (Binding.GetStackBaseTrait(Base) && Base.IsValid() && Base.GetTrait()->GetTraitUID() == FOffsetRootBoneTrait::TraitUID)
			Context.PushScopedInterface<ILNPSpikeRootScope>(Binding);
		IEvaluate::PreEvaluate(Context, Binding);
	}

	FLNPPrepareSphericalOffsetRootTask FLNPSphericalOffsetRootTrait::MakePreparationTask(FEvaluateTraversalContext& Context, const FTraitBinding& Binding)
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
			return {};
		}

		const FSharedData* Shared = Binding.GetSharedData<FSharedData>();
		FLNPPrepareSphericalOffsetRootTask Task;
		Task.RootState = Base.GetInstanceData<FOffsetRootBoneTrait::FInstanceData>();
		Task.LastAppliedSerial = &Instance->LastAppliedSerial;
		Task.Inputs = Shared->GetRootInputs(Binding);
		Task.SteeringAngularVelocities = Instance->SteeringAngularVelocities;
		Task.SteeringDebugElapsed = &Instance->SteeringDebugElapsed;
		// Bool 선택기는 기본 인터페이스 전달을 막으므로 목적 자식의 MM만 조회한다.
		FTraitStackBinding Stack;
		TTraitBinding<IBlendStack> BlendStack;
		bool bHasBlendStack = IHierarchy::GetForwardedStackInterface<IBlendStack>(Context, Binding, Stack, BlendStack);
		if (!bHasBlendStack)
		{
			FTraitStackBinding SelectorStack;
			TTraitBinding<IDiscreteBlend> Selector;
			if (IHierarchy::GetForwardedStackInterface<IDiscreteBlend>(Context, Binding, SelectorStack, Selector))
			{
				FChildrenArray Children;
				IHierarchy::GetStackChildren(Context, SelectorStack, Children);
				const int32 Destination = Selector.GetBlendDestinationChildIndex(Context);
				FTraitStackBinding ChildStack;
				if (Children.IsValidIndex(Destination) && Context.GetStack(Children[Destination], ChildStack))
					bHasBlendStack = IHierarchy::GetForwardedStackInterface<IBlendStack>(Context, ChildStack, Stack, BlendStack);
			}
		}
		// MM 스택에서 같은 활성 애니의 시간을 읽는다. 공중 루프를 이전 지상 검색 결과와 섞지 않는다.
		FTraitStackBinding TimelineStack;
		TTraitBinding<ITimeline> Timeline;
		FTimelineState Time;
		const bool bHasTimeline = bHasBlendStack
			&& IHierarchy::GetForwardedStackInterface<ITimeline>(Context, Stack, TimelineStack, Timeline)
			&& Timeline.GetState(Context, Time);
		if (bHasTimeline)
			Task.CurrentAnimAssetTime = Time.GetPosition();
		if (bHasBlendStack)
		{
			IBlendStack::FGraphRequestPtr ActiveGraph = nullptr;
			BlendStack.GetActiveGraph(Context, ActiveGraph);
			const UAnimSequence* Sequence = ActiveGraph ? Cast<UAnimSequence>(ActiveGraph->FactoryObject) : nullptr;
			for (uint32 Index = 0; Index < Stack.GetNumTraits(); ++Index)
			{
				FTraitBinding Candidate;
				if (Stack.GetTrait(Index, Candidate) && Candidate.GetTrait()->GetTraitUID() == FMotionMatchingTrait::TraitUID)
				{
					const auto& Result = Candidate.GetInstanceData<FMotionMatchingTrait::FInstanceData>()->MotionMatchingState.SearchResult;
					Task.bTurnInPlace = Result.SelectedDatabase && Result.SelectedDatabase->Tags.Contains(TEXT("TurnInPlace"));
					Task.bHasMotionMatchingResult = Result.SelectedDatabase && Sequence && bHasTimeline
						&& ActiveGraph->FactoryObject == Result.SelectedAnim;
					Task.SelectedDatabase = FName(*GetNameSafe(Result.SelectedDatabase));
					Task.SelectedAnimation = FName(*GetNameSafe(Result.SelectedAnim));
					if (Task.bHasMotionMatchingResult)
					{
						UAnimationWarpingLibrary::GetCurveValueFromAnimation(Sequence, TEXT("SteeringTargetTime"), Task.CurrentAnimAssetTime, Task.SteeringCurve);
						// 네이티브 콜백의 캡처 객체를 지연 사용하지 않는다. 그래프가 보유한 활성 클립에서 지금 샘플링하고 값만 넘긴다.
						if (CVarSpikeSteering.GetValueOnAnyThread() != 0 && Task.Inputs.bValid)
						{
							for (int32 Channel = 0; Channel < 2; ++Channel)
							{
								if (!(Channel == 0 ? Task.Inputs.bEnableMovementSteering : Task.bTurnInPlace)) continue;
								const float PredictionTime = (Channel == 0 ? 10.0f : 0.2f) * Time.GetPlayRate();
								const double StartTime = FMath::Clamp(Task.CurrentAnimAssetTime, 0.0f, Sequence->GetPlayLength());
								Task.PredictedRotations[Channel] = Sequence->ExtractRootMotion(
									FAnimExtractContext(StartTime, true, FDeltaTimeRecord(PredictionTime), Time.IsLooping())).GetRotation();
							}
						}
					}
					break;
				}
			}
		}
		if (!Task.Inputs.bValid && !Instance->bReportedConfigurationError)
		{
			Context.Log(ELogVerbosity::Error, TEXT("Missing valid OffsetRoot shared input; root offset evaluation is disabled."), TEXT("LNPSphericalOffsetRoot"));
			Instance->bReportedConfigurationError = true;
		}
		return Task;
	}

	void FLNPSphericalOffsetRootTrait::PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
	{
		FInstanceData* Instance = Binding.GetInstanceData<FInstanceData>();
		FLNPPrepareSphericalOffsetRootTask Task = MakePreparationTask(Context, Binding);
		if (!Task.RootState)
		{
			Context.PopScopedInterface<ILNPSpikeRootScope>(Binding);
			IEvaluate::PostEvaluate(Context, Binding);
			return;
		}

		FLNPInspectSphericalOffsetRootTask Inspect;
		Inspect.RootState = Task.RootState;
		Inspect.Window = &Instance->RootDebugWindow;
		Inspect.BasedMovementDelta = Task.Inputs.BasedMovementDelta;
		Inspect.MovementBase = Task.Inputs.MovementBase;
		Inspect.bMoving = Task.Inputs.bEnableMovementSteering;
		Inspect.bOnMovingBase = Task.Inputs.bOnMovingBase;
		Inspect.bReset = Task.Inputs.bReset;
		FLNPFinalizeSphericalOffsetRootTask Finalize;
		Finalize.RootState = Task.RootState;
		Finalize.bValid = Task.Inputs.bValid;

		// Strafe 자식이 있으면 그 task 앞에서 준비·Steering을 한 번만 실행한다.
		const bool bEvaluateRoot = IsSpikeOffsetRootEnabled();
		if (bEvaluateRoot && !Instance->bPreparationScheduled)
			Context.AppendTask(Task);
		IEvaluate::PostEvaluate(Context, Binding);
		if (bEvaluateRoot)
			Context.AppendTask(Finalize);
		const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.Anim.DebugLocomotion"));
		if (bEvaluateRoot && Debug && Debug->GetInt() >= 2)
			Context.AppendTask(Inspect);
		else
			Instance->RootDebugWindow = {};
		Context.PopScopedInterface<ILNPSpikeRootScope>(Binding);
	}

	AUTO_REGISTER_ANIM_TRAIT(FLNPStrafeWarpingInputsTrait)
#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
	GeneratorMacro(IEvaluate)
	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FLNPStrafeWarpingInputsTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
#undef TRAIT_INTERFACE_ENUMERATOR

	void FLNPStrafeWarpingInputsTrait::PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
	{
		FInstanceData* Instance = Binding.GetInstanceData<FInstanceData>();
		FLNPPrepareStrafeWarpingTask Prepare;
		const FTraitStackBinding& Stack = *Binding.GetStack();
		for (uint32 Index = 0; Index < Binding.GetTraitIndex(); ++Index)
		{
			FTraitBinding Candidate;
			if (Stack.GetTrait(Index, Candidate) && Candidate.GetTrait()->GetTraitUID() == FStrafeWarpingTrait::TraitUID)
				Prepare.StrafeState = Candidate.GetInstanceData<FStrafeWarpingTrait::FInstanceData>();
		}
		TTraitBinding<ILNPSpikeRootScope> Root;
		if (Prepare.StrafeState && Context.GetScopedInterface(Root) && IsSpikeOffsetRootEnabled())
		{
			auto* RootInstance = Root.GetInstanceData<FLNPSphericalOffsetRootTrait::FInstanceData>();
			if (!RootInstance->bPreparationScheduled)
			{
				FLNPPrepareSphericalOffsetRootTask RootPrepare = FLNPSphericalOffsetRootTrait::MakePreparationTask(Context, Root);
				Prepare.RootState = RootPrepare.RootState;
				Prepare.Inputs = RootPrepare.Inputs;
				Context.AppendTask(RootPrepare);
				RootInstance->bPreparationScheduled = true;
			}
		}
		if ((!Prepare.StrafeState || !Root.IsValid()) && !Instance->bReportedConfigurationError)
		{
			Context.Log(ELogVerbosity::Error, TEXT("Place LNP Strafe Warping Inputs above native Strafe Warping on a child stack of LNP Spherical Offset Root."), TEXT("LNPStrafeWarpingInputs"));
			Instance->bReportedConfigurationError = true;
		}
		if (Prepare.StrafeState)
			Context.AppendTask(Prepare);
		IEvaluate::PostEvaluate(Context, Binding);
		const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.Anim.DebugLocomotion"));
		if (Prepare.StrafeState && Debug && Debug->GetInt() >= 2)
		{
			FLNPInspectStrafeWarpingTask Inspect;
			Inspect.StrafeState = Prepare.StrafeState;
			Inspect.Elapsed = &Instance->DebugElapsed;
			Context.AppendTask(Inspect);
		}
	}
}

void FLNPPrepareStrafeWarpingTask::Execute(UE::UAF::FEvaluationVM& VM) const
{
	using namespace UE::UAF;
	auto& Strafe = *static_cast<FStrafeWarpingTrait::FInstanceData*>(StrafeState);
	const auto* Root = static_cast<FOffsetRootBoneTrait::FInstanceData*>(RootState);
	const auto* Keyframe = VM.PeekValue<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0);
	const FVector Up = Inputs.MeshComponentTransform.GetUnitAxis(EAxis::Z);
	const FVector Direction = FVector::VectorPlaneProject(Inputs.LastNonZeroVelocity, Up).GetSafeNormal();
	const bool bEnabled = Root && Keyframe && Inputs.bValid && Inputs.bOnGround && !Inputs.bReset
		&& !Root->bIsFirstUpdate && Root->Alpha > 0.0f && !Direction.IsNearlyZero() && CVarSpikeStrafeWarping.GetValueOnAnyThread() != 0;
	Strafe.Alpha = bEnabled ? FMath::Clamp(Keyframe->Get()->Curves.Get(TEXT("Enable_Warping")), 0.0f, 1.0f) : 0.0f;
	if (Strafe.Alpha <= 0.0f)
	{
		Strafe.RootMotionDeltaDirection = FVector::ZeroVector;
		Strafe.RootMotionDeltaRotation = FQuat::Identity;
		Strafe.CounterCompensateTargetAngleRad = 0.0f;
		Strafe.OrientationAngleForPoseWarpRad = 0.0f;
		return;
	}
	Strafe.RootBoneTransform = FTransform(Root->SimulatedRotation, Root->SimulatedTranslation);
	Strafe.TargetOrientation = FRotationMatrix::MakeFromXZ(Direction, Up).ToQuat();
}

void FLNPInspectStrafeWarpingTask::Execute(UE::UAF::FEvaluationVM& VM) const
{
	using namespace UE::UAF;
	const auto& Strafe = *static_cast<FStrafeWarpingTrait::FInstanceData*>(StrafeState);
	*Elapsed += Strafe.DeltaTime;
	if (*Elapsed < 1.0f) return;
	*Elapsed = 0.0f;
	const auto* Keyframe = VM.PeekValue<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0);
	UE_LOG(LogLootNPop, Log, TEXT("[UAFStrafe] alpha=%.3f angle=%.3f curve=%.3f rootTilt=%.3f pose=%d"),
		Strafe.Alpha, FMath::RadiansToDegrees(Strafe.OrientationAngleForPoseWarpRad),
		Keyframe ? Keyframe->Get()->Curves.Get(TEXT("Enable_Warping")) : 0.0f,
		FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Strafe.RootBoneTransform.GetUnitAxis(EAxis::Z), Strafe.TargetOrientation.GetAxisZ()), -1.0, 1.0))),
		Keyframe != nullptr);
}

void FLNPPrepareSphericalOffsetRootTask::Execute(UE::UAF::FEvaluationVM& VM) const
{
	using namespace UE::UAF;
	FOffsetRootBoneTrait::FInstanceData& State = *static_cast<FOffsetRootBoneTrait::FInstanceData*>(RootState);
	const auto ResetSteering = [this]()
	{
		SteeringAngularVelocities[0] = SteeringAngularVelocities[1] = FVector::ZeroVector;
	};
	if (!Inputs.bValid)
	{
		ResetSteering();
		State.Alpha = 0.0f;
		State.bIsFirstUpdate = true;
		return;
	}
	if (State.DeltaTime <= 0.0f || State.Alpha == 0.0f)
	{
		ResetSteering();
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
	{
		ResetSteering();
		return;
	}

	const bool bNewInput = *LastAppliedSerial != Inputs.UpdateSerial;
	if (bNewInput && Inputs.bReset)
		State.bIsFirstUpdate = true;
	if (State.bIsFirstUpdate)
	{
		ResetSteering();
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

	const FQuat ComponentRotation = Inputs.MeshComponentTransform.GetRotation();
	const FQuat Rotation = ConstrainSphericalRootRotation(State.SimulatedRotation, ComponentRotation);
	State.SimulatedRotation = Rotation;

	// 보정된 UAF 누적 루트를 기준으로 루트모션을 먼저 바꾼다. 뒤의 기본 task가 한 번만 소비한다.
	const bool bReady = bHasMotionMatchingResult && CurrentAnimAssetTime >= 0.0f && !Inputs.Trajectory.Samples.IsEmpty();
	const auto ReportSteering = [&](bool bApplied)
	{
		const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.Anim.DebugLocomotion"));
		if (!Debug || Debug->GetInt() < 2) return;
		*SteeringDebugElapsed += State.DeltaTime;
		if (*SteeringDebugElapsed < 1.0f) return;
		*SteeringDebugElapsed = 0.0f;
		UE_LOG(LogLootNPop, Log, TEXT("[UAFSteering] ready=%d applied=%d tip=%d db=%s anim=%s time=%.3f curve=%.3f rootYaw=%.1f rootTilt=%.2f deltaYaw=%.2f"),
			bReady, bApplied, bTurnInPlace, *SelectedDatabase.ToString(), *SelectedAnimation.ToString(), CurrentAnimAssetTime, SteeringCurve,
			(ComponentRotation.Inverse() * Rotation).Rotator().Yaw,
			FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Rotation.GetAxisZ(), State.AnimatedGroundNormal), -1.0, 1.0))),
			RootMotion.GetRotation().Rotator().Yaw);
	};
	if (CVarSpikeSteering.GetValueOnAnyThread() == 0 || !bReady || Inputs.RotationMode == EOffsetRootBoneMode::Release)
	{
		ResetSteering();
		ReportSteering(false);
		return;
	}
	const float Curve = FMath::Clamp(SteeringCurve, 0.0f, 1.0f);
	FQuat Target = Inputs.Trajectory.GetSampleAtTime(FMath::Lerp(0.1f, 1.5f, Curve)).Facing;
	Target = (FQuat::FindBetweenNormals(Target.GetAxisZ(), State.AnimatedGroundNormal) * Target).GetNormalized();
	const float Speed = RootMotion.GetTranslation().Length() / State.DeltaTime;
	const bool Enabled[2] = { Inputs.bEnableMovementSteering && Speed > 10.0f, bTurnInPlace };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (!Enabled[Index])
		{
			SteeringAngularVelocities[Index] = FVector::ZeroVector;
			continue;
		}
		SteerRootMotion(RootMotion, Rotation, Target, PredictedRotations[Index], Index == 0 ? FMath::Lerp(0.1f, 0.3f, Curve) : 0.2f,
			State.DeltaTime, SteeringAngularVelocities[Index]);
	}
	if (Enabled[0] || Enabled[1])
		Provider->OverrideRootMotion(RootMotion, Keyframe->Get()->Attributes);
	ReportSteering(Enabled[0] || Enabled[1]);
}

void FLNPFinalizeSphericalOffsetRootTask::Execute(UE::UAF::FEvaluationVM& VM) const
{
	using namespace UE::UAF;
	auto& State = *static_cast<FOffsetRootBoneTrait::FInstanceData*>(RootState);
	auto* Keyframe = VM.PeekValueMutable<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0);
	const UE::Anim::IAnimRootMotionProvider* Provider = UE::Anim::IAnimRootMotionProvider::Get();
	FTransform RootMotion;
	if (!bValid || !Keyframe || State.bIsFirstUpdate || State.Alpha <= 0.0f || State.DeltaTime <= 0.0f
		|| !Provider || !Provider->ExtractRootMotion(Keyframe->Get()->Attributes, RootMotion))
		return;
	FTransform PoseRoot(Keyframe->Get()->Pose.LocalTransforms[UE::UAF::FReferencePose::RootBoneIndex]);
	FinalizeSphericalRootRotation(State.SimulatedRotation, PoseRoot, State.MeshComponentTransformWorld.GetRotation());
	Keyframe->Get()->Pose.LocalTransforms[UE::UAF::FReferencePose::RootBoneIndex] = PoseRoot;
}

void FLNPInspectSphericalOffsetRootTask::Execute(UE::UAF::FEvaluationVM& VM) const
{
	using namespace UE::UAF;
	const auto& State = *static_cast<FOffsetRootBoneTrait::FInstanceData*>(RootState);
	const auto* Keyframe = VM.PeekValue<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0);
	if (!Keyframe || State.bIsFirstUpdate || State.Alpha <= 0.0f || State.DeltaTime <= 0.0f)
	{
		*Window = {};
		return;
	}
	const FTransform& Component = State.MeshComponentTransformWorld;
	const FTransform PoseRoot = FTransform(Keyframe->Get()->Pose.LocalTransforms[UE::UAF::FReferencePose::RootBoneIndex]) * Component;
	const FVector Up = Component.GetUnitAxis(EAxis::Z);
	const auto Tilt = [&Up](const FQuat& Rotation)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Rotation.GetAxisZ(), Up), -1.0, 1.0)));
	};
	const auto AbsYaw = [&Component](const FQuat& Rotation)
	{
		return FMath::Abs((Component.GetRotation().Inverse() * Rotation).Rotator().Yaw);
	};
	Window->Elapsed += State.DeltaTime;
	++Window->Frames;
	Window->Moving += bMoving;
	Window->Ground += State.bOnGround;
	Window->Based += bOnMovingBase;
	Window->Resets += bReset;
	Window->StateCm = FMath::Max(Window->StateCm, FVector::Distance(State.SimulatedTranslation, Component.GetLocation()));
	Window->PoseCm = FMath::Max(Window->PoseCm, FVector::Distance(PoseRoot.GetLocation(), Component.GetLocation()));
	Window->StateYaw = FMath::Max(Window->StateYaw, AbsYaw(State.SimulatedRotation));
	Window->PoseYaw = FMath::Max(Window->PoseYaw, AbsYaw(PoseRoot.GetRotation()));
	Window->StateTilt = FMath::Max(Window->StateTilt, Tilt(State.SimulatedRotation));
	Window->PoseTilt = FMath::Max(Window->PoseTilt, Tilt(PoseRoot.GetRotation()));
	Window->BaseCm = FMath::Max(Window->BaseCm, BasedMovementDelta.GetTranslation().Length());
	Window->BaseDegrees = FMath::Max(Window->BaseDegrees, FMath::RadiansToDegrees(BasedMovementDelta.GetRotation().AngularDistance(FQuat::Identity)));
	if (Window->Elapsed < 1.0f) return;
	const FVector Location = Component.GetLocation();
	const double Latitude = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Location.GetSafeNormal().Z, -1.0, 1.0)));
	UE_LOG(LogLootNPop, Log, TEXT("[UAFRoot] frames=%d moving=%d ground=%d based=%d resets=%d lat=%.2f upZ=%.3f pos=(%.0f,%.0f,%.0f) base=%s maxStateCm=%.3f maxPoseCm=%.3f maxStateYaw=%.2f maxPoseYaw=%.2f maxStateTilt=%.3f maxPoseTilt=%.3f maxBaseCm=%.3f maxBaseDeg=%.4f"),
		Window->Frames, Window->Moving, Window->Ground, Window->Based, Window->Resets, Latitude, Up.Z, Location.X, Location.Y, Location.Z,
		*MovementBase.ToString(), Window->StateCm, Window->PoseCm, Window->StateYaw, Window->PoseYaw, Window->StateTilt, Window->PoseTilt, Window->BaseCm, Window->BaseDegrees);
	*Window = {};
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPUAFSphericalRootFinalizationTest,
	"LootNPop.Animation.UAFSpike.SphericalRootFinalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPUAFSphericalRootFinalizationTest::RunTest(const FString& Parameters)
{
	const FQuat Components[] = { FQuat::Identity, FRotator(90, 0, 0).Quaternion(), FRotator(180, 0, 0).Quaternion(), FRotator(40, 30, 20).Quaternion() };
	const FQuat Authored = FRotator(6, 17, -4).Quaternion();
	const FVector Location(12, -7, 3), Scale(1.1, 0.9, 1.2);
	for (const FQuat& Component : Components)
	{
		for (const double Sign : { -1.0, 1.0 })
		{
			// 소비 후 실제 관찰한 범위를 재현한다. 이전 순서에서는 이 상태가 그대로 출력됐다.
			FQuat State = (Component * FRotator(10.593, Sign * 92.4, 0).Quaternion()).GetNormalized();
			TestTrue(TEXT("Repro has tilt"), !State.GetAxisZ().Equals(Component.GetAxisZ(), 1e-4));
			FTransform Pose((Component.Inverse() * State * Authored).GetNormalized(), Location, Scale);
			FinalizeSphericalRootRotation(State, Pose, Component);
			const FQuat Expected = Component * FQuat(FVector::UpVector, Sign * UE_HALF_PI);
			TestTrue(TEXT("Final state is the local yaw boundary for every world Up"), State.AngularDistance(Expected) < 1e-5);
			TestTrue(TEXT("Final state Up matches component Up"), State.GetAxisZ().Equals(Component.GetAxisZ(), 1e-6));
			TestTrue(TEXT("Authored root rotation relative to state is preserved"),
				(State.Inverse() * Component * Pose.GetRotation()).AngularDistance(Authored) < 1e-5);
			TestTrue(TEXT("Root translation is preserved"), Pose.GetLocation().Equals(Location, 1e-9));
			TestTrue(TEXT("Root scale is preserved"), Pose.GetScale3D().Equals(Scale, 1e-9));
		}
		FQuat State = Component * FQuat(FVector::UpVector, FMath::DegreesToRadians(35.0));
		FTransform Pose((Component.Inverse() * State * Authored).GetNormalized(), Location, Scale);
		const FQuat Before = Pose.GetRotation();
		FinalizeSphericalRootRotation(State, Pose, Component);
		TestTrue(TEXT("Already constrained output is unchanged"), Pose.GetRotation().AngularDistance(Before) < 1e-5);
	}
	return true;
}

#endif
