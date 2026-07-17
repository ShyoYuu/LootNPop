// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Interaction/LNPInteractionComponent.h"
#include "Interaction/LNPInteractableRegistrySubsystem.h"
#include "LootPod/LNPLootPod.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "LootDice/LNPLootDice.h"
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
	const float RadiusSq = FMath::Square(InteractionRadius);
	const FVector OwnerLocation = Owner->GetActorLocation();

	for (const TWeakObjectPtr<AActor>& WeakActor : Registry->GetInteractables())
	{
		AActor* Actor = WeakActor.Get();

		// 파괴됐거나 풀에 반납되어 숨겨진 액터는 제외
		if (Actor == nullptr || Actor->IsHidden())
			continue;

		const float DistSq = FVector::DistSquared(OwnerLocation, Actor->GetActorLocation());
		if (DistSq > RadiusSq)
			continue;

		if (ALNPLootPod* Pod = Cast<ALNPLootPod>(Actor))
		{
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
		else if (ALNPLootDice* Dice = Cast<ALNPLootDice>(Actor))
		{
			// Dice는 거리 + "캐릭터 전방 시야" 판정 — Pod과 반대 방향이다. Pod은 자기 전방에 플레이어가
			// 있는지 보지만, Dice는 플레이어(캐릭터) 전방에 Dice가 들어와 있어야 한다. 뒤쪽 Dice는 더
			// 가까워도 후보에서 제외해 "지금 보고 있는 것"이 우선되게 한다. 시야 기준은 카메라가 아니라
			// 캐릭터 facing(GetActorForwardVector) — 감성적 캐릭터 시야.
			if (Dice->CanInteract(Owner))
			{
				const FVector ToDice = (Dice->GetActorLocation() - OwnerLocation).GetSafeNormal();
				const float FacingDot = FVector::DotProduct(Owner->GetActorForwardVector(), ToDice);
				if (FacingDot >= 0.342f)  // cos(70°) — 전방 140° 원뿔
					InteractionCandidates.Add(Dice);
			}
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

	// 후보 중 가장 가까운 "입력이 필요한" 타겟 선정 — Idle Pod 또는 Dice.
	// Looting 중인 Pod는 프레즌스 기반 기여라 입력이 불필요하므로 프롬프트를 띄우지 않는다.
	AActor* NewTarget = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector OwnerLocation = Owner->GetActorLocation();

	for (const TWeakObjectPtr<AActor>& Candidate : InteractionCandidates)
	{
		AActor* Actor = Candidate.Get();
		if (Actor == nullptr)
			continue;

		if (const ALNPLootPod* Pod = Cast<ALNPLootPod>(Actor))
		{
			if (Pod->GetCurrentState() != ELNPLootPodState::Idle)
				continue;
		}
		// Dice는 후보(거리 통과)면 항상 프롬프트 대상

		const float DistSq = FVector::DistSquared(OwnerLocation, Actor->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			NewTarget = Actor;
		}
	}

	if (CurrentPromptTarget.Get() == NewTarget)
		return;

	// 타입별 프롬프트 표시 전환 — 대상은 Pod 또는 Dice 둘 중 하나다
	auto SetPromptVisible = [](AActor* Target, bool bVisible)
	{
		if (ALNPLootPod* Pod = Cast<ALNPLootPod>(Target))
			Pod->SetInteractionPromptVisible(bVisible);
		else if (ALNPLootDice* Dice = Cast<ALNPLootDice>(Target))
			Dice->SetInteractionPromptVisible(bVisible);
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
		UE_LOG(LogLootNPop, Log, TEXT("[Interaction] Interact input — 프롬프트 대상 없음"));
		return;
	}

	// ALNPLootPod — 루팅 시작
	if (ALNPLootPod* Pod = Cast<ALNPLootPod>(Target))
	{
		if (!Pod->CanInteract(Owner))
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
		if (!Dice->CanInteract(Owner))
			return;

		if (Owner->HasAuthority())
			PickupDiceOnServer(Dice);
		else
			Server_PickupDice(Dice);
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
	if (!Dice->CanInteract(Owner))
	{
		UE_LOG(LogLootNPop, Log, TEXT("[LootDice] %s 획득 거부 — 이미 획득됐거나 거리 초과 (%s)"),
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
		Inventory->AddItemInstance(Item);
	}
	else
	{
		// 디버그 스폰 등 페이로드 없는 Dice — 편입 없이 제거만
		UE_LOG(LogLootNPop, Log, TEXT("[LootDice] %s — ItemDef 없음, 인벤토리 편입 생략"), *Dice->GetName());
	}

	UE_LOG(LogLootNPop, Log, TEXT("[LootDice] %s 획득 — %s → %s"),
		*Dice->GetName(), *GetNameSafe(Item), *GetNameSafe(Owner));

	Dice->SetClaimed();
	Dice->Destroy();  // Destroy 복제로 전 클라이언트에서 제거
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

