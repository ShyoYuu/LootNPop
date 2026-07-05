// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/GameplayCues/LNPGameplayCueNotify_HitReact.h"
#include "Character/LNPCharacterBase.h"

bool ULNPGameplayCueNotify_HitReact::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (ALNPCharacterBase* VictimChar = Cast<ALNPCharacterBase>(MyTarget))
	{
		VictimChar->PlayHitReact(Parameters.Normal);
		//VictimChar->ApplyHitStop(0.08f);
	}
	return true;
}
