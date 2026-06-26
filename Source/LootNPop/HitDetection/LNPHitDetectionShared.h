// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassCommandBuffer.h"
#include "MassEntityManager.h"
#include "MassActorSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueManager.h"

#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GAS/Effects/LNPGameplayEffect_Damage.h"
#include "Character/LNPCharacterBase.h"
#include "LNPGameplayTags.h"
#include "LootNPop.h"

namespace LNPHitDetection
{
	inline UAbilitySystemComponent* GetASC(AActor* Actor)
	{
		if (!IsValid(Actor))
			return nullptr;
		ALNPCharacterBase* Character = Cast<ALNPCharacterBase>(Actor);
		return Character ? Character->GetAbilitySystemComponent() : nullptr;
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
		FMassEntityHandle      AttackerEntity;
	};

	FLNPMeleeParryCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(AActor* InVictim, FMassEntityHandle InAttacker)
	{
		Entries.Add({ InVictim, InAttacker });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		UMassActorSubsystem* ActorSub = EntityManager.GetWorld()
			? EntityManager.GetWorld()->GetSubsystem<UMassActorSubsystem>() : nullptr;

		for (const FEntry& Entry : Entries)
		{
			AActor* Victim = Entry.VictimActor.Get();
			UAbilitySystemComponent* VictimASC = LNPHitDetection::GetASC(Victim);
			if (!IsValid(VictimASC))
				continue;

			FGameplayCueParameters CueParams;
			CueParams.Location = Victim->GetActorLocation();
			VictimASC->ExecuteGameplayCue(TAG_GameplayCue_Parry_Success, CueParams);

			AActor* Attacker = nullptr;
			if (ActorSub && Entry.AttackerEntity.IsSet() && EntityManager.IsEntityValid(Entry.AttackerEntity))
				Attacker = ActorSub->GetActorFromHandle(Entry.AttackerEntity);

			FGameplayEventData EventData;
			EventData.Target     = Victim;
			EventData.Instigator = Attacker;
			VictimASC->HandleGameplayEvent(TAG_GameplayEvent_Parry_Success, &EventData);

			if (UAbilitySystemComponent* AttackerASC = LNPHitDetection::GetASC(Attacker))
			{
				FGameplayEventData StaggerData;
				StaggerData.Target     = Attacker;
				StaggerData.Instigator = Victim;
				AttackerASC->HandleGameplayEvent(TAG_GameplayEvent_Parry_Stagger, &StaggerData);
			}

			if (ALNPCharacterBase* AttackerPawn = Cast<ALNPCharacterBase>(Attacker))
			{
				AttackerPawn->PlayMontage(TAG_Montage_Situation_ParrySuccess, TAG_Montage_Value_Parry_Parried);

				constexpr float DirectionWeight = 0.7f;
				constexpr float UpWeight        = 0.3f;

				const FVector AwayDir      = (AttackerPawn->GetActorLocation() - Victim->GetActorLocation()).GetSafeNormal();
				const FVector KnockbackDir = (AwayDir * DirectionWeight + AttackerPawn->GetUpDirection() * UpWeight).GetSafeNormal();
				AttackerPawn->ApplyKnockback(KnockbackDir, 2000.0f);
			}

			if (ALNPCharacterBase* VictimPawn = Cast<ALNPCharacterBase>(Victim))
			{
				VictimPawn->PlayMontage(TAG_Montage_Situation_ParrySuccess, TAG_Montage_Value_Parry_Parrier);
			}

			UE_LOG(LogLootNPop, Log, TEXT("[Parry] Melee parry success"));
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};;

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
		TWeakObjectPtr<AActor>       Victim;
		FMassEntityHandle            AttackerEntity;
		TSubclassOf<UGameplayEffect> EffectClass;
		float                        Damage;
		FVector                      HitFromDirection;
		float                        KnockbackStrength;
	};

	FLNPApplyDamageGECommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(AActor* InVictim, FMassEntityHandle InAttacker, TSubclassOf<UGameplayEffect> InEffectClass, float InDamage, FVector InHitFromDir, float InKnockbackStrength = 0.f)
	{
		Entries.Add({ InVictim, InAttacker, InEffectClass, InDamage, InHitFromDir, InKnockbackStrength });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override
	{
		UMassActorSubsystem* ActorSub = EntityManager.GetWorld()
			? EntityManager.GetWorld()->GetSubsystem<UMassActorSubsystem>() : nullptr;

		for (const FEntry& Entry : Entries)
		{
			AActor* Victim = Entry.Victim.Get();
			UAbilitySystemComponent* ASC = LNPHitDetection::GetASC(Victim);
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
			if (0.f < HpBefore)
				UE_LOG(LogLootNPop, Log, TEXT("[GE] HP: %.1f -> %.1f (damage=%.1f)"), HpBefore, HpAfter, Entry.Damage);

			AActor* Attacker = nullptr;
			if (ActorSub && Entry.AttackerEntity.IsSet() && EntityManager.IsEntityValid(Entry.AttackerEntity))
				Attacker = ActorSub->GetActorFromHandle(Entry.AttackerEntity);

			if (ALNPCharacterBase* VictimChar = Cast<ALNPCharacterBase>(Victim))
			{
				VictimChar->PlayHitReact(Entry.HitFromDirection);
				VictimChar->ApplyHitStop(0.08f);
				if (Entry.KnockbackStrength > 0.f)
					VictimChar->ApplyKnockback(Entry.HitFromDirection, Entry.KnockbackStrength);
			}
			if (ALNPCharacterBase* AttackerChar = Cast<ALNPCharacterBase>(Attacker))
				AttackerChar->ApplyHitStop(0.08f);
		}
	}

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};;
