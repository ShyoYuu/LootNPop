// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "TraitCore/Trait.h"
#include "TraitCore/TraitSharedData.h"
#include "TraitInterfaces/IUpdate.h"
#include "LNPUAFRifleOverlayTrait.generated.h"

/** native Blend Layer가 해제한 라이플 분기를 다시 만들 때 초기화를 누락하지 않게 한다. */
USTRUCT(meta = (DisplayName = "LNP Rifle Overlay Relevance"))
struct FLNPUAFRifleOverlaySharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()
};

namespace UE::UAF
{
	struct FLNPUAFRifleOverlayTrait : FAdditiveTrait, IUpdate
	{
		DECLARE_ANIM_TRAIT(FLNPUAFRifleOverlayTrait, FAdditiveTrait)
		using FSharedData = FLNPUAFRifleOverlaySharedData;

		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding,
			const FTraitUpdateState& TraitState) const override;
	};
}
