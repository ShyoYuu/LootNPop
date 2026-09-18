// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPGrappleFlightModifier.h"

FLNPGrappleFlightModifier::FLNPGrappleFlightModifier()
{
	// 실제 비행 시간은 ULNPCharacterMoverComponent::ExecuteGrapple이 큐잉 직전에 덮어쓴다.
	DurationMs = 0.0f;
}

FMovementModifierBase* FLNPGrappleFlightModifier::Clone() const
{
	return new FLNPGrappleFlightModifier(*this);
}

void FLNPGrappleFlightModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
}

UScriptStruct* FLNPGrappleFlightModifier::GetScriptStruct() const
{
	return FLNPGrappleFlightModifier::StaticStruct();
}

FString FLNPGrappleFlightModifier::ToSimpleString() const
{
	return TEXT("LNP Grapple Flight Modifier");
}
