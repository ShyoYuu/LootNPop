// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPEquipmentComponent.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Item/LNPWeaponData.h"
#include "Item/LNPSkillData.h"
#include "Item/LNPInventoryItemInstance.h"
#include "Character/LNPCharacterBase.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "GAS/LNPStatModifier.h"
#include "Player/LNPPlayerState.h"
#include "Config/LNPSettings.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

ULNPEquipmentComponent::ULNPEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// WeaponSlot이 장비 상태의 단일 원본이므로 컴포넌트 자체가 복제되어야 한다.
	// PlayerState는 bAlwaysRelevant이라 시뮬레이티드 프록시를 포함한 전 클라이언트가 받는다.
	SetIsReplicatedByDefault(true);
}

void ULNPEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 조건 없음 — 프록시도 받아야 한다. 원격 관전 시 근접 판정이 무기 정의를 읽기 때문.
	// 실제 복제되는 것은 Definition 하나뿐이다 (나머지 필드는 NotReplicated, FLNPWeaponInstance 참조).
	DOREPLIFETIME(ULNPEquipmentComponent, WeaponSlot);
}

void ULNPEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ULNPSettings* Settings = GetDefault<ULNPSettings>())
		MaxActiveSkillSlots = Settings->MaxActiveSkillSlots;

	ActiveSkillSlots.SetNum(MaxActiveSkillSlots);

	// 권위 게이트 필수 — 클라이언트가 스스로 DefaultWeapon을 장착하면 복제 값과 충돌한다.
	// (게이트가 없던 시절, 프록시에 DefaultWeapon이 영구히 남아 실제 장착 무기와 어긋났다.)
	if (DefaultWeapon && GetOwnerRole() == ROLE_Authority)
		EquipWeapon(DefaultWeapon);
}

bool ULNPEquipmentComponent::EnsureAuthority(const TCHAR* Context) const
{
	return ensureMsgf(GetOwnerRole() == ROLE_Authority,
		TEXT("ULNPEquipmentComponent::%s called without authority — route through ALNPPlayerCharacter::Server_Equip*"),
		Context);
}

void ULNPEquipmentComponent::EquipWeapon(ULNPWeaponData* WeaponDef)
{
	if (!EnsureAuthority(TEXT("EquipWeapon")))
		return;

	ClearWeaponSlot();
	if (WeaponDef)
	{
		WeaponSlot.Definition = WeaponDef;
		GrantItemImpl(WeaponDef, WeaponSlot.GrantedAbilities, WeaponSlot.AppliedEffects);
	}

	OnWeaponSlotApplied();
}

void ULNPEquipmentComponent::EquipWeaponInstance(ULNPInventoryItemInstance* Instance)
{
	if (!EnsureAuthority(TEXT("EquipWeaponInstance")))
		return;

	if (Instance == nullptr)
		return;

	ULNPWeaponData* WeaponDef = Cast<ULNPWeaponData>(Instance->GetDefinition());
	if (WeaponDef == nullptr)
		return;

	ClearWeaponSlot();

	WeaponSlot.Definition = WeaponDef;
	WeaponSlot.SourceInstance = Instance;
	Instance->SetEquipped(true);  // 복제되어 소유 클라 UI가 가방에서 숨긴다
	GrantItemImpl(WeaponDef, WeaponSlot.GrantedAbilities, WeaponSlot.AppliedEffects);

	OnWeaponSlotApplied();
}

void ULNPEquipmentComponent::UnequipWeapon()
{
	if (!EnsureAuthority(TEXT("UnequipWeapon")))
		return;

	if (!WeaponSlot.IsValid())
		return;

	ClearWeaponSlot();
	OnWeaponSlotApplied();
}

// 통지하지 않는다 — 교체(EquipWeapon)의 중간 단계로도 쓰이기 때문이다.
// 여기서 통지하면 무기를 바꿀 때마다 "맨손 → 새 무기" 두 번 적용되어 애님 레이어가 한 번 튄다.
void ULNPEquipmentComponent::ClearWeaponSlot()
{
	if (!WeaponSlot.IsValid())
		return;

	if (WeaponSlot.SourceInstance)
		WeaponSlot.SourceInstance->SetEquipped(false);
	RevokeItemImpl(WeaponSlot.GrantedAbilities, WeaponSlot.AppliedEffects);
	WeaponSlot.Reset();
}

void ULNPEquipmentComponent::OnRep_WeaponSlot()
{
	OnWeaponSlotApplied();
}

void ULNPEquipmentComponent::OnWeaponSlotApplied()
{
	PushWeaponToPawn();
	OnEquipmentChanged.Broadcast();
}

void ULNPEquipmentComponent::PushWeaponToPawn() const
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (PS == nullptr)
		return;

	// Pawn이 아직 없거나 PlayerState와 아직 연결되지 않았으면 아무것도 하지 않는다 —
	// Pawn 쪽이 BeginPlay/OnRep_PlayerState/PossessedBy에서 스스로 끌어간다 (풀 방향).
	if (ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(PS->GetPawn()))
		Character->ApplyWeaponVisuals(WeaponSlot.Definition);
}

void ULNPEquipmentComponent::EquipActiveSkill(int32 SlotIndex, ULNPSkillData* SkillDef)
{
	if (!ActiveSkillSlots.IsValidIndex(SlotIndex))
		return;

	UnequipActiveSkill(SlotIndex);
	if (SkillDef)
	{
		ActiveSkillSlots[SlotIndex].Definition = SkillDef;
		if (GetOwner() && GetOwner()->HasAuthority())
			GrantItemImpl(SkillDef, ActiveSkillSlots[SlotIndex].GrantedAbilities, ActiveSkillSlots[SlotIndex].AppliedEffects);
	}
}

void ULNPEquipmentComponent::UnequipActiveSkill(int32 SlotIndex)
{
	if (!ActiveSkillSlots.IsValidIndex(SlotIndex))
		return;

	FLNPSkillInstance& Slot = ActiveSkillSlots[SlotIndex];
	if (!Slot.IsValid())
		return;

	if (GetOwner() && GetOwner()->HasAuthority())
		RevokeItemImpl(Slot.GrantedAbilities, Slot.AppliedEffects);
	Slot.Reset();
}

void ULNPEquipmentComponent::AddPassiveSkill(ULNPSkillData* SkillDef)
{
	if (!SkillDef)
		return;

	FLNPSkillInstance& NewInstance = PassiveSkillInstances.AddDefaulted_GetRef();
	NewInstance.Definition = SkillDef;
	if (GetOwner() && GetOwner()->HasAuthority())
		GrantItemImpl(SkillDef, NewInstance.GrantedAbilities, NewInstance.AppliedEffects);
}

void ULNPEquipmentComponent::RemovePassiveSkill(ULNPSkillData* SkillDef)
{
	for (int32 i = 0; i < PassiveSkillInstances.Num(); ++i)
	{
		if (PassiveSkillInstances[i].Definition == SkillDef)
		{
			if (GetOwner() && GetOwner()->HasAuthority())
				RevokeItemImpl(PassiveSkillInstances[i].GrantedAbilities, PassiveSkillInstances[i].AppliedEffects);
			PassiveSkillInstances.RemoveAt(i);
			return;
		}
	}
}

bool ULNPEquipmentComponent::IsEquipped(const ULNPItemDefinitionBase* ItemDef) const
{
	if (ItemDef == nullptr)
		return false;

	if (WeaponSlot.Definition == ItemDef)
		return true;

	for (const FLNPSkillInstance& Slot : ActiveSkillSlots)
	{
		if (Slot.Definition == ItemDef)
			return true;
	}

	for (const FLNPSkillInstance& Passive : PassiveSkillInstances)
	{
		if (Passive.Definition == ItemDef)
			return true;
	}

	return false;
}

bool ULNPEquipmentComponent::IsEquippedInstance(const FGuid& ItemId) const
{
	if (!ItemId.IsValid())
		return false;

	if (WeaponSlot.SourceInstance && WeaponSlot.SourceInstance->GetItemId() == ItemId)
		return true;

	for (const FLNPSkillInstance& Slot : ActiveSkillSlots)
	{
		if (Slot.SourceInstance && Slot.SourceInstance->GetItemId() == ItemId)
			return true;
	}

	for (const FLNPSkillInstance& Passive : PassiveSkillInstances)
	{
		if (Passive.SourceInstance && Passive.SourceInstance->GetItemId() == ItemId)
			return true;
	}

	return false;
}

UAbilitySystemComponent* ULNPEquipmentComponent::GetASC() const
{
	if (const ALNPPlayerState* PS = Cast<ALNPPlayerState>(GetOwner()))
		return PS->GetAbilitySystemComponent();
	return nullptr;
}

void ULNPEquipmentComponent::GrantItemImpl(ULNPItemDefinitionBase* Def,
                                            TArray<FGameplayAbilitySpecHandle>& OutAbilities,
                                            TArray<FActiveGameplayEffectHandle>& OutEffects)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC || !Def)
		return;

	for (const TSubclassOf<ULNPGameplayAbility>& AbilityClass : Def->AbilitiesToGrant)
	{
		if (!AbilityClass)
			continue;

		OutAbilities.Add(ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass)));
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	for (const TSubclassOf<UGameplayEffect>& EffectClass : Def->EffectsToApply)
	{
		if (!EffectClass)
			continue;

		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
		if (!Spec.IsValid())
			continue;

		OutEffects.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
	}

	// 무기 스텟 등 선언형 스탯 변경. 해제는 RevokeItemImpl이 핸들로 처리한다.
	LNPStat::ApplyModifiers(*ASC, Def->StatModifiers, OutEffects);
}

void ULNPEquipmentComponent::RevokeItemImpl(TArray<FGameplayAbilitySpecHandle>& Abilities,
                                             TArray<FActiveGameplayEffectHandle>& Effects)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
		return;

	for (const FGameplayAbilitySpecHandle& Handle : Abilities)
		ASC->ClearAbility(Handle);
	for (const FActiveGameplayEffectHandle& Handle : Effects)
		ASC->RemoveActiveGameplayEffect(Handle);
}
