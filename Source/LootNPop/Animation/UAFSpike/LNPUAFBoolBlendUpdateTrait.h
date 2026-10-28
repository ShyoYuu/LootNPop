// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "TraitCore/Trait.h"
#include "TraitCore/TraitSharedData.h"
#include "TraitInterfaces/IUpdate.h"
#include "LNPUAFBoolBlendUpdateTrait.generated.h"

/** Bool 블렌드의 평가에 남은 자식이 작은 가중치 때문에 갱신에서만 빠지지 않게 한다. */
USTRUCT(meta = (DisplayName = "LNP Bool Blend Update"))
struct FLNPUAFBoolBlendUpdateSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()
};

namespace UE::UAF
{
	struct FLNPUAFBoolBlendUpdateTrait : FAdditiveTrait, IUpdateTraversal
	{
		DECLARE_ANIM_TRAIT(FLNPUAFBoolBlendUpdateTrait, FAdditiveTrait)
		using FSharedData = FLNPUAFBoolBlendUpdateSharedData;

		virtual void QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TTraitBinding<IUpdateTraversal>& Binding,
			const FTraitUpdateState& TraitState, FUpdateTraversalQueue& TraversalQueue) const override;
	};
}
