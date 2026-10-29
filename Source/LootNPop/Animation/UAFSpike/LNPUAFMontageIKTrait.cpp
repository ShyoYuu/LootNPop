// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Animation/UAFSpike/LNPUAFMontageIKTrait.h"

#include "Injection/MontageTrait.h"
#include "Injection/UAFMontageComponent.h"
#include "Misc/ScopeLock.h"
#include "Module/AnimNextModuleInstance.h"

void FLNPUAFHandIKFeedback::OnBeginExecution(float InDeltaTime)
{
	Pending = {};
	SeenSlots = 0;
	bDuplicateSlot = false;
}

void FLNPUAFHandIKFeedback::RecordSlot(int32 Index, float LocalWeight, float GlobalWeight)
{
	check(Index >= 0 && Index < 3);
	bDuplicateSlot |= (SeenSlots & (1 << Index)) != 0;
	SeenSlots |= 1 << Index;
	Pending.Weights[Index] = LocalWeight * GlobalWeight;
}

void FLNPUAFHandIKFeedback::OnEndExecution(float InDeltaTime)
{
	FScopeLock Lock(&Mutex);
	Pending.Serial = Completed.Serial + 1;
	Pending.bValid = SeenSlots == 7 && !bDuplicateSlot;
	Completed = Pending;
}

FLNPUAFHandIKSnapshot FLNPUAFHandIKFeedback::Read() const
{
	FScopeLock Lock(&Mutex);
	return Completed;
}

namespace UE::UAF
{
	AUTO_REGISTER_ANIM_TRAIT(FLNPUAFMontageIKTrait)

#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
	GeneratorMacro(IUpdate)
	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FLNPUAFMontageIKTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
#undef TRAIT_INTERFACE_ENUMERATOR

	void FLNPUAFMontageIKTrait::PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding,
		const FTraitUpdateState& TraitState) const
	{
		IUpdate::PreUpdate(Context, Binding, TraitState);
		FTraitBinding Base;
		const bool bMontageBase = Binding.GetStackBaseTrait(Base) && Base.GetTrait()->GetTraitUID() == FMontageTrait::TraitUID;
		const auto* Shared = bMontageBase ? Base.GetSharedData<FMontageTrait::FSharedData>() : nullptr;
		const auto* State = bMontageBase ? Base.GetInstanceData<FMontageTrait::FInstanceData>() : nullptr;
		const FName Slot = Shared ? Shared->GetSlotName(Base) : NAME_None;
		const int32 Index = Slot == TEXT("DefaultSlot") ? 0 : Slot == TEXT("FullBody") ? 1 : Slot == TEXT("UpperBody") ? 2 : INDEX_NONE;
		if (!State || !State->MontageComponent || Index == INDEX_NONE || !Shared->GetbAlwaysUpdateSource(Base))
		{
			FInstanceData* Instance = Binding.GetInstanceData<FInstanceData>();
			if (!Instance->bReportedConfigurationError)
			{
				Context.Log(ELogVerbosity::Error, TEXT("Requires a native DefaultSlot, FullBody or UpperBody Montage Trait with Always Update Source=true."), TEXT("LNPMontageIK"));
				Instance->bReportedConfigurationError = true;
			}
			return;
		}
		if (FAnimNextModuleInstance* Module = Context.GetRootGraphInstance().GetModuleInstance())
			Module->GetOrAddComponent<FLNPUAFHandIKFeedback>().RecordSlot(Index,
				State->MontageComponent->GetSlotLocalWeight(Slot), TraitState.GetTotalWeight());
	}
}
