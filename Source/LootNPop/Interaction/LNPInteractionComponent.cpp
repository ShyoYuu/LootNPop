// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/LNPInteractionComponent.h"
#include "LootPod/LNPLootPod.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LootNPop.h"

#include "SmartObjectSubsystem.h"
#include "SmartObjectComponent.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectTypes.h"
#include "MassAgentComponent.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassCommands.h"
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

	// 누적 방지를 위해 매 Tick 후보 목록 리셋
	InteractionCandidates.Empty();

	// 필터 및 검색 요청 사용
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

		// 기본 상호작용 체크
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

	// 필요 시 안전하게 제거할 수 있도록 복사본을 순회
	TArray<TWeakObjectPtr<AActor>> CurrentCandidates = InteractionCandidates.Array();

	for (const TWeakObjectPtr<AActor>& Candidate : CurrentCandidates)
	{
		if (!Candidate.IsValid())
		{
			InteractionCandidates.Remove(Candidate);
			continue;
		}
		
		AActor* Actor = Candidate.Get();

		// ALNPLootPod 전용 상호작용 로직
		if (ALNPLootPod* Pod = Cast<ALNPLootPod>(Actor))
		{
			// CanInteract가 true인 경우 진행
			if (!Pod->CanInteract(Owner))
				continue;

			FMassEntityHandle PlayerEntity = MassAgentComponent->GetEntityHandle();
			if (!PlayerEntity.IsValid())
				continue;

			FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*GetWorld());

			// Player가 이미 루팅 중인지 확인
			if (EntityManager.GetFragmentDataPtr<FLNPPlayerLootingFragment>(PlayerEntity) == nullptr)
			{
				UE_LOG(LogLootNPop, Log, TEXT("Interacting with LootPod: %s"), *Pod->GetName());

				// 1. Player 루팅 신호
				EntityManager.Defer().AddTag<FLNPPlayerLootingTag>(PlayerEntity);

				// 2. 타게팅 정보 설정
				FLNPPlayerLootingFragment FragmentPayload;
				FragmentPayload.BuffedLootSpeed = 1.0f;
				EntityManager.Defer().PushCommand<FMassCommandAddFragmentInstances<FLNPPlayerLootingFragment>>(PlayerEntity, FragmentPayload);

				// 3. Pod 로직 트리거
				Pod->StartLooting();
			}
			else
			{
				UE_LOG(LogLootNPop, Warning, TEXT("Player is already looting another pod."));
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

