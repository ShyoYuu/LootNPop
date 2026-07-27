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
		// MaxSpeed는 여기서 쓰지 않는다 — FLNPMoveSpeedModifier가 매 틱 CDO 기준으로
		// (IsSprinting이면 SprintSpeed) × MoveSpeed 버프를 계산해 소유한다.
		// 여기서 대입해도 그 틱에 곧바로 덮어써지므로 혼란만 남는다.
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
			// MaxSpeed는 복원하지 않는다 — OnStart에서 건드리지 않으므로 되돌릴 것이 없다.
			// (여기서 CDO 값을 복원하면 FLNPMoveSpeedModifier가 적용한 버프를 한 틱 지워 깜빡임이 생긴다.)
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

void FLNPSprintModifier::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	InOutTags.AddTag(LNP_Mover_IsSprinting);
}

FMovementModifierBase* FLNPSprintModifier::Clone() const
{
	FLNPSprintModifier* CopyPtr = new FLNPSprintModifier(*this);
	return CopyPtr;
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
