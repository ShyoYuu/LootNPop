// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPDashCooldownModifier.h"

FLNPDashCooldownModifier::FLNPDashCooldownModifier()
{
	// 실제 쿨다운 길이는 ULNPCharacterMoverComponent::ExecuteDash가 큐잉 직전에 덮어쓴다.
	DurationMs = 0.0f;
}

FMovementModifierBase* FLNPDashCooldownModifier::Clone() const
{
	return new FLNPDashCooldownModifier(*this);
}

void FLNPDashCooldownModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
}

UScriptStruct* FLNPDashCooldownModifier::GetScriptStruct() const
{
	return FLNPDashCooldownModifier::StaticStruct();
}

FString FLNPDashCooldownModifier::ToSimpleString() const
{
	return TEXT("LNP Dash Cooldown Modifier");
}
