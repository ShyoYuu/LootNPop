// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPGuardModifier.h"
#include "Movement/LNPCharacterMoverComponent.h"
#include "Movement/LNPCharacterMovementSettings.h"

#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/MovementUtils.h"
#include "GameFramework/Pawn.h"
#include "LootNPop.h"

namespace
{
	// 1P/2P 구분용 디버그 태그. 임시 진단 로그 전용 — 이슈 해결 후 제거 예정.
	FString DebugOwnerTag(const UMoverComponent* MoverComp)
	{
		const AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
		const APawn*  Pawn  = Cast<APawn>(Owner);
		return Owner
			? FString::Printf(TEXT("%s Auth=%d Local=%d"), *Owner->GetName(), Owner->HasAuthority(), Pawn && Pawn->IsLocallyControlled())
			: TEXT("?");
	}
}

FLNPGuardModifier::FLNPGuardModifier()
{
	DurationMs = -1.0f; // 취소될 때까지 무한 지속
}

void FLNPGuardModifier::OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	ULNPCharacterMovementSettings* LNPSettings    = MoverComp->FindSharedSettings_Mutable<ULNPCharacterMovementSettings>();
	UCommonLegacyMovementSettings* CommonSettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();

	UE_LOG(LogLootNPop, Log, TEXT("[GuardDebug] Modifier::OnStart [%s]: MaxSpeed %.1f -> %.1f (LNPSettings=%d CommonSettings=%d)"),
		*DebugOwnerTag(MoverComp),
		CommonSettings ? CommonSettings->MaxSpeed : -1.f,
		LNPSettings ? LNPSettings->GuardWalkSpeed : -1.f,
		LNPSettings != nullptr, CommonSettings != nullptr);

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

		UE_LOG(LogLootNPop, Log, TEXT("[GuardDebug] Modifier::OnEnd [%s]: MaxSpeed %.1f -> restoring to %.1f"),
			*DebugOwnerTag(MoverComp),
			CurrentCommonSettings ? CurrentCommonSettings->MaxSpeed : -1.f,
			OriginalCommonSettings ? OriginalCommonSettings->MaxSpeed : -1.f);

		if (CurrentCommonSettings && OriginalCommonSettings)
		{
			CurrentCommonSettings->MaxSpeed     = OriginalCommonSettings->MaxSpeed;
			CurrentCommonSettings->Acceleration = OriginalCommonSettings->Acceleration;
		}
	}
	else
	{
		UE_LOG(LogLootNPop, Warning, TEXT("[GuardDebug] Modifier::OnEnd [%s]: GetOriginalComponentType returned null — could not restore MaxSpeed"), *DebugOwnerTag(MoverComp));
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
