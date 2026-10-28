// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFBoolBlendUpdateTrait.h"

#include "Animation/AnimTypes.h"
#include "HAL/IConsoleManager.h"
#include "LootNPop.h"
#include "Module/AnimNextModuleInstance.h"
#include "Traits/BlendByBool.h"

namespace UE::UAF
{
	AUTO_REGISTER_ANIM_TRAIT(FLNPUAFBoolBlendUpdateTrait)

#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
	GeneratorMacro(IUpdateTraversal)
	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FLNPUAFBoolBlendUpdateTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
#undef TRAIT_INTERFACE_ENUMERATOR

	void FLNPUAFBoolBlendUpdateTrait::QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TTraitBinding<IUpdateTraversal>& Binding,
		const FTraitUpdateState& TraitState, FUpdateTraversalQueue& TraversalQueue) const
	{
		FTraitBinding Base;
		if (!Binding.GetStackBaseTrait(Base) || Base.GetTrait()->GetTraitUID() != FBlendByBoolTrait::TraitUID)
			return;
		TTraitBinding<IDiscreteBlend> Blend;
		if (!Binding.GetStackInterface(Blend))
			return;
		const auto* State = Base.GetInstanceData<FBlendByBoolTrait::FInstanceData>();
		const auto* Shared = Base.GetSharedData<FBlendByBoolTrait::FSharedData>();
		const float TrueWeight = Blend.GetBlendWeight(Context, 0);
		for (int32 ChildIndex = 0; ChildIndex < 2; ++ChildIndex)
		{
			const FTraitPtr& Child = ChildIndex == 0 ? State->TrueChild : State->FalseChild;
			// False의 기본 갱신 조건도 1-True 가중치를 사용한다. 같은 판정으로 중복 등록을 피한다.
			const float Weight = ChildIndex == 0 ? TrueWeight : 1.0f - TrueWeight;
			if (!Child.IsValid() || FAnimWeight::IsRelevant(Weight) || (ChildIndex == 0 && Shared->bAlwaysUpdateTrueChild))
				continue;
			const bool bWasRelevant = ChildIndex == 0 ? State->bWasTrueChildRelevant : State->bWasFalseChildRelevant;
			TraversalQueue.Push(Child, TraitState.WithWeight(Weight)
				.AsBlendingOut(State->PreviousChildIndex != ChildIndex).AsNewlyRelevant(!bWasRelevant));
			// 각 트레이트를 따로 순회하므로 상위로 전달하지 않는다. 기본 Bool은 나머지 자식을 등록한다.
			const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("LNP.Anim.DebugLocomotion"));
			if (Debug && Debug->GetInt() >= 2)
			{
				const FAnimNextModuleInstance* Module = Context.GetRootGraphInstance().GetModuleInstance();
				UE_LOG(LogLootNPop, Log, TEXT("[UAFBoolUpdate] host=%s child=%d destination=%d weight=%.9g newlyRelevant=%d"),
					*GetPathNameSafe(Module ? Module->GetObject() : nullptr), ChildIndex, State->PreviousChildIndex, Weight, !bWasRelevant);
			}
		}
	}
}
