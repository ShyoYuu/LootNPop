// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "MassCommandBuffer.h"
#include "MassEntityManager.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

#include "GAS/Attributes/LNPBaseAttributeSet.h"
#include "GAS/Effects/LNPGameplayEffect_Damage.h"
#include "LootNPop.h"

/** 배치 커맨드: GE 기반 피해 spec을 Actor의 ASC에 적용한다. 처리 단계 후 게임 Thread에서 실행. */
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
			AActor* Actor = Entry.Actor.Get();
			if (!IsValid(Actor) || !Entry.EffectClass)
				continue;

			IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Actor);
			if (!ASCInterface)
				continue;

			UAbilitySystemComponent* ASC = ASCInterface->GetAbilitySystemComponent();
			if (!IsValid(ASC))
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
