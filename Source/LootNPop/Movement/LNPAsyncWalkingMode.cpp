// Copyright (c) 2026 LootNPop. All rights reserved.


#include "Movement/LNPAsyncWalkingMode.h"
#include "Movement/LNPCharacterMovementSettings.h"

ULNPAsyncWalkingMode::ULNPAsyncWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SharedSettingsClasses.Add(ULNPCharacterMovementSettings::StaticClass());
}