// Fill out your copyright notice in the Description page of Project Settings.

#include "LNPGameState.h"
#include "Net/UnrealNetwork.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"

ALNPGameState::ALNPGameState()
{
	bIsSphereWorld = false;
	OctantGenSeed = 0;
	PCGSeedOffset = 0;
	ServerPhase = ELNPInitPhase::WorldGeneration;
}

void ALNPGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALNPGameState, bIsSphereWorld);
	DOREPLIFETIME(ALNPGameState, OctantGenSeed);
	DOREPLIFETIME(ALNPGameState, PCGSeedOffset);
	DOREPLIFETIME(ALNPGameState, ServerPhase);
}

void ALNPGameState::OnRep_OctantGenSeed()
{
	if (ULNPOctantSpawnSubsystem* OctantSub = GetWorld()->GetSubsystem<ULNPOctantSpawnSubsystem>())
	{
		if (!OctantSub->bGenerationComplete && !OctantSub->IsTickable())
		{
			// Subscribe so we know when client-side loading finishes
			OctantSub->OnWorldGenerationFinished.AddDynamic(this, &ALNPGameState::OnClientWorldGenerationFinished);
			OctantSub->StartWorldGeneration();
		}
	}
}

void ALNPGameState::OnRep_ServerPhase()
{
	UE_LOG(LogTemp, Log, TEXT("LNPGameState: Server phase changed to %d"), (int32)ServerPhase);

	if (ServerPhase == ELNPInitPhase::SurfaceBaking)
	{
		// Gate 1 satisfied. Bake only if client octants are also ready.
		TryBeginClientBaking();
	}
}

void ALNPGameState::OnClientWorldGenerationFinished()
{
	// Gate 2 satisfied. Bake only if server has already advanced to SurfaceBaking.
	if (ServerPhase >= ELNPInitPhase::SurfaceBaking)
	{
		TryBeginClientBaking();
	}
}

void ALNPGameState::TryBeginClientBaking()
{
	ULNPOctantSpawnSubsystem* OctantSub = GetWorld()->GetSubsystem<ULNPOctantSpawnSubsystem>();
	if (!OctantSub || !OctantSub->bGenerationComplete)
		return;

	if (ULNPSurfaceCacheSubsystem* SurfaceSub = GetWorld()->GetSubsystem<ULNPSurfaceCacheSubsystem>())
	{
		SurfaceSub->BeginBaking(); // no-op if already baking or complete
	}
}
