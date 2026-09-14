// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_Reload.h"
#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "Character/LNPCharacterBase.h"
#include "Item/LNPWeaponData.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "LNPGameplayTags.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"

namespace
{
	const ULNPWeaponData* GetMagazineWeapon(const FGameplayAbilityActorInfo* ActorInfo)
	{
		// CanActivateAbility는 CDO에서도 불리므로 CurrentActorInfo가 아니라 인수에서 읽는다.
		const ALNPCharacterBase* Ch = ActorInfo ? Cast<ALNPCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
		const ULNPWeaponData* WeaponDef = Ch ? Ch->GetActiveWeaponDef() : nullptr;
		return (WeaponDef && WeaponDef->MagazineSize > 0) ? WeaponDef : nullptr;
	}
}

ULNPAbility_Reload::ULNPAbility_Reload()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_Reload);
	SetAssetTags(Tags);

	ActivationOwnedTags.AddTag(TAG_State_Reloading);
	ActivationBlockedTags.AddTag(TAG_State_Staggered);
	ActivationBlockedTags.AddTag(TAG_State_Reloading);
}

bool ULNPAbility_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
		return false;

	if (GetMagazineWeapon(ActorInfo) == nullptr)
		return false;

	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC == nullptr)
		return false;

	// 가득 차 있으면 재장전하지 않는다.
	return ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetMagazineAmmoAttribute())
		< ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetMagazineSizeAttribute());
}

void ULNPAbility_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ALNPCharacterBase* Character = GetOwningCharacter();
	const ULNPWeaponData* WeaponDef = GetMagazineWeapon(ActorInfo);
	if (Character == nullptr || WeaponDef == nullptr || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ReloadDuration = WeaponDef->GetReloadDuration(GetAttackSpeed());
	ActivationTime = GetWorld()->GetTimeSeconds();

	// 대시는 재장전을 끊는다. 대시는 서버·소유 클라 양쪽의 Mover 시뮬레이션에서 각각 발송된다.
	if (ULNPCharacterMoverComponent* Mover = Character->GetMoverComponent())
	{
		BoundMover = Mover;
		DashExecutedHandle = Mover->OnDashExecuted.AddUObject(this, &ULNPAbility_Reload::OnDashExecuted);
	}

	// 무기 메시 애니 — 활성화 예측 창 안에서 걸어야 소유 클라가 예측 재생하고 서버 복제와 중복되지 않는다.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddGameplayCue(TAG_GameplayCue_Weapon_Reload);
		bCueAdded = true;
	}

	// 캐릭터 몽타주는 재장전 시간에 길이를 맞춘다. 없으면 애니 없이 시간만 흐른다.
	if (UAnimMontage* Montage = Character->EvaluateMontage(TAG_Montage_Situation_Reload))
	{
		const float Rate = Montage->GetPlayLength() / FMath::Max(0.05f, ReloadDuration);
		if (UAbilityTask_PlayMontageAndWait* MontageTask =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, Rate))
		{
			MontageTask->ReadyForActivation();
		}
	}

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ReloadDuration);
	if (WaitTask == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	WaitTask->OnFinish.AddDynamic(this, &ULNPAbility_Reload::OnReloadFinished);
	WaitTask->ReadyForActivation();
}

void ULNPAbility_Reload::OnReloadFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULNPAbility_Reload::OnDashExecuted()
{
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void ULNPAbility_Reload::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsActive())
	{
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}

	if (ULNPCharacterMoverComponent* Mover = BoundMover.Get())
		Mover->OnDashExecuted.Remove(DashExecutedHandle);
	BoundMover.Reset();
	DashExecutedHandle.Reset();

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

	if (ASC && bCueAdded)
		ASC->RemoveGameplayCue(TAG_GameplayCue_Weapon_Reload);
	bCueAdded = false;

	// 서버·소유 클라가 각자 채운다 — 클라가 복제를 기다리면 RTT 동안 0발로 보여 자동 재장전이 다시 걸린다.
	// 서버 값이 복제되며 같은 값으로 수렴한다.
	const double Elapsed = GetWorld() ? GetWorld()->GetTimeSeconds() - ActivationTime : 0.0;
	if (ASC && !bWasCancelled && ReloadDuration > 0.f && Elapsed >= ReloadDuration * MinCompletionRatio)
	{
		ASC->SetNumericAttributeBase(ULNPBaseAttributeSet::GetMagazineAmmoAttribute(),
			ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetMagazineSizeAttribute()));
	}
	ReloadDuration = 0.f;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
