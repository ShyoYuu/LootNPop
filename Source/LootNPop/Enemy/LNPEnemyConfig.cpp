// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Enemy/LNPEnemyConfig.h"

#if WITH_EDITOR
#include "HitDetection/LNPHitDetectionShared.h"
#include "Misc/DataValidation.h"

EDataValidationResult ULNPEnemyConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!LNPHitDetection::IsWeakSpotPlacementValid(GetHitCapsuleHalfHeight(), GetHitCapsuleRadius(),
		WeakSpotLocalOffset, WeakSpotRadius))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("WeakSpot must overlap and protrude from the hit capsule: HalfHeight=%.2f Radius=%.2f Offset=(%s) WeakSpotRadius=%.2f."),
			GetHitCapsuleHalfHeight(), GetHitCapsuleRadius(), *WeakSpotLocalOffset.ToString(), WeakSpotRadius)));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
