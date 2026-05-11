// Fill out your copyright notice in the Description page of Project Settings.

#include "GameMode/LNPGameMode.h"
#include "GameMode/LNPGameState.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "GameLogic/LNPMassSpawnSubsystem.h"
#include "Player/LNPPlayerController.h"

ALNPGameMode::ALNPGameMode()
{
	GameStateClass = ALNPGameState::StaticClass();
	PlayerControllerClass = ALNPPlayerController::StaticClass();
}

void ALNPGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	check(World);

	ALNPGameState* GS = World->GetGameState<ALNPGameState>();
	check(GS);

	// Resolve seed: if not set via config, generate one now so all clients get the same value
	if (GS->OctantGenSeed == 0)
	{
		GS->OctantGenSeed = FMath::Rand();
	}

	GS->ServerPhase = ELNPInitPhase::WorldGeneration;

	if (ULNPOctantSpawnSubsystem* OctantSub = World->GetSubsystem<ULNPOctantSpawnSubsystem>())
	{
		OctantSub->OnWorldGenerationFinished.AddDynamic(this, &ALNPGameMode::OnWorldGenerationComplete);
		OctantSub->StartWorldGeneration();
	}
}

void ALNPGameMode::OnWorldGenerationComplete()
{
	UWorld* World = GetWorld();
	ALNPGameState* GS = World->GetGameState<ALNPGameState>();
	GS->ServerPhase = ELNPInitPhase::SurfaceBaking;

	if (ULNPSurfaceCacheSubsystem* SurfaceSub = World->GetSubsystem<ULNPSurfaceCacheSubsystem>())
	{
		SurfaceSub->OnBakingComplete.AddDynamic(this, &ALNPGameMode::OnSurfaceBakingComplete);
		SurfaceSub->BeginBaking();
	}
}

void ALNPGameMode::OnSurfaceBakingComplete()
{
	UWorld* World = GetWorld();
	ALNPGameState* GS = World->GetGameState<ALNPGameState>();
	GS->ServerPhase = ELNPInitPhase::EntitySpawning;

	if (ULNPMassSpawnSubsystem* SpawnSub = World->GetSubsystem<ULNPMassSpawnSubsystem>())
	{
		SpawnSub->OnSpawningComplete.AddDynamic(this, &ALNPGameMode::OnEntitySpawningComplete);
		SpawnSub->BeginSpawning();
	}
}

void ALNPGameMode::OnEntitySpawningComplete()
{
	UWorld* World = GetWorld();
	ALNPGameState* GS = World->GetGameState<ALNPGameState>();
	GS->ServerPhase = ELNPInitPhase::Complete;
	bServerInitComplete = true;

	UE_LOG(LogTemp, Log, TEXT("ALNPGameMode: Server init complete. Spawning %d pending players."), PendingPlayers.Num());

	for (TWeakObjectPtr<AController>& Ctrl : PendingPlayers)
	{
		if (Ctrl.IsValid())
		{
			Super::RestartPlayer(Ctrl.Get());
		}
	}
	PendingPlayers.Empty();
}

void ALNPGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!bServerInitComplete)
	{
		PendingPlayers.AddUnique(NewPlayer);
		return;
	}
	Super::RestartPlayer(NewPlayer);
}

void ALNPGameMode::OnClientReady(ALNPPlayerController* PC)
{
	// Hook for future use: e.g. wait for all clients before game start
}
