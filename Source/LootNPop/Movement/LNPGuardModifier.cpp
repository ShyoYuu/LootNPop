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
		// MaxSpeed는 여기서 쓰지 않는다 — FLNPMoveSpeedModifier가 매 틱 CDO 기준으로
		// (IsGuarding이면 GuardWalkSpeed) × MoveSpeed 버프를 계산해 소유한다.
		CommonSettings->Acceleration = LNPSettings->GuardAcceleration;
	}
}

void FLNPGuardModifier::OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	// CDO에서 원래 공통 설정 값을 조회해 복원
	if (const UMoverComponent* CDOMoverComp = UMovementUtils::GetOriginalComponentType<UMoverComponent>(MoverComp->GetOwner()))
	{
		const UCommonLegacyMovementSettings* OriginalCommonSettings = CDOMoverComp->FindSharedSettings<UCommonLegacyMovementSettings>();
		UCommonLegacyMovementSettings*       CurrentCommonSettings  = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();

		if (CurrentCommonSettings && OriginalCommonSettings)
		{
			// MaxSpeed는 복원하지 않는다 — OnStart에서 건드리지 않으므로 되돌릴 것이 없다.
			CurrentCommonSettings->Acceleration = OriginalCommonSettings->Acceleration;
		}
	}
}

bool FLNPGuardModifier::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	if (bExactMatch)
		return TagToFind.MatchesTagExact(LNP_Mover_IsGuarding);

	return TagToFind.MatchesTag(LNP_Mover_IsGuarding);
}

void FLNPGuardModifier::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	InOutTags.AddTag(LNP_Mover_IsGuarding);
}

FMovementModifierBase* FLNPGuardModifier::Clone() const
{
	FLNPGuardModifier* CopyPtr = new FLNPGuardModifier(*this);
	return CopyPtr;
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
