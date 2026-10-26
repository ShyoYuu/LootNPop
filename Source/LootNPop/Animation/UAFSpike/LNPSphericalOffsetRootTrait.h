// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Animation/UAFSpike/LNPUAFSpikeRootInputs.h"
#include "EvaluationVM/EvaluationTask.h"
#include "TraitCore/Trait.h"
#include "TraitCore/TraitSharedData.h"
#include "TraitCore/IScopedTraitInterface.h"
#include "TraitInterfaces/IEvaluate.h"
#include "LNPSphericalOffsetRootTrait.generated.h"

struct FLNPPrepareSphericalOffsetRootTask;

/** 약 1초 동안 실제 평가한 모든 프레임의 최대값. 기본 꺼짐인 로코모션 디버그에서만 수집한다. */
struct FLNPUAFRootDebugWindow
{
	float Elapsed = 0.0f;
	int32 Frames = 0, Moving = 0, Ground = 0, Based = 0, Resets = 0;
	double StateCm = 0, PoseCm = 0, StateYaw = 0, PoseYaw = 0, StateTilt = 0, PoseTilt = 0, BaseCm = 0, BaseDegrees = 0;
};

/** 기본 UAF Offset Root와 같은 스택에 추가하는 스파이크 전용 구면 보정. */
USTRUCT(meta = (DisplayName = "LNP Spherical Offset Root"))
struct FLNPSphericalOffsetRootSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PinShownByDefault))
	FLNPUAFSpikeRootInputs RootInputs;

#define TRAIT_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
	GeneratorMacro(RootInputs)
	GENERATE_TRAIT_LATENT_PROPERTIES(FLNPSphericalOffsetRootSharedData, TRAIT_LATENT_PROPERTIES_ENUMERATOR)
#undef TRAIT_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::UAF
{
	/** 자식 Strafe Warping이 같은 UAF 루트의 준비 task를 먼저 예약하기 위한 평가 범위. */
	struct ILNPSpikeRootScope : IScopedTraitInterface
	{
		DECLARE_ANIM_TRAIT_INTERFACE(ILNPSpikeRootScope)
	};

	template<>
	struct TTraitBinding<ILNPSpikeRootScope> : FTraitBinding {};

	struct FLNPSphericalOffsetRootTrait : FAdditiveTrait, IEvaluate, ILNPSpikeRootScope
	{
		DECLARE_ANIM_TRAIT(FLNPSphericalOffsetRootTrait, FAdditiveTrait)
		using FSharedData = FLNPSphericalOffsetRootSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
			int64 LastAppliedSerial = 0;
			bool bReportedConfigurationError = false;
			FVector SteeringAngularVelocities[2] = { FVector::ZeroVector, FVector::ZeroVector };
			float SteeringDebugElapsed = 0.0f;
			FLNPUAFRootDebugWindow RootDebugWindow;
			bool bPreparationScheduled = false;
		};

		virtual void PreEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;
		static FLNPPrepareSphericalOffsetRootTask MakePreparationTask(FEvaluateTraversalContext& Context, const FTraitBinding& Binding);
	};
}

/** 기본 Strafe Warping의 수학은 유지하고 현재 평가 포즈·UAF 루트 입력만 공급한다. */
USTRUCT(meta = (DisplayName = "LNP Strafe Warping Inputs"))
struct FLNPStrafeWarpingInputsSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()
};

namespace UE::UAF
{
	struct FLNPStrafeWarpingInputsTrait : FAdditiveTrait, IEvaluate
	{
		DECLARE_ANIM_TRAIT(FLNPStrafeWarpingInputsTrait, FAdditiveTrait)
		using FSharedData = FLNPStrafeWarpingInputsSharedData;
		struct FInstanceData : FTrait::FInstanceData
		{
			bool bReportedConfigurationError = false;
			float DebugElapsed = 0.0f;
		};
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;
	};
}

/** 루트 준비와 기본 Strafe Warping 사이에서 실제 평가 커브·이동 방향을 전달한다. */
USTRUCT()
struct FLNPPrepareStrafeWarpingTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()
	DECLARE_ANIM_EVALUATION_TASK(FLNPPrepareStrafeWarpingTask)
	virtual void Execute(UE::UAF::FEvaluationVM& VM) const override;

	UE::UAF::FTraitInstanceData* StrafeState = nullptr;
	UE::UAF::FTraitInstanceData* RootState = nullptr;
	FLNPUAFSpikeRootInputs Inputs;
};

/** 기본 task가 계산한 실제 워핑 각도를 낮은 빈도로 확인한다. */
USTRUCT()
struct FLNPInspectStrafeWarpingTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()
	DECLARE_ANIM_EVALUATION_TASK(FLNPInspectStrafeWarpingTask)
	virtual void Execute(UE::UAF::FEvaluationVM& VM) const override;

	UE::UAF::FTraitInstanceData* StrafeState = nullptr;
	float* Elapsed = nullptr;
};

/** 기본 Offset Root task 바로 앞에서 같은 인스턴스의 누적 상태를 보정한다. */
USTRUCT()
struct FLNPPrepareSphericalOffsetRootTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()
	DECLARE_ANIM_EVALUATION_TASK(FLNPPrepareSphericalOffsetRootTask)

	virtual void Execute(UE::UAF::FEvaluationVM& VM) const override;

	UE::UAF::FTraitInstanceData* RootState = nullptr;
	int64* LastAppliedSerial = nullptr;
	FLNPUAFSpikeRootInputs Inputs;
	FVector* SteeringAngularVelocities = nullptr;
	FQuat PredictedRotations[2] = { FQuat::Identity, FQuat::Identity };
	float CurrentAnimAssetTime = -1.0f;
	float SteeringCurve = 0.0f;
	bool bTurnInPlace = false;
	bool bHasMotionMatchingResult = false;
	float* SteeringDebugElapsed = nullptr;
	FName SelectedDatabase;
	FName SelectedAnimation;
};

/** 기본 task가 루트모션을 소비한 뒤 누적 회전과 출력 루트에 같은 구면 회전 제약을 적용한다. */
USTRUCT()
struct FLNPFinalizeSphericalOffsetRootTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()
	DECLARE_ANIM_EVALUATION_TASK(FLNPFinalizeSphericalOffsetRootTask)
	virtual void Execute(UE::UAF::FEvaluationVM& VM) const override;

	UE::UAF::FTraitInstanceData* RootState = nullptr;
	bool bValid = false;
};

/** 기본 Offset Root 평가 직후 누적 상태와 실제 출력 포즈를 읽는다. 포즈를 바꾸지 않는다. */
USTRUCT()
struct FLNPInspectSphericalOffsetRootTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()
	DECLARE_ANIM_EVALUATION_TASK(FLNPInspectSphericalOffsetRootTask)
	virtual void Execute(UE::UAF::FEvaluationVM& VM) const override;

	UE::UAF::FTraitInstanceData* RootState = nullptr;
	FLNPUAFRootDebugWindow* Window = nullptr;
	FTransform BasedMovementDelta;
	FName MovementBase;
	bool bMoving = false, bOnMovingBase = false, bReset = false;
};
