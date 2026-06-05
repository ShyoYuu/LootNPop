// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPGuardModifier.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPCharacterMovementSettings.h"

#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/MovementUtils.h"

FLNPGuardModifier::FLNPGuardModifier()
{
	DurationMs = -1.0f; // 취소될 때까지 무한 지속
}

void FLNPGuardModifier::OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	ULNPCharacterMovementSettings* LNPSettings    = MoverComp->FindSharedSettings_Mutable<ULNPCharacterMovementSettings>();
	UCommonLegacyMovementSettings* CommonSettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();

	if (LNPSettings && CommonSettings)
	{
		CommonSettings->MaxSpeed     = LNPSettings->GuardWalkSpeed;
		CommonSettings->Acceleration = LNPSettings->GuardAcceleration;
	}
}

void FLNPGuardModifier::OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	if (const UMoverComponent* CDOMoverComp = UMovementUtils::GetOriginalComponentType<UMoverComponent>(MoverComp->GetOwner()))
	{
		const UCommonLegacyMovementSettings* OriginalCommonSettings = CDOMoverComp->FindSharedSettings<UCommonLegacyMovementSettings>();
		UCommonLegacyMovementSettings*       CurrentCommonSettings  = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();

		if (CurrentCommonSettings && OriginalCommonSettings)
		{
			CurrentCommonSettings->MaxSpeed     = OriginalCommonSettings->MaxSpeed;
			CurrentCommonSettings->Acceleration = OriginalCommonSettings->Acceleration;
		}
	}
}

bool FLNPGuardModifier::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	if (bExactMatch)
		return TagToFind.MatchesTagExact(LNPTAG_Mover_IsGuarding);

	return TagToFind.MatchesTag(LNPTAG_Mover_IsGuarding);
}

FMovementModifierBase* FLNPGuardModifier::Clone() const
{
	return new FLNPGuardModifier(*this);
}

void FLNPGuardModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
}

UScriptStruct* FLNPGuardModifier::GetScriptStruct() const
{
	return FLNPGuardModifier::StaticStruct();
}

FString FLNPGuardModifier::ToSimpleString() const
{
	return TEXT("LNP Guard Modifier");
}
