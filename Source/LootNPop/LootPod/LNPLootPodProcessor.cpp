// Fill out your copyright notice in the Description page of Project Settings.

#include "LNPLootPodProcessor.h"
#include "LNPLootPodMassTypes.h"
#include "LNPLootPod.h"
#include "LootNPop.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassCommandBuffer.h"
#include "MassActorSubsystem.h"

// --- Integrated Command for notifying Actor of state transitions and handling logic ---
struct FLNPPodStateTransitionCommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<ALNPLootPod> Pod;
		int32 PodID;
		ELNPLootPodState OldState;
		ELNPLootPodState NewState;
		FVector Location;
	};

	FLNPPodStateTransitionCommand()
		: FMassBatchedCommand(EMassCommandOperationType::None)
	{}

	void Add(ALNPLootPod* InPod, int32 InPodID, ELNPLootPodState InOldState, ELNPLootPodState InNewState, const FVector& InLocation)
	{
		Entries.Add({ InPod, InPodID, InOldState, InNewState, InLocation });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		for (const FEntry& Entry : Entries)
		{
			ALNPLootPod* Pod = Entry.Pod.Get();
			
			// 1. Notify Visuals/Actor State
			if (Pod != nullptr)
			{
				Pod->UpdateVisuals(Entry.NewState);
			}

			if (Entry.OldState != Entry.NewState)
			{
				UE_LOG(LogLootNPop, Log, TEXT("[LootPod] PodID %d transitioned from %s to %s at location %s"), 
					Entry.PodID, 
					*UEnum::GetValueAsString(Entry.OldState), 
					*UEnum::GetValueAsString(Entry.NewState), 
					*Entry.Location.ToString());
			}

			// 2. Specialized Processing based on State Transition
			if (Entry.NewState == ELNPLootPodState::Popped)
			{
				// Integrated reward dropping logic
				// TODO: Spawn actual reward actors/items here
			}
			else if (Entry.NewState == ELNPLootPodState::Looting)
			{
			}
			else if (Entry.NewState == ELNPLootPodState::Idle && Entry.OldState == ELNPLootPodState::Looting)
			{
			}
		}
	}

	virtual void Reset() override
	{
		Entries.Reset();
		FMassBatchedCommand::Reset();
	}

	virtual SIZE_T GetAllocatedSize() const override { return Entries.GetAllocatedSize(); }

	virtual int32 GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};

// --- 1. ULNPIdleToLootingProcessor ---

ULNPIdleToLootingProcessor::ULNPIdleToLootingProcessor()
	: EntityQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
}

void ULNPIdleToLootingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FLNPLootPodIdleTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FLNPLootPodFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);

	// Query for potential looters
	PlayerQuery.AddTagRequirement<FLNPPlayerLootingTag>(EMassFragmentPresence::All);
	PlayerQuery.AddRequirement<FLNPPlayerLootingFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void ULNPIdleToLootingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Cache player locations
	struct FLooterInfo { FVector Location; };
	TArray<FLooterInfo> ActiveLooters;
	
	PlayerQuery.ForEachEntityChunk(Context, [&ActiveLooters](FMassExecutionContext& PlayerContext)
	{
		const int32 NumPlayers = PlayerContext.GetNumEntities();
		const TConstArrayView<FTransformFragment> Transforms = PlayerContext.GetFragmentView<FTransformFragment>();

		for (int32 i = 0; i < NumPlayers; ++i)
		{
			ActiveLooters.Add({ Transforms[i].GetTransform().GetLocation() });
		}
	});

	EntityQuery.ForEachEntityChunk(Context, [this, &ActiveLooters](FMassExecutionContext& IterContext)
	{
		const int32 NumEntities = IterContext.GetNumEntities();
		const TArrayView<FLNPLootPodFragment> LootPods = IterContext.GetMutableFragmentView<FLNPLootPodFragment>();
		const TConstArrayView<FTransformFragment> Transforms = IterContext.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassActorFragment> ActorFragments = IterContext.GetMutableFragmentView<FMassActorFragment>();

		for (FMassExecutionContext::FEntityIterator i = IterContext.CreateEntityIterator(); i; ++i)
		{
			const FVector PodLocation = Transforms[i].GetTransform().GetLocation();
			const float MaxDistSq = LootPods[i].LootableDistSquared;

			bool bPlayerDetected = false;
			for (const FLooterInfo& Looter : ActiveLooters)
			{
				if (FVector::DistSquared(PodLocation, Looter.Location) <= MaxDistSq)
				{
					bPlayerDetected = true;
					break;
				}
			}

			if (bPlayerDetected)
			{
				LootPods[i].State = ELNPLootPodState::Looting;

				// Notify State Transition
				ALNPLootPod* PodActor = Cast<ALNPLootPod>(ActorFragments[i].GetMutable());
				IterContext.Defer().PushCommand<FLNPPodStateTransitionCommand>(PodActor, LootPods[i].PodID, ELNPLootPodState::Idle, ELNPLootPodState::Looting, PodLocation);

				// Defer tag changes
				IterContext.Defer().RemoveTag<FLNPLootPodIdleTag>(IterContext.GetEntity(i));
				IterContext.Defer().AddTag<FLNPLootPodLootingTag>(IterContext.GetEntity(i));
			}
		}
	});
}

// --- 2. ULNPLootingProcessor ---

ULNPLootingProcessor::ULNPLootingProcessor()
	: EntityQuery(*this), PlayerQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
}

void ULNPLootingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FLNPLootPodLootingTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FLNPLootPodFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);

	// Query for looters
	PlayerQuery.AddTagRequirement<FLNPPlayerLootingTag>(EMassFragmentPresence::All);
	PlayerQuery.AddRequirement<FLNPPlayerLootingFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void ULNPLootingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	// 1. Cache player info (Do this once per execution)
	struct FLooterInfo { FVector Location; float BuffedLootSpeed; };
	TArray<FLooterInfo> ActiveLooters;
	PlayerQuery.ForEachEntityChunk(Context, [&ActiveLooters](FMassExecutionContext& PlayerContext)
	{
		const int32 NumPlayers = PlayerContext.GetNumEntities();
		const TConstArrayView<FLNPPlayerLootingFragment> LootingFragments = PlayerContext.GetFragmentView<FLNPPlayerLootingFragment>();
		const TConstArrayView<FTransformFragment> Transforms = PlayerContext.GetFragmentView<FTransformFragment>();
		for (int32 i = 0; i < NumPlayers; ++i)
		{
			// TODO: Fetch actual BuffedLootSpeed from player stats/buffs
			ActiveLooters.Add({ Transforms[i].GetTransform().GetLocation(), LootingFragments[i].BuffedLootSpeed });
		}
	});

	// 2. Process all looting pods
	EntityQuery.ForEachEntityChunk(Context, [DeltaTime, &ActiveLooters](FMassExecutionContext& IterContext)
	{
		const int32 NumEntities = IterContext.GetNumEntities();
		const TArrayView<FLNPLootPodFragment> LootPods = IterContext.GetMutableFragmentView<FLNPLootPodFragment>();
		const TConstArrayView<FTransformFragment> Transforms = IterContext.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassActorFragment> ActorFragments = IterContext.GetMutableFragmentView<FMassActorFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			ALNPLootPod* PodActor = Cast<ALNPLootPod>(ActorFragments[i].GetMutable());
			const FVector PodLocation = Transforms[i].GetTransform().GetLocation();
			const int32 PodID = LootPods[i].PodID;
			const float MaxDistSq = LootPods[i].LootableDistSquared;

			// Proximity Check: Collect all valid looters for this pod
			TArray<float> ValidLooterSpeeds;
			for (const auto& Looter : ActiveLooters)
			{
				if (FVector::DistSquared(PodLocation, Looter.Location) <= MaxDistSq)
				{
					ValidLooterSpeeds.Add(Looter.BuffedLootSpeed);
				}
			}

			// --- Unified Decision Logic ---
			
			// A. If there are valid looters: Update Gauge and check for Completion
			if (0 < ValidLooterSpeeds.Num())
			{
				// Update Gauge
				float FinalLootSpeed = 0.0f;
				for (float Speed : ValidLooterSpeeds)
				{
					FinalLootSpeed += Speed;
				}
				
				LootPods[i].CurrentGauge = FMath::Min(LootPods[i].MaxGauge, LootPods[i].CurrentGauge + (FinalLootSpeed * DeltaTime));

				// Check Completion
				if (LootPods[i].MaxGauge <= LootPods[i].CurrentGauge)
				{
					IterContext.Defer().PushCommand<FLNPPodStateTransitionCommand>(PodActor, PodID, ELNPLootPodState::Looting, ELNPLootPodState::Popped, PodLocation);
					
					// 즉시 파괴되는진 모르겠지만 DestroyEntity를 하는데 굳이 상태 갱신을 할 필요가 있을까?
					//LootPods[i].State = ELNPLootPodState::Popped;
					//IterContext.Defer().RemoveTag<FLNPLootPodLootingTag>(IterContext.GetEntity(i));

					IterContext.Defer().DestroyEntity(IterContext.GetEntity(i));
				}
			}
			// B. No valid looters in range: Interruption
			else
			{
				LootPods[i].State = ELNPLootPodState::Idle;
				IterContext.Defer().PushCommand<FLNPPodStateTransitionCommand>(PodActor, PodID, ELNPLootPodState::Looting, ELNPLootPodState::Idle, PodLocation);

				IterContext.Defer().RemoveTag<FLNPLootPodLootingTag>(IterContext.GetEntity(i));
				IterContext.Defer().AddTag<FLNPLootPodIdleTag>(IterContext.GetEntity(i));
			}
		}
	});
}
