// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPInventoryComponent.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPBuffData.h"
#include "Item/LNPInventoryItemInstance.h"
#include "GAS/LNPStatModifier.h"
#include "Player/LNPPlayerState.h"
#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

ULNPInventoryComponent::ULNPInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 소유 클라이언트 UI가 가방/버프 목록을 읽으려면 컴포넌트가 복제되어야 한다.
	SetIsReplicatedByDefault(true);
	// 아이템 인스턴스(UObject)를 등록 서브오브젝트 리스트로 복제한다 (Iris 네이티브 지원).
	bReplicateUsingRegisteredSubObjectList = true;
	// FastArray가 복제 콜백에서 소유 컴포넌트를 참조할 수 있도록 양측 생성자에서 설정.
	BagList.SetOwnerComponent(this);
	ActiveBuffList.SetOwnerComponent(this);
}

void ULNPInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 인벤토리는 사생활·대역폭상 소유 클라이언트에만 복제한다 (PlayerState의 Owner = 소유 PlayerController).
	DOREPLIFETIME_CONDITION(ULNPInventoryComponent, BagList, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ULNPInventoryComponent, ActiveBuffList, COND_OwnerOnly);
}

void ULNPInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

void ULNPInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (BuffRuntime.Num() > 0)
		TickBuffItems(DeltaTime);
}

// --- 가방 인스턴스 API ---

ULNPInventoryItemInstance* ULNPInventoryComponent::AddItemInstance(ULNPItemDefinitionBase* ItemDef)
{
	if (ItemDef == nullptr || GetOwnerRole() != ROLE_Authority)
		return nullptr;

	ULNPInventoryItemInstance* Instance = NewObject<ULNPInventoryItemInstance>(this);
	Instance->Init(ItemDef);

	// 인스턴스를 소유자 전용 등록 서브오브젝트로 복제(가방 FastArray와 동일 조건).
	AddReplicatedSubObject(Instance, COND_OwnerOnly);
	BagList.AddEntry(Instance);

	OnInventoryChanged.Broadcast();
	return Instance;
}

bool ULNPInventoryComponent::RemoveItemInstance(const FGuid& ItemId)
{
	if (GetOwnerRole() != ROLE_Authority)
		return false;

	ULNPInventoryItemInstance* Instance = FindItemInstance(ItemId);
	if (Instance == nullptr)
		return false;

	BagList.RemoveEntry(Instance);
	RemoveReplicatedSubObject(Instance);

	OnInventoryChanged.Broadcast();
	return true;
}

ULNPInventoryItemInstance* ULNPInventoryComponent::FindItemInstance(const FGuid& ItemId) const
{
	for (const FLNPInventoryEntry& Entry : BagList.GetEntries())
	{
		if (Entry.Instance != nullptr && Entry.Instance->GetItemId() == ItemId)
			return Entry.Instance;
	}
	return nullptr;
}

TArray<ULNPInventoryItemInstance*> ULNPInventoryComponent::GetBagInstances() const
{
	TArray<ULNPInventoryItemInstance*> Result;
	Result.Reserve(BagList.GetEntries().Num());
	for (const FLNPInventoryEntry& Entry : BagList.GetEntries())
	{
		if (Entry.Instance != nullptr)
			Result.Add(Entry.Instance);
	}
	return Result;
}

// --- 버프 인스턴스 API ---

void ULNPInventoryComponent::AddBuffItem(ULNPBuffData* ItemDef, float InRemainingDuration)
{
	if (ItemDef == nullptr || GetOwnerRole() != ROLE_Authority)
		return;

	UAbilitySystemComponent* ASC = GetASC();
	if (ASC == nullptr)
		return;

	ULNPInventoryItemInstance* Instance = NewObject<ULNPInventoryItemInstance>(this);
	Instance->Init(ItemDef);

	// 0은 유효한 지속 시간이 아니다 — 드랍 페이로드가 비어 있을 때만 아이템 정의값으로 폴백한다.
	// (영구 버프는 -1이 그대로 왕복해야 하므로 "> 0" 조건을 쓰면 안 된다.)
	float Duration = FMath::IsNearlyZero(InRemainingDuration) ? ItemDef->Duration : InRemainingDuration;
	if (FMath::IsNearlyZero(Duration))
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[Buff] %s has Duration 0 — treating as permanent. Use -1 for permanent buffs."),
			*GetNameSafe(ItemDef));
		Duration = LNPBuff::PermanentDuration;
	}

	Instance->SetRemainingDuration(Duration);  // 복제 스냅샷 (라이브 카운트다운은 UI 폴리시)

	// 서버 전용 런타임 — GAS 이펙트 적용 후 핸들 보관.
	FLNPBuffRuntime Runtime;
	Runtime.RemainingDuration = Duration;

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	for (const TSubclassOf<UGameplayEffect>& EffectClass : ItemDef->EffectsToApply)
	{
		if (!EffectClass)
			continue;

		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
		if (!Spec.IsValid())
			continue;

		Runtime.AppliedEffects.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
	}

	// 선언형 스탯 버프 (합연산 = AddBase, 곱연산 = MultiplyAdditive).
	LNPStat::ApplyModifiers(*ASC, ItemDef->StatModifiers, Runtime.AppliedEffects);

	BuffRuntime.Add(Instance->GetItemId(), MoveTemp(Runtime));

	AddReplicatedSubObject(Instance, COND_OwnerOnly);
	ActiveBuffList.AddEntry(Instance);

	OnInventoryChanged.Broadcast();
}

float ULNPInventoryComponent::RemoveBuffInstance(const FGuid& ItemId)
{
	if (GetOwnerRole() != ROLE_Authority)
		return 0.0f;

	float Remaining = 0.0f;
	if (const FLNPBuffRuntime* Runtime = BuffRuntime.Find(ItemId))
		Remaining = Runtime->RemainingDuration;

	ExpireBuffInstance(ItemId);
	return Remaining;
}

ULNPInventoryItemInstance* ULNPInventoryComponent::FindBuffInstance(const FGuid& ItemId) const
{
	for (const FLNPInventoryEntry& Entry : ActiveBuffList.GetEntries())
	{
		if (Entry.Instance != nullptr && Entry.Instance->GetItemId() == ItemId)
			return Entry.Instance;
	}
	return nullptr;
}

bool ULNPInventoryComponent::HasBuffItem(const ULNPBuffData* ItemDef) const
{
	for (const FLNPInventoryEntry& Entry : ActiveBuffList.GetEntries())
	{
		if (Entry.Instance != nullptr && Entry.Instance->GetDefinition() == ItemDef)
			return true;
	}
	return false;
}

TArray<ULNPInventoryItemInstance*> ULNPInventoryComponent::GetActiveBuffInstances() const
{
	TArray<ULNPInventoryItemInstance*> Result;
	Result.Reserve(ActiveBuffList.GetEntries().Num());
	for (const FLNPInventoryEntry& Entry : ActiveBuffList.GetEntries())
	{
		if (Entry.Instance != nullptr)
			Result.Add(Entry.Instance);
	}
	return Result;
}

UAbilitySystemComponent* ULNPInventoryComponent::GetASC() const
{
	if (const ALNPPlayerState* PS = Cast<ALNPPlayerState>(GetOwner()))
		return PS->GetAbilitySystemComponent();
	return nullptr;
}

void ULNPInventoryComponent::TickBuffItems(float DeltaTime)
{
	// 만료 후보를 먼저 수집한다 (만료 처리가 BuffRuntime을 변경하므로 순회 중 수정 방지).
	TArray<FGuid> Expired;
	for (TPair<FGuid, FLNPBuffRuntime>& Pair : BuffRuntime)
	{
		FLNPBuffRuntime& Runtime = Pair.Value;
		if (Runtime.RemainingDuration < 0.0f)
			continue;  // 영구 버프 (-1)

		Runtime.RemainingDuration -= DeltaTime;
		if (Runtime.RemainingDuration <= 0.0f)
			Expired.Add(Pair.Key);
	}

	for (const FGuid& ItemId : Expired)
		ExpireBuffInstance(ItemId);
}

void ULNPInventoryComponent::ExpireBuffInstance(const FGuid& ItemId)
{
	// GAS 이펙트 해제.
	if (FLNPBuffRuntime* Runtime = BuffRuntime.Find(ItemId))
	{
		if (UAbilitySystemComponent* ASC = GetASC())
		{
			for (const FActiveGameplayEffectHandle& Handle : Runtime->AppliedEffects)
				ASC->RemoveActiveGameplayEffect(Handle);
		}
		BuffRuntime.Remove(ItemId);
	}

	// 리스트·서브오브젝트에서 인스턴스 제거.
	if (ULNPInventoryItemInstance* Instance = FindBuffInstance(ItemId))
	{
		ActiveBuffList.RemoveEntry(Instance);
		RemoveReplicatedSubObject(Instance);
		OnInventoryChanged.Broadcast();
	}
}

namespace
{
	/** 디버그(Part B 검증용): 서버에서 아이템 인스턴스를 생성해 지정 플레이어 가방에 추가.
	 *  Usage: LNP.Debug.AddBagInstance <ItemDef 경로> [PlayerIndex]
	 *   - PlayerIndex 0 = 호스트(권위=원본, PostReplicatedAdd 안 뜸), 1 = 원격 클라(그 클라가 복제 수신 로그).
	 *   - 인수 없으면 서버의 모든 PlayerController 인덱스를 나열만 함. */
	FAutoConsoleCommandWithWorldAndArgs GLNPDebugAddBagInstance(
		TEXT("LNP.Debug.AddBagInstance"),
		TEXT("Server-only: create an item instance from a DataAsset path and add it to a target player's inventory bag. Args: <ItemDefPath> [PlayerIndex]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr)
				return;

			TArray<APlayerController*> Controllers;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
					Controllers.Add(PC);
			}

			if (Args.Num() < 1)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Debug.AddBagInstance] Usage: <ItemDefPath> [PlayerIndex]. Controllers on this world:"));
				for (int32 i = 0; i < Controllers.Num(); ++i)
				{
					UE_LOG(LogLootNPop, Warning, TEXT("  [%d] %s (Local=%d, Role=%d)"),
						i, *GetNameSafe(Controllers[i]->PlayerState), Controllers[i]->IsLocalController() ? 1 : 0, (int32)Controllers[i]->GetLocalRole());
				}
				return;
			}

			const int32 PlayerIndex = (Args.Num() >= 2) ? FCString::Atoi(*Args[1]) : 0;
			if (!Controllers.IsValidIndex(PlayerIndex))
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Debug.AddBagInstance] PlayerIndex %d out of range (0..%d)."), PlayerIndex, Controllers.Num() - 1);
				return;
			}

			ALNPPlayerState* PS = Controllers[PlayerIndex]->GetPlayerState<ALNPPlayerState>();
			ULNPInventoryComponent* Inventory = PS ? PS->GetInventoryComponent() : nullptr;
			if (Inventory == nullptr)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Debug.AddBagInstance] No inventory component on player %d."), PlayerIndex);
				return;
			}
			if (Inventory->GetOwnerRole() != ROLE_Authority)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Debug.AddBagInstance] Must run on authority (listen host / server)."));
				return;
			}

			ULNPItemDefinitionBase* ItemDef = LoadObject<ULNPItemDefinitionBase>(nullptr, *Args[0]);
			if (ItemDef == nullptr)
			{
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Debug.AddBagInstance] Failed to load ItemDef: %s"), *Args[0]);
				return;
			}

			// 픽업 경로(PickupDiceOnServer)와 동일하게 버프는 AddBuffItem으로 분기한다.
			if (ULNPBuffData* BuffDef = Cast<ULNPBuffData>(ItemDef))
			{
				Inventory->AddBuffItem(BuffDef);
				UE_LOG(LogLootNPop, Log, TEXT("[LNP.Debug.AddBagInstance] Added buff %s to player %d (%s). Active buffs now %d."),
					*GetNameSafe(BuffDef), PlayerIndex, *GetNameSafe(PS), Inventory->GetActiveBuffInstances().Num());
				return;
			}

			ULNPInventoryItemInstance* Instance = Inventory->AddItemInstance(ItemDef);
			UE_LOG(LogLootNPop, Log, TEXT("[LNP.Debug.AddBagInstance] Added %s to player %d (%s), ItemId=%s. Bag now has %d. Watch that client for PostReplicatedAdd."),
				*GetNameSafe(ItemDef), PlayerIndex, *GetNameSafe(PS), Instance ? *Instance->GetItemId().ToString() : TEXT("null"), Inventory->GetBagInstances().Num());
		}));
}
