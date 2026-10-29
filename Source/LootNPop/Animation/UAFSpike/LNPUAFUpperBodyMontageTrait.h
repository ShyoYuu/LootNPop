// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "EvaluationVM/EvaluationTask.h"
#include "TraitCore/Trait.h"
#include "TraitCore/TraitSharedData.h"
#include "TraitInterfaces/IEvaluate.h"
#include "LNPUAFUpperBodyMontageTrait.generated.h"

/** 같은 스택의 native UpperBody 몽타주를 기존 spine_01 마스크로 합성한다. */
USTRUCT(meta = (DisplayName = "LNP UpperBody Montage Mask"))
struct FLNPUAFUpperBodyMontageSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()
};

namespace UE::UAF
{
	struct FLNPUAFUpperBodyMontageTrait : FAdditiveTrait, IEvaluate
	{
		DECLARE_ANIM_TRAIT(FLNPUAFUpperBodyMontageTrait, FAdditiveTrait)
		using FSharedData = FLNPUAFUpperBodyMontageSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
			bool bReportedConfigurationError = false;
			uint32 EvaluationCount = 0;
		};

		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;
	};
}

/** 원본/몽타주 두 keyframe을 하나로 줄인다. 마스크 배열은 이 task 실행 동안만 사용한다. */
USTRUCT()
struct FLNPUAFBlendUpperBodyTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()
	DECLARE_ANIM_EVALUATION_TASK(FLNPUAFBlendUpperBodyTask)
	virtual void Execute(UE::UAF::FEvaluationVM& VM) const override;

	bool bLogEvaluation = false;
};
