// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPInventoryComponent.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPBuffData.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPWeaponData.h"
#include "GAS/LNPStatModifier.h"
#include "Player/LNPPlayerState.h"
#include "Config/LNPSettings.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

ULNPInventoryComponent::ULNPInventoryComponent()
{
	// 버프 만료는 FTimerManager가 담당하므로 틱이 필요 없다 (매 프레임 감산 방식에서 전환).
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

// --- 가방 인스턴스 API ---

ULNPInventoryItemInstance* ULNPInventoryComponent::AddItemInstance(ULNPItemDefinitionBase* ItemDef, int32 InLevel)
{
	if (ItemDef == nullptr || GetOwnerRole() != ROLE_Authority)
		return nullptr;

	ULNPInventoryItemInstance* Instance = NewObject<ULNPInventoryItemInstance>(this);
	Instance->Init(ItemDef);

	// 레벨은 반드시 자동 장착(TryAutoEquipWeapon)보다 **먼저** 확정해야 한다 —
	// 장착이 이 값을 읽어 GAS 어빌리티 레벨과 스탯 행을 고르기 때문이다.
	Instance->AddStatTagStack(TAG_Item_Level, FMath::Max(1, InLevel));

	// 인스턴스를 소유자 전용 등록 서브오브젝트로 복제(가방 FastArray와 동일 조건).
	AddReplicatedSubObject(Instance, COND_OwnerOnly);
	BagList.AddEntry(Instance);

	// 무기 슬롯이 비어 있으면 자동 장착 (안전망 — 기획상 맨손 상태는 없어야 한다).
	// 장착 여부 판단은 장비 상태를 소유한 쪽이 한다. 브로드캐스트 앞에 두어야 UI가 최종 상태를 본다.
	if (const ALNPPlayerState* PS = Cast<ALNPPlayerState>(GetOwner()))
		if (ULNPEquipmentComponent* Equipment = PS->GetEquipmentComponent())
			Equipment->TryAutoEquipWeapon(Instance);

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

ULNPInventoryItemInstance* ULNPInventoryComponent::FindBagInstanceByDefinition(const ULNPItemDefinitionBase* ItemDef) const
{
	if (ItemDef == nullptr)
		return nullptr;

	for (const FLNPInventoryEntry& Entry : BagList.GetEntries())
	{
		if (Entry.Instance != nullptr && Entry.Instance->GetDefinition() == ItemDef)
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

// --- 합성 (Merge) ---

void ULNPInventoryComponent::CollectMergeMaterials(const ULNPInventoryItemInstance* Target, int32 MaxCount,
                                                   TArray<ULNPInventoryItemInstance*>& OutMaterials) const
{
	OutMaterials.Reset();
	if (Target == nullptr || MaxCount == 0)
		return;

	const ULNPItemDefinitionBase* TargetDef = Target->GetDefinition();
	const int32 TargetLevel = Target->GetItemLevel();

	for (const FLNPInventoryEntry& Entry : BagList.GetEntries())
	{
		ULNPInventoryItemInstance* Candidate = Entry.Instance;
		if (Candidate == nullptr || Candidate == Target)
			continue;

		// 장착 중인 무기는 재료로 쓸 수 없다 (기획). Target 자신이 장착본인 경우는 여기서 걸리지 않는다 —
		// Target은 소모되는 게 아니라 결과물이 되기 때문이다.
		if (Candidate->IsEquipped())
			continue;

		if (Candidate->GetDefinition() != TargetDef || Candidate->GetItemLevel() != TargetLevel)
			continue;

		OutMaterials.Add(Candidate);
		if (MaxCount > 0 && OutMaterials.Num() >= MaxCount)
			return;
	}
}

bool ULNPInventoryComponent::CanMergeItem(const ULNPInventoryItemInstance* Target, int32& OutHave, int32& OutNeed) const
{
	OutHave = 0;
	OutNeed = 0;

	if (Target == nullptr)
		return false;

	// 합성 대상은 무기뿐이다 — 레벨 테이블이 곧 레벨의 정의이자 상한이기 때문이다.
	const ULNPWeaponData* WeaponDef = Cast<ULNPWeaponData>(Target->GetDefinition());
	if (WeaponDef == nullptr)
		return false;

	if (Target->GetItemLevel() >= WeaponDef->GetMaxLevel())
		return false;

	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	const int32 MaterialCount = Settings ? Settings->WeaponMergeMaterialCount : 3;
	OutNeed = FMath::Max(1, MaterialCount - 1);

	TArray<ULNPInventoryItemInstance*> Materials;
	CollectMergeMaterials(Target, OutNeed, Materials);
	OutHave = Materials.Num();

	return true;
}

bool ULNPInventoryComponent::TryMergeItem(const FGuid& TargetItemId)
{
	if (GetOwnerRole() != ROLE_Authority)
		return false;

	ULNPInventoryItemInstance* Target = FindItemInstance(TargetItemId);

	// 클라이언트가 보낸 요청이므로 버튼 상태와 무관하게 서버가 처음부터 다시 판정한다.
	int32 Have = 0;
	int32 Need = 0;
	if (!CanMergeItem(Target, Have, Need) || Have < Need)
	{
		UE_LOG(LogLootNPop, Log, TEXT("[Merge] Rejected — %s (have %d/%d)"),
			*GetNameSafe(Target ? Target->GetDefinition() : nullptr), Have, Need);
		return false;
	}

	// 제거하면서 순회하면 BagList가 흔들리므로 대상을 먼저 확정한다.
	TArray<ULNPInventoryItemInstance*> Materials;
	CollectMergeMaterials(Target, Need, Materials);

	TArray<FGuid> MaterialIds;
	MaterialIds.Reserve(Materials.Num());
	for (const ULNPInventoryItemInstance* Material : Materials)
		MaterialIds.Add(Material->GetItemId());

	for (const FGuid& MaterialId : MaterialIds)
		RemoveItemInstance(MaterialId);

	const int32 NewLevel = Target->GetItemLevel() + 1;
	Target->SetItemLevel(NewLevel);

	// 장착 중인 무기를 올렸다면 GAS 부여를 새 레벨로 다시 건다 (어빌리티 스펙 레벨·스탯 GE).
	if (Target->IsEquipped())
	{
		if (const ALNPPlayerState* PS = Cast<ALNPPlayerState>(GetOwner()))
			if (ULNPEquipmentComponent* Equipment = PS->GetEquipmentComponent())
				Equipment->RefreshWeaponSlotGrants();
	}

	UE_LOG(LogLootNPop, Log, TEXT("[Merge] %s: %s -> Lv.%d (consumed %d, equipped=%d)"),
		*GetNameSafe(GetOwner()), *GetNameSafe(Target->GetDefinition()), NewLevel,
		MaterialIds.Num(), Target->IsEquipped() ? 1 : 0);

	OnInventoryChanged.Broadcast();
	return true;
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

	// 기간제 버프는 만료 시각을 여기서 한 번 확정하고 타이머에 맡긴다 — 매 프레임 감산이 없어
	// 버프별 float 누적 오차가 생기지 않고, 컴포넌트 틱도 필요 없다.
	// 영구 버프(-1)는 타이머를 걸지 않으며, 그 상태(핸들 무효)가 곧 "영구" 판별이 된다.
	if (Duration > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			Runtime.ExpireTimer,
			FTimerDelegate::CreateUObject(this, &ULNPInventoryComponent::ExpireBuffInstance, Instance->GetItemId()),
			Duration, /*bLoop=*/false);
		Runtime.ExpireWorldTime = GetWorld()->GetTimeSeconds() + Duration;
	}

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

	// 타이머 핸들 유효 = 기간제 → 만료 시각에서 남은 초를 역산한다.
	// 무효 = 영구 → -1을 그대로 반환해 Dice 페이로드를 왕복시킨다 (0은 "페이로드 없음"이라 쓸 수 없다).
	float Remaining = 0.0f;
	if (const FLNPBuffRuntime* Runtime = BuffRuntime.Find(ItemId))
	{
		Remaining = Runtime->ExpireTimer.IsValid()
			? static_cast<float>(Runtime->ExpireWorldTime - GetWorld()->GetTimeSeconds())
			: LNPBuff::PermanentDuration;
	}

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

void ULNPInventoryComponent::ExpireBuffInstance(FGuid ItemId)
{
	// GAS 이펙트 해제.
	if (FLNPBuffRuntime* Runtime = BuffRuntime.Find(ItemId))
	{
		// 양도·조기 제거로 들어온 경로라면 만료 타이머가 아직 대기 중이다 — 반드시 해제한다.
		// (타이머 발화로 들어온 경로에서는 자기 콜백 안에서의 ClearTimer를 엔진이 안전하게 처리한다.)
		GetWorld()->GetTimerManager().ClearTimer(Runtime->ExpireTimer);

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
	 *  Usage: LNP.Debug.AddBagInstance <ItemDef 경로> [PlayerIndex] [Level]
	 *   - PlayerIndex 0 = 호스트(권위=원본, PostReplicatedAdd 안 뜸), 1 = 원격 클라(그 클라가 복제 수신 로그).
	 *   - Level 생략 시 1. 합성 테스트는 같은 레벨 사본을 n개 만들어야 하므로 이 인수가 필요하다.
	 *   - 인수 없으면 서버의 모든 PlayerController 인덱스를 나열만 함. */
	FAutoConsoleCommandWithWorldAndArgs GLNPDebugAddBagInstance(
		TEXT("LNP.Debug.AddBagInstance"),
		TEXT("Server-only: create an item instance from a DataAsset path and add it to a target player's inventory bag. Args: <ItemDefPath> [PlayerIndex] [Level]"),
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
				UE_LOG(LogLootNPop, Warning, TEXT("[LNP.Debug.AddBagInstance] Usage: <ItemDefPath> [PlayerIndex] [Level]. Controllers on this world:"));
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

			const int32 ItemLevel = (Args.Num() >= 3) ? FCString::Atoi(*Args[2]) : 1;

			ULNPInventoryItemInstance* Instance = Inventory->AddItemInstance(ItemDef, ItemLevel);
			UE_LOG(LogLootNPop, Log, TEXT("[LNP.Debug.AddBagInstance] Added %s Lv.%d to player %d (%s), ItemId=%s. Bag now has %d. Watch that client for PostReplicatedAdd."),
				*GetNameSafe(ItemDef), Instance ? Instance->GetItemLevel() : 0, PlayerIndex, *GetNameSafe(PS),
				Instance ? *Instance->GetItemId().ToString() : TEXT("null"), Inventory->GetBagInstances().Num());
		}));
}
