// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Interaction/LNPInteractionComponent.h"
#include "Interaction/LNPInteractable.h"
#include "Interaction/LNPInteractableRegistrySubsystem.h"
#include "LootPod/LNPLootPod.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LootDice/LNPLootDice.h"
#include "WorldDevice/LNPSpringLauncher.h"
#include "WorldDevice/LNPGrappleAnchor.h"
#include "Character/LNPPlayerCharacter.h"
#include "Character/LNPInputHandlerComponent.h"
#include "Item/LNPInventoryComponent.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPBuffData.h"
#include "Player/LNPPlayerState.h"
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

	const ULNPInteractableRegistrySubsystem* Registry = UWorld::GetSubsystem<ULNPInteractableRegistrySubsystem>(GetWorld());
	if (Registry == nullptr)
		return;

	// 살아 있는 인터랙터블 레지스트리만 순회 (→ ULNPInteractableRegistrySubsystem 주석: SmartObject 쿼리 폐기 사유)
	int32 NumPods = 0;
	ALNPLootPod* NearestPod = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	const FVector OwnerLocation = Owner->GetActorLocation();

	for (const TWeakObjectPtr<AActor>& WeakActor : Registry->GetInteractables())
	{
		AActor* Actor = WeakActor.Get();

		// 파괴됐거나 풀에 반납되어 숨겨진 액터는 제외
		if (Actor == nullptr || Actor->IsHidden())
			continue;

		const ILNPInteractable* Interactable = Cast<ILNPInteractable>(Actor);
		if (Interactable == nullptr)
			continue;

		// 광역 컷은 타입마다 다르다 — 초근접 Pod(250cm)과 원거리 그래플 앵커(수십 m)가 같은 루프를 돈다.
		const float DistSq = FVector::DistSquared(OwnerLocation, Actor->GetActorLocation());
		if (DistSq > FMath::Square(Interactable->GetInteractionSearchRadius()))
			continue;

		// Pod 전용 진단 추적 — 아래 실패 사유 로그가 Pod 기준으로만 쓰인다
		if (ALNPLootPod* Pod = Cast<ALNPLootPod>(Actor))
		{
			++NumPods;
			if (DistSq < NearestDistSq)
			{
				NearestDistSq = DistSq;
				NearestPod = Pod;
			}
		}

		if (ILNPInteractable::Execute_CanInteract(Actor, Owner))
		{
			InteractionCandidates.Add(Actor);
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

	// 후보 중 "입력이 필요한" 최선 타겟 선정 — 우선순위 내림차순, 동률이면 최근접.
	// 우선순위는 근접 대상(Pod·Dice)이 원거리 대상(그래플 앵커)보다 항상 이기게 하는 장치다.
	// 최근접만으로 뽑으면 Pod 앞에 서 있어도 조준선에 걸린 먼 앵커가 프롬프트를 가져갈 수 있다.
	AActor* NewTarget = nullptr;
	int32 BestPriority = TNumericLimits<int32>::Lowest();
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector OwnerLocation = Owner->GetActorLocation();

	for (const TWeakObjectPtr<AActor>& Candidate : InteractionCandidates)
	{
		AActor* Actor = Candidate.Get();
		if (Actor == nullptr)
			continue;

		// 조준·상태 판정 — 루팅 중인 Pod(입력 불필요), 등 뒤의 Dice 등을 걸러낸다. 로컬 전용이다.
		const ILNPInteractable* Interactable = Cast<ILNPInteractable>(Actor);
		if (Interactable == nullptr || !Interactable->WantsInteractionPrompt(Owner))
			continue;

		const int32 Priority = Interactable->GetInteractionPriority();
		const float DistSq = FVector::DistSquared(OwnerLocation, Actor->GetActorLocation());
		if (Priority > BestPriority || (Priority == BestPriority && DistSq < BestDistSq))
		{
			BestPriority = Priority;
			BestDistSq = DistSq;
			NewTarget = Actor;
		}
	}

	if (CurrentPromptTarget.Get() == NewTarget)
		return;

	auto SetPromptVisible = [](AActor* Target, bool bVisible)
	{
		if (ILNPInteractable* Interactable = Cast<ILNPInteractable>(Target))
			Interactable->SetInteractionPromptVisible(bVisible);
	};

	if (AActor* OldTarget = CurrentPromptTarget.Get())
		SetPromptVisible(OldTarget, false);

	if (NewTarget != nullptr)
	{
		SetPromptVisible(NewTarget, true);
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

	// F는 프롬프트가 떠 있는 "단일" 대상과만 상호작용한다 — 주변의 모든 Dice가 한 번에 습득되던
	// 문제를 막는다. CurrentPromptTarget은 UpdateInteractionPrompt가 매 틱 갱신하는 최근접 유효
	// 대상(Idle Pod 또는 Dice, 로컬 플레이어 전용)이며, 프롬프트가 없으면 상호작용도 없다.
	AActor* Target = CurrentPromptTarget.Get();
	if (Target == nullptr)
	{
		UE_LOG(LogLootNPop, Log, TEXT("[Interaction] Interact input — no prompt target"));
		return;
	}

	// ALNPLootPod — 루팅 시작
	if (ALNPLootPod* Pod = Cast<ALNPLootPod>(Target))
	{
		if (!ILNPInteractable::Execute_CanInteract(Pod, Owner))
			return;

		// 로컬 비주얼 즉시 반응 (예측) — 서버 확정 상태는 CurrentState 복제(OnRep_PodState)가 덮는다
		Pod->StartLooting();

		// 루팅 태그/프래그먼트는 서버 월드의 플레이어 엔티티에 붙어야 한다 (Phase 7)
		if (Owner->HasAuthority())
			StartLootingOnServer(Pod);
		else
			Server_StartLooting(Pod);
	}
	// ALNPLootDice — 획득 (인벤토리 편입은 서버 권위, 선착순 판정 포함)
	else if (ALNPLootDice* Dice = Cast<ALNPLootDice>(Target))
	{
		if (!ILNPInteractable::Execute_CanInteract(Dice, Owner))
			return;

		if (Owner->HasAuthority())
			PickupDiceOnServer(Dice);
		else
			Server_PickupDice(Dice);
	}
	// ALNPSpringLauncher — 정렬 후 사출. 여기만 컴포넌트의 Server RPC를 쓰지 않는다:
	// 발동 요청(GAS)이 곧 RPC이고, 어느 런처인지가 그 요청에 원자적으로 실려 간다.
	else if (ALNPSpringLauncher* Launcher = Cast<ALNPSpringLauncher>(Target))
	{
		if (ALNPPlayerCharacter* PlayerCharacter = Cast<ALNPPlayerCharacter>(Owner))
			PlayerCharacter->TryActivateSpringLaunch(Launcher);
	}
	// ALNPGrappleAnchor — RPC를 아예 쓰지 않는다. 고속 이동 그 자체라 대시와 같은 예측 경로를 타야 하고,
	// 그러려면 의도가 Mover InputCmd에 실려야 한다. 여기서는 버퍼 창을 여는 것이 전부다.
	else if (const ALNPGrappleAnchor* Anchor = Cast<ALNPGrappleAnchor>(Target))
	{
		if (ULNPInputHandlerComponent* InputHandler = Owner->FindComponentByClass<ULNPInputHandlerComponent>())
			InputHandler->RequestGrapple(Anchor->AnchorID);
	}
}

void ULNPInteractionComponent::Server_PickupDice_Implementation(ALNPLootDice* Dice)
{
	PickupDiceOnServer(Dice);
}

void ULNPInteractionComponent::PickupDiceOnServer(ALNPLootDice* Dice)
{
	APawn* Owner = Cast<APawn>(GetOwner());
	if (Owner == nullptr || Dice == nullptr)
		return;

	// 서버 재검증 — 거리·파괴 진행·획득 여부(bClaimed)를 함께 확인한다.
	// 동시 획득 시도는 서버 RPC 직렬화가 순서를 만들고, 첫 성공이 SetClaimed()로 나머지를 걸러낸다 (선착순).
	if (!ILNPInteractable::Execute_CanInteract(Dice, Owner))
	{
		UE_LOG(LogLootNPop, Log, TEXT("[LootDice] %s pickup rejected — already claimed or out of range (%s)"),
			*Dice->GetName(), *GetNameSafe(Owner));
		return;
	}

	ALNPPlayerState* PS = Owner->GetPlayerState<ALNPPlayerState>();
	ULNPInventoryComponent* Inventory = (PS != nullptr) ? PS->GetInventoryComponent() : nullptr;
	if (Inventory == nullptr)
		return;

	// ItemDef 유형별 인벤토리 편입 — 버프는 잔여 지속 시간이 유지된 채 넘어간다 (양도 규칙).
	// 가방 아이템은 인스턴스로 편입된다 (per-instance 정체성·스탯 보유).
	ULNPItemDefinitionBase* Item = Dice->GetItemDef();
	if (ULNPBuffData* Buff = Cast<ULNPBuffData>(Item))
	{
		Inventory->AddBuffItem(Buff, Dice->GetRemainingDuration());
	}
	else if (Item != nullptr)
	{
		// 무기 레벨·탄창 잔량은 Dice 페이로드에 실려 왔다 — 드랍한 사람이 아니라 줍는 사람 기준으로도 보존된다.
		Inventory->AddItemInstance(Item, Dice->GetItemLevel(), Dice->GetAmmoSpent());
	}
	else
	{
		// 디버그 스폰 등 페이로드 없는 Dice — 편입 없이 제거만
		UE_LOG(LogLootNPop, Log, TEXT("[LootDice] %s — no ItemDef, skipping inventory add"), *Dice->GetName());
	}

	UE_LOG(LogLootNPop, Log, TEXT("[LootDice] %s picked up — %s → %s"),
		*Dice->GetName(), *GetNameSafe(Item), *GetNameSafe(Owner));

	Dice->SetClaimed();
	Dice->Destroy();  // Destroy 복제로 전 클라이언트에서 제거
}

void ULNPInteractionComponent::Server_StartLooting_Implementation(ALNPLootPod* Pod)
{
	// 서버 재검증 — 클라이언트 판정 시점과의 레이스(Popped 직후 등)를 걸러낸다
	APawn* Owner = Cast<APawn>(GetOwner());
	if (Owner == nullptr || Pod == nullptr || !ILNPInteractable::Execute_CanInteract(Pod, Owner))
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

