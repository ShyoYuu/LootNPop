// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Animation/UAFSpike/LNPUAFSpikeRootInputs.h"
#include "EvaluationVM/EvaluationTask.h"
#include "TraitCore/Trait.h"
#include "TraitCore/TraitSharedData.h"
#include "TraitInterfaces/IEvaluate.h"
#include "LNPSphericalOffsetRootTrait.generated.h"

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
	struct FLNPSphericalOffsetRootTrait : FAdditiveTrait, IEvaluate
	{
		DECLARE_ANIM_TRAIT(FLNPSphericalOffsetRootTrait, FAdditiveTrait)
		using FSharedData = FLNPSphericalOffsetRootSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
			int64 LastAppliedSerial = 0;
			bool bReportedConfigurationError = false;
		};

		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;
	};
}

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
};
