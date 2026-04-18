// Fill out your copyright notice in the Description page of Project Settings.


#include "Movement/LNPAsyncWalkingMode.h"
#include "Movement/LNPCharacterMovementSettings.h"

ULNPAsyncWalkingMode::ULNPAsyncWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SharedSettingsClasses.Add(ULNPCharacterMovementSettings::StaticClass());
}