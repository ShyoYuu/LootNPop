// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/GameplayCues/LNPGameplayCueNotify_HitReact.h"
#include "Character/LNPCharacterBase.h"

bool ULNPGameplayCueNotify_HitReact::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (ALNPCharacterBase* VictimChar = Cast<ALNPCharacterBase>(MyTarget))
	{
		// 파라미터 규약은 FLNPApplyDamageGECommand가 정한다: Location=맞은 지점, Normal=피격자→공격자,
		// RawMagnitude=피해, NormalizedMagnitude=근접이면 1.
		VictimChar->PlayHitReact(Parameters.Normal, Parameters.Location, Parameters.RawMagnitude, Parameters.NormalizedMagnitude > 0.5f);
		VictimChar->ApplyHitStop(0.08f);
	}
	return true;
}
