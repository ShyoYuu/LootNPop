// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassCommandBuffer.h"
#include "MassEntityManager.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueManager.h"

#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GAS/Effects/LNPGameplayEffect_Damage.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

namespace LNPHitDetection
{
	inline UAbilitySystemComponent* GetASC(AActor* Actor)
	{
		if (!IsValid(Actor)) return nullptr;
		IAbilitySystemInterface* I = Cast<IAbilitySystemInterface>(Actor);
		return I ? I->GetAbilitySystemComponent() : nullptr;
	}
}

// ──────────────────────────────────────────────────────────────────────────────
// 근접 패링
// ──────────────────────────────────────────────────────────────────────────────

/** 근접 공격 패링 성공 시 발동. 방어자 GA_ParrySuccess + 공격자 GA_Stagger 이벤트 전달. */
struct FLNPMeleeParryCommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<AActor> VictimActor;
		TWeakObjectPtr<AActor> AttackerActor;
	};

	FLNPMeleeParryCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(AActor* InVictim, AActor* InAttacker)
	{
		Entries.Add({ InVictim, InAttacker });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		for (const FEntry& Entry : Entries)
		{
			AActor* Victim = Entry.VictimActor.Get();
			UAbilitySystemComponent* VictimASC = LNPHitDetection::GetASC(Victim);
			if (!IsValid(VictimASC))
				continue;

			FGameplayCueParameters CueParams;
			CueParams.Location = Victim->GetActorLocation();
			VictimASC->ExecuteGameplayCue(TAG_GameplayCue_Parry_Success, CueParams);

			FGameplayEventData EventData;
			EventData.Target     = Victim;
			EventData.Instigator = Entry.AttackerActor.Get();
			VictimASC->HandleGameplayEvent(TAG_GameplayEvent_Parry_Success, &EventData);

			if (UAbilitySystemComponent* AttackerASC = LNPHitDetection::GetASC(Entry.AttackerActor.Get()))
			{
				FGameplayEventData StaggerData;
				StaggerData.Target     = Entry.AttackerActor.Get();
				StaggerData.Instigator = Victim;
				AttackerASC->HandleGameplayEvent(TAG_GameplayEvent_Parry_Stagger, &StaggerData);
			}

			UE_LOG(LogLootNPop, Log, TEXT("[Parry] Melee parry success"));
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};

// ──────────────────────────────────────────────────────────────────────────────
// 투사체 패링
// ──────────────────────────────────────────────────────────────────────────────

/** 투사체 패링 성공 시 발동. 방어자 GA_ParrySuccess 이벤트 + VFX 담당.
 *  Velocity/InstigatorTeam 반전은 Processor에서 이미 처리. */
struct FLNPProjectileParryCommand : public FMassBatchedCommand
{
	FLNPProjectileParryCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(AActor* InVictim)
	{
		Victims.Add(InVictim);
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		for (const TWeakObjectPtr<AActor>& WeakVictim : Victims)
		{
			AActor* Victim = WeakVictim.Get();
			UAbilitySystemComponent* VictimASC = LNPHitDetection::GetASC(Victim);
			if (!IsValid(VictimASC))
				continue;

			FGameplayCueParameters CueParams;
			CueParams.Location = Victim->GetActorLocation();
			VictimASC->ExecuteGameplayCue(TAG_GameplayCue_Parry_Success, CueParams);

			FGameplayEventData EventData;
			EventData.Target = Victim;
			VictimASC->HandleGameplayEvent(TAG_GameplayEvent_Parry_Success, &EventData);

			UE_LOG(LogLootNPop, Log, TEXT("[Parry] Projectile parry success"));
		}
	}

	virtual void Reset() override { Victims.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Victims.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Victims.Num(); }

private:
	TArray<TWeakObjectPtr<AActor>> Victims;
};

// ──────────────────────────────────────────────────────────────────────────────
// 가드
// ──────────────────────────────────────────────────────────────────────────────

/** 가드 성공 시 발동. GameplayCue_Guard_Block VFX/SFX 실행. */
struct FLNPGuardBlockCommand : public FMassBatchedCommand
{
	FLNPGuardBlockCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(AActor* InVictim)
	{
		Victims.Add(InVictim);
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		for (const TWeakObjectPtr<AActor>& WeakVictim : Victims)
		{
			AActor* Victim = WeakVictim.Get();
			UAbilitySystemComponent* VictimASC = LNPHitDetection::GetASC(Victim);
			if (!IsValid(VictimASC))
				continue;

			FGameplayCueParameters CueParams;
			CueParams.Location = Victim->GetActorLocation();
			VictimASC->ExecuteGameplayCue(TAG_GameplayCue_Guard_Block, CueParams);
			UE_LOG(LogLootNPop, Log, TEXT("[Guard] Block success"));
		}
	}

	virtual void Reset() override { Victims.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Victims.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Victims.Num(); }

private:
	TArray<TWeakObjectPtr<AActor>> Victims;
};

// ──────────────────────────────────────────────────────────────────────────────
// 데미지
// ──────────────────────────────────────────────────────────────────────────────

/** 순수 데미지 적용. 판정 로직 없음. */
struct FLNPApplyDamageGECommand : public FMassBatchedCommand
{
	struct FEntry
	{
		TWeakObjectPtr<AActor>       Actor;
		TSubclassOf<UGameplayEffect> EffectClass;
		float                        Damage;
	};

	FLNPApplyDamageGECommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(AActor* InActor, TSubclassOf<UGameplayEffect> InEffectClass, float InDamage)
	{
		Entries.Add({ InActor, InEffectClass, InDamage });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		for (const FEntry& Entry : Entries)
		{
			UAbilitySystemComponent* ASC = LNPHitDetection::GetASC(Entry.Actor.Get());
			if (!IsValid(ASC) || !Entry.EffectClass)
				continue;

			const float HpBefore = ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetHealthAttribute());

			FGameplayEffectContextHandle Ctx  = ASC->MakeEffectContext();
			FGameplayEffectSpecHandle    Spec = ASC->MakeOutgoingSpec(Entry.EffectClass, 1.0f, Ctx);
			if (!Spec.IsValid())
				continue;

			Spec.Data->SetSetByCallerMagnitude(TAG_GE_Data_Damage, Entry.Damage);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

			const float HpAfter = ASC->GetNumericAttribute(ULNPBaseAttributeSet::GetHealthAttribute());
			UE_LOG(LogLootNPop, Log, TEXT("[GE] HP: %.1f -> %.1f (damage=%.1f)"), HpBefore, HpAfter, Entry.Damage);
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};
