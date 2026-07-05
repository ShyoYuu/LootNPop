// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/GameplayCues/LNPGameplayCueNotify_AttackerHitStop.h"
#include "Character/LNPCharacterBase.h"

bool ULNPGameplayCueNotify_AttackerHitStop::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	ALNPCharacterBase* AttackerChar = Cast<ALNPCharacterBase>(MyTarget);
	if (!AttackerChar || AttackerChar->IsLocallyControlled())
		return true; // 공격자 본인 화면 — 예측 경로가 이미 처리했으므로 no-op.

	AttackerChar->ApplyHitStop(0.08f);
	return true;
}
