// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFRifleOverlayTrait.h"

#include "HAL/IConsoleManager.h"
#include "LootNPop.h"
#include "Traits/BlendLayer.h"

namespace UE::UAF
{
	AUTO_REGISTER_ANIM_TRAIT(FLNPUAFRifleOverlayTrait)

#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
	GeneratorMacro(IUpdate)
	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FLNPUAFRifleOverlayTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
#undef TRAIT_INTERFACE_ENUMERATOR

	void FLNPUAFRifleOverlayTrait::PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding,
		const FTraitUpdateState& TraitState) const
	{
		FTraitBinding Base;
		if (Binding.GetStackBaseTrait(Base) && Base.GetTrait()->GetTraitUID() == FBlendLayerTrait::TraitUID)
		{
			auto* State = Base.GetInstanceData<FBlendLayerTrait::FInstanceData>();
			// native는 ChildBlend.Reset() 뒤에도 이 값을 유지한다. 새 자식의 OnBecomeRelevant를 다시 허용한다.
			if (!State->ChildBlend.IsValid() && State->bWasChildBlendRelevant)
			{
				State->bWasChildBlendRelevant = false;
				const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.Anim.DebugLocomotion"));
				if (Debug && Debug->GetInt() >= 2)
					UE_LOG(LogLootNPop, Log, TEXT("[UAFRifleRelevance] cleared stale relevance for a released child"));
			}
		}
		// 자식 생성/해제·가중치·순회·포즈 합성은 기존 native 경로가 계속 소유한다.
		IUpdate::PreUpdate(Context, Binding, TraitState);
	}
}
