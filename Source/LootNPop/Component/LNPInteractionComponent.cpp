// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/LNPInteractionComponent.h"
#include "GameLogic/LNPLootPod.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectComponent.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectTypes.h"
#include "MassAgentComponent.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassCommands.h"
#include "GameLogic/LootPodMassTypes.h"
#include "Engine/World.h"

ULNPInteractionComponent::ULNPInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULNPInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateInteractionCandidate();
}

void ULNPInteractionComponent::UpdateInteractionCandidate()
{
	APawn* Owner = Cast<APawn>(GetOwner());
	if (Owner == nullptr)
		return;

	USmartObjectSubsystem* SOSubsystem = UWorld::GetSubsystem<USmartObjectSubsystem>(GetWorld());
	if (SOSubsystem == nullptr)
		return;

	// Reset candidates list each tick to avoid accumulation
	InteractionCandidates.Empty();

	// Use Filter and Search Request
	FSmartObjectRequestFilter Filter;
	FBox QueryBounds = FBox::BuildAABB(Owner->GetActorLocation(), FVector(InteractionRadius));
	FSmartObjectRequest Request(QueryBounds, Filter);

	TArray<FSmartObjectRequestResult> OutResults;
	SOSubsystem->FindSmartObjects(Request, OutResults);

	for (const FSmartObjectRequestResult& Result : OutResults)
	{
		const USmartObjectComponent* SOComp = SOSubsystem->GetSmartObjectContainer().GetSmartObjectComponent(Result.SmartObjectHandle);
		if (SOComp == nullptr)
			continue;

		ALNPLootPod* Pod = Cast<ALNPLootPod>(SOComp->GetOwner());

		// Basic interaction check
		if (Pod != nullptr && Pod->CanInteract(Owner))
		{
			InteractionCandidates.Add(Pod);
		}
	}
}

void ULNPInteractionComponent::PerformInteraction()
{
	APawn* Owner = Cast<APawn>(GetOwner());
	if (Owner == nullptr)
		return;

	UMassAgentComponent* MassAgentComponent = Owner->FindComponentByClass<UMassAgentComponent>();
	if (MassAgentComponent == nullptr)
		return;

	// Iterate over a copy to handle removals safely if needed
	TArray<TWeakObjectPtr<AActor>> CurrentCandidates = InteractionCandidates.Array();

	for (const TWeakObjectPtr<AActor>& Candidate : CurrentCandidates)
	{
		if (!Candidate.IsValid())
		{
			InteractionCandidates.Remove(Candidate);
			continue;
		}
		
		AActor* Actor = Candidate.Get();

		// ALNPLootPod-specific interaction logic
		if (ALNPLootPod* Pod = Cast<ALNPLootPod>(Actor))
		{
			// Corrected condition: proceed if CanInteract is true
			if (!Pod->CanInteract(Owner))
				continue;

			FMassEntityHandle PlayerEntity = MassAgentComponent->GetEntityHandle();
			if (!PlayerEntity.IsValid())
				continue;

			FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*GetWorld());

			// Check if the player is already looting
			if (EntityManager.GetFragmentDataPtr<FLNPPlayerLootingFragment>(PlayerEntity) == nullptr)
			{
				UE_LOG(LogTemp, Log, TEXT("Interacting with LootPod: %s"), *Pod->GetName());

				// 1. Signal Player is looting
				EntityManager.Defer().AddTag<FLNPPlayerLootingTag>(PlayerEntity);

				// 2. Set targeting info
				FLNPPlayerLootingFragment FragmentPayload;
				FragmentPayload.BuffedLootSpeed = 1.0f;
				EntityManager.Defer().PushCommand<FMassCommandAddFragmentInstances<FLNPPlayerLootingFragment>>(PlayerEntity, FragmentPayload);

				// 3. Trigger Pod logic
				Pod->StartLooting();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Player is already looting another pod."));
			}
		}
	}
}

TArray<AActor*> ULNPInteractionComponent::GetInteractionCandidates() const
{
	TArray<AActor*> OutArray;
	for (const TWeakObjectPtr<AActor>& WeakPtr : InteractionCandidates)
	{
		if (AActor* Actor = WeakPtr.Get())
		{
			OutArray.Add(Actor);
		}
	}
	return OutArray;
}

AActor* ULNPInteractionComponent::GetFirstInteractionCandidate() const
{
	for (const TWeakObjectPtr<AActor>& WeakPtr : InteractionCandidates)
	{
		if (AActor* Actor = WeakPtr.Get())
		{
			return Actor;
		}
	}
	return nullptr;
}

