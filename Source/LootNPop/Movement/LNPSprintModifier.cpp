// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPSprintModifier.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPCharacterMovementSettings.h"

#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/MovementUtils.h"

FLNPSprintModifier::FLNPSprintModifier()
{
	DurationMs = -1.0f; // 취소될 때까지 무한 지속
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
	// CDO를 사용하여 원래 공통 설정 값 조회
	if (const UMoverComponent* CDOMoverComp = UMovementUtils::GetOriginalComponentType<UMoverComponent>(MoverComp->GetOwner()))
	{
		const UCommonLegacyMovementSettings* OriginalCommonSettings = CDOMoverComp->FindSharedSettings<UCommonLegacyMovementSettings>();
		UCommonLegacyMovementSettings* CurrentCommonSettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();

		if (CurrentCommonSettings && OriginalCommonSettings)
		{
			// 커스텀 설정에서 WalkSpeed로 복원, 없으면 CDO 사용
			CurrentCommonSettings->MaxSpeed = OriginalCommonSettings->MaxSpeed;
			CurrentCommonSettings->Acceleration = OriginalCommonSettings->Acceleration;
		}
	}
}

bool FLNPSprintModifier::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	if (bExactMatch)
	{
		return TagToFind.MatchesTagExact(LNPTAG_Mover_IsSprinting);
	}

	return TagToFind.MatchesTag(LNPTAG_Mover_IsSprinting);
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
