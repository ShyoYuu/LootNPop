// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GAS/Abilities/LNPAbility_RangedSpreadAttack.h"
#include "Character/LNPCharacterBase.h"
#include "HitDetection/LNPSpreadPattern.h"

TArray<FVector> ULNPAbility_RangedSpreadAttack::GetFireDirections(const FVector& SpawnPos) const
{
	const TArray<FVector> Base = Super::GetFireDirections(SpawnPos);
	const FVector BaseDir = Base.IsEmpty() ? FVector::ForwardVector : Base[0];

	const ALNPCharacterBase* Character = GetOwningCharacter();
	const FVector RefUp      = (nullptr != Character) ? Character->GetUpDirection() : FVector::UpVector;
	const FVector RefForward = (nullptr != Character) ? Character->GetActorForwardVector() : FVector::ForwardVector;

	// 배치 공식은 순수 엔티티 공격과 공유한다 (LNPSpreadPattern.h).
	TArray<FVector> Directions;
	LNPSpread::BuildHexRingDirections(BaseDir, RefUp, RefForward, HexRingCount, HexStepDegrees, Directions);
	return Directions;
}
