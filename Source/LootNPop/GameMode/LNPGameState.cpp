// Fill out your copyright notice in the Description page of Project Settings.

#include "LNPGameState.h"
#include "Net/UnrealNetwork.h"

ALNPGameState::ALNPGameState()
{
	bIsSphereWorld = false;
	OctantGenSeed = 0;
	PCGSeedOffset = 0;
}

void ALNPGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALNPGameState, bIsSphereWorld);
	DOREPLIFETIME(ALNPGameState, OctantGenSeed);
	DOREPLIFETIME(ALNPGameState, PCGSeedOffset);
}
