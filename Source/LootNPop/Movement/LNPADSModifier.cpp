// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPADSModifier.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPCharacterMovementSettings.h"

#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/MovementUtils.h"

FLNPADSModifier::FLNPADSModifier()
{
	DurationMs = -1.0f; // 취소될 때까지 무한 지속
}

void FLNPADSModifier::OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	ULNPCharacterMovementSettings* LNPSettings    = MoverComp->FindSharedSettings_Mutable<ULNPCharacterMovementSettings>();
	UCommonLegacyMovementSettings* CommonSettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();

	if (LNPSettings && CommonSettings)
	{
		// MaxSpeed는 여기서 쓰지 않는다 — FLNPMoveSpeedModifier가 매 틱 CDO 기준으로
		// (IsADS이면 ADSWalkSpeed) × MoveSpeed 버프를 계산해 소유한다. (Guard와 동일)
		CommonSettings->Acceleration = LNPSettings->ADSAcceleration;
	}
}

void FLNPADSModifier::OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	// CDO에서 원래 공통 설정 값을 조회해 복원
	if (const UMoverComponent* CDOMoverComp = UMovementUtils::GetOriginalComponentType<UMoverComponent>(MoverComp->GetOwner()))
	{
		const UCommonLegacyMovementSettings* OriginalCommonSettings = CDOMoverComp->FindSharedSettings<UCommonLegacyMovementSettings>();
		UCommonLegacyMovementSettings*       CurrentCommonSettings  = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();

		if (CurrentCommonSettings && OriginalCommonSettings)
		{
			CurrentCommonSettings->Acceleration = OriginalCommonSettings->Acceleration;
		}
	}
}

bool FLNPADSModifier::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	if (bExactMatch)
		return TagToFind.MatchesTagExact(LNP_Mover_IsADS);

	return TagToFind.MatchesTag(LNP_Mover_IsADS);
}

void FLNPADSModifier::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	InOutTags.AddTag(LNP_Mover_IsADS);
}

FMovementModifierBase* FLNPADSModifier::Clone() const
{
	FLNPADSModifier* CopyPtr = new FLNPADSModifier(*this);
	return CopyPtr;
}

void FLNPADSModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
}

UScriptStruct* FLNPADSModifier::GetScriptStruct() const
{
	return FLNPADSModifier::StaticStruct();
}

FString FLNPADSModifier::ToSimpleString() const
{
	return TEXT("LNP ADS Modifier");
}
