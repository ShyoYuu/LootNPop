// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Interaction/LNPInteractionComponent.h"
#include "LootPod/LNPLootPod.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LootPod/LNPLootPodSubsystem.h"
#include "LootNPop.h"

#include "MassAgentComponent.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassCommands.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "Engine/World.h"

ULNPInteractionComponent::ULNPInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULNPInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateInteractionCandidate();
	UpdateInteractionPrompt();
}

void ULNPInteractionComponent::UpdateInteractionCandidate()
{
	APawn* Owner = Cast<APawn>(GetOwner());
	if (Owner == nullptr)
		return;

	// 누적 방지를 위해 매 Tick 후보 목록 리셋
	InteractionCandidates.Empty();

	const ULNPLootPodSubsystem* PodSubsystem = UWorld::GetSubsystem<ULNPLootPodSubsystem>(GetWorld());
	if (PodSubsystem == nullptr)
		return;

	// 살아 있는 Pod Actor 레지스트리만 순회 (→ ULNPLootPodSubsystem 주석: SmartObject 쿼리 폐기 사유)
	int32 NumPods = 0;
	ALNPLootPod* NearestPod = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	const float RadiusSq = FMath::Square(InteractionRadius);
	const FVector OwnerLocation = Owner->GetActorLocation();

	for (const TWeakObjectPtr<ALNPLootPod>& WeakPod : PodSubsystem->GetActivePods())
	{
		ALNPLootPod* Pod = WeakPod.Get();

		// 파괴됐거나 풀에 반납되어 숨겨진 액터는 제외
		if (Pod == nullptr || Pod->IsHidden())
			continue;

		const float DistSq = FVector::DistSquared(OwnerLocation, Pod->GetActorLocation());
		if (DistSq > RadiusSq)
			continue;

		++NumPods;
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			NearestPod = Pod;
		}

		// 기본 상호작용 체크
		if (Pod->CanInteract(Owner))
		{
			InteractionCandidates.Add(Pod);
		}
	}

	NearestNearbyPod = NearestPod;

	// LootPod 개발용 테스트 로그 — 주변 Pod 수·상호작용 가능 수가 변할 때만, 로컬 플레이어에서만 출력
	if (Owner->IsPlayerControlled() && Owner->IsLocallyControlled()
		&& (NumPods != LastLoggedPodCount || InteractionCandidates.Num() != LastLoggedInteractableCount))
	{
		LastLoggedPodCount = NumPods;
		LastLoggedInteractableCount = InteractionCandidates.Num();
		UE_LOG(LogLootNPop, Log, TEXT("[Interaction] Nearby LootPods=%d, interactable=%d (pod registry)"),
			NumPods, InteractionCandidates.Num());

		// Pod는 있는데 상호작용 불가면 사유 진단 (거리/각도/상태 중 무엇이 걸렸는지)
		if (NumPods > 0 && InteractionCandidates.Num() == 0 && NearestPod != nullptr)
		{
			UE_LOG(LogLootNPop, Log, TEXT("[Interaction] CanInteract=false — %s"), *NearestPod->GetInteractDiagnosticString(Owner));
		}
	}
}

void ULNPInteractionComponent::UpdateInteractionPrompt()
{
	APawn* Owner = Cast<APawn>(GetOwner());

	// 프롬프트는 로컬 플레이어 화면 전용 — 서버의 원격 폰·시뮬레이티드 프록시에서는 처리하지 않는다
	if (Owner == nullptr || !Owner->IsPlayerControlled() || !Owner->IsLocallyControlled())
		return;

	// 후보(거리+각도 통과) 중 가장 가까운 Idle Pod를 프롬프트 타겟으로 선정.
	// Looting 중인 Pod는 프레즌스 기반 기여라 입력이 불필요하므로 프롬프트를 띄우지 않는다.
	ALNPLootPod* NewTarget = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector OwnerLocation = Owner->GetActorLocation();

	for (const TWeakObjectPtr<AActor>& Candidate : InteractionCandidates)
	{
		ALNPLootPod* Pod = Cast<ALNPLootPod>(Candidate.Get());
		if (Pod == nullptr || Pod->GetCurrentState() != ELNPLootPodState::Idle)
			continue;

		const float DistSq = FVector::DistSquared(OwnerLocation, Pod->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			NewTarget = Pod;
		}
	}

	if (CurrentPromptTarget.Get() == NewTarget)
		return;

	if (ALNPLootPod* OldTarget = CurrentPromptTarget.Get())
		OldTarget->SetInteractionPromptVisible(false);

	if (NewTarget != nullptr)
	{
		NewTarget->SetInteractionPromptVisible(true);
		UE_LOG(LogLootNPop, Log, TEXT("[Interaction] Prompt ON — %s (dist %.0f)"), *NewTarget->GetName(), FMath::Sqrt(BestDistSq));
	}
	else
	{
		// 프롬프트가 꺼진 사유 진단 — 주변 Pod가 있으면 판정 값(거리/각도/상태)을 함께 출력
		if (const ALNPLootPod* Nearest = NearestNearbyPod.Get())
		{
			UE_LOG(LogLootNPop, Log, TEXT("[Interaction] Prompt OFF — nearest: %s"), *Nearest->GetInteractDiagnosticString(Owner));
		}
		else
		{
			UE_LOG(LogLootNPop, Log, TEXT("[Interaction] Prompt OFF — no LootPod nearby"));
		}
	}

	CurrentPromptTarget = NewTarget;
}

void ULNPInteractionComponent::PerformInteraction()
{
	APawn* Owner = Cast<APawn>(GetOwner());
	if (Owner == nullptr)
		return;

	// 필요 시 안전하게 제거할 수 있도록 복사본을 순회
	TArray<TWeakObjectPtr<AActor>> CurrentCandidates = InteractionCandidates.Array();

	// 상호작용 입력 테스트 로그 — 입력이 들어왔는지, 그 시점의 후보 수가 몇이었는지
	UE_LOG(LogLootNPop, Log, TEXT("[Interaction] Interact input — candidates=%d"), CurrentCandidates.Num());

	// 후보가 없으면 최근접 Pod의 판정 값을 입력 시점 기준으로 즉석 출력 (변화 감지 로그의 샘플링 맹점 보완)
	if (CurrentCandidates.Num() == 0)
	{
		if (const ALNPLootPod* Nearest = NearestNearbyPod.Get())
		{
			UE_LOG(LogLootNPop, Log, TEXT("[Interaction] input rejected — nearest: %s"), *Nearest->GetInteractDiagnosticString(Owner));
		}
	}

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

			// 로컬 비주얼 즉시 반응 (예측) — 서버 확정 상태는 CurrentState 복제(OnRep_PodState)가 덮는다
			Pod->StartLooting();

			// 루팅 태그/프래그먼트는 서버 월드의 플레이어 엔티티에 붙어야 한다 (Phase 7)
			if (Owner->HasAuthority())
				StartLootingOnServer(Pod);
			else
				Server_StartLooting(Pod);
		}
	}
}

void ULNPInteractionComponent::Server_StartLooting_Implementation(ALNPLootPod* Pod)
{
	// 서버 재검증 — 클라이언트 판정 시점과의 레이스(Popped 직후 등)를 걸러낸다
	APawn* Owner = Cast<APawn>(GetOwner());
	if (Owner == nullptr || Pod == nullptr || !Pod->CanInteract(Owner))
		return;

	StartLootingOnServer(Pod);
}

void ULNPInteractionComponent::StartLootingOnServer(ALNPLootPod* Pod)
{
	APawn* Owner = Cast<APawn>(GetOwner());
	if (Owner == nullptr || Pod == nullptr)
		return;

	UMassAgentComponent* MassAgentComponent = Owner->FindComponentByClass<UMassAgentComponent>();
	if (MassAgentComponent == nullptr)
		return;

	FMassEntityHandle PlayerEntity = MassAgentComponent->GetEntityHandle();
	if (!PlayerEntity.IsValid())
		return;

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*GetWorld());

	UE_LOG(LogLootNPop, Log, TEXT("Interacting with LootPod: %s"), *Pod->GetName());

	// 1. 루팅 존 활성화 요청 — 1회성 Tag, ULNPIdleToLootingProcessor가 처리 후 소비한다
	EntityManager.Defer().AddTag<FLNPPlayerLootingTag>(PlayerEntity);

	// 2. 루팅 속도 Fragment는 최초 1회만 부착 (플레이어 상주 데이터) — 이후 갱신은 LootSpeed Attribute
	//    변경 델리게이트(ALNPPlayerCharacter::PushLootSpeedToEntity)가 담당한다
	if (EntityManager.GetFragmentDataPtr<FLNPPlayerLootingFragment>(PlayerEntity) == nullptr)
	{
		float LootSpeed = 1.0f;
		if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			LootSpeed = ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetLootSpeedAttribute());
		}

		FLNPPlayerLootingFragment FragmentPayload;
		FragmentPayload.BuffedLootSpeed = LootSpeed;
		EntityManager.Defer().PushCommand<FMassCommandAddFragmentInstances<FLNPPlayerLootingFragment>>(PlayerEntity, FragmentPayload);
	}

	// 3. Pod 로직 트리거 (서버/리슨호스트 비주얼 + CurrentState 복제 마킹은 프로세서 전환이 담당)
	Pod->StartLooting();
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

