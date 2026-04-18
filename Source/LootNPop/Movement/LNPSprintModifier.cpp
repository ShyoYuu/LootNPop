// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPSprintModifier.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "MoverComponent.h"
#include "Movement/LNPCharacterMovementSettings.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/MovementUtils.h"

FLNPSprintModifier::FLNPSprintModifier()
{
	DurationMs = -1.0f; // Infinite duration until canceled
}

void FLNPSprintModifier::OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	ULNPCharacterMovementSettings* LNPSettings = MoverComp->FindSharedSettings_Mutable<ULNPCharacterMovementSettings>();
	UCommonLegacyMovementSettings* CommonSettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();

	if (LNPSettings && CommonSettings)
	{
		CommonSettings->MaxSpeed = LNPSettings->SprintSpeed;
		CommonSettings->Acceleration = LNPSettings->SprintAcceleration;
	}
}

void FLNPSprintModifier::OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	// Use CDO to find original common settings values
	if (const UMoverComponent* CDOMoverComp = UMovementUtils::GetOriginalComponentType<UMoverComponent>(MoverComp->GetOwner()))
	{
		const UCommonLegacyMovementSettings* OriginalCommonSettings = CDOMoverComp->FindSharedSettings<UCommonLegacyMovementSettings>();
		UCommonLegacyMovementSettings* CurrentCommonSettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();

		if (CurrentCommonSettings && OriginalCommonSettings)
		{
			// Restore to WalkSpeed from our custom settings if available, otherwise use CDO
			CurrentCommonSettings->MaxSpeed = OriginalCommonSettings->MaxSpeed;
			CurrentCommonSettings->Acceleration = OriginalCommonSettings->Acceleration;
		}
	}
}

bool FLNPSprintModifier::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	if (bExactMatch)
	{
		return TagToFind.MatchesTagExact(LNP_Mover_IsSprinting);
	}

	return TagToFind.MatchesTag(LNP_Mover_IsSprinting);
}

FMovementModifierBase* FLNPSprintModifier::Clone() const
{
	return new FLNPSprintModifier(*this);
}

void FLNPSprintModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
}

UScriptStruct* FLNPSprintModifier::GetScriptStruct() const
{
	return FLNPSprintModifier::StaticStruct();
}

FString FLNPSprintModifier::ToSimpleString() const
{
	return TEXT("LNP Sprint Modifier");
}
