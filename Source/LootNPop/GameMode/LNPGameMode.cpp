// Fill out your copyright notice in the Description page of Project Settings.

#include "GameMode/LNPGameMode.h"
#include "GameMode/LNPGameState.h"
#include "Player/LNPPlayerState.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "GameLogic/LNPMassSpawnSubsystem.h"
#include "Player/LNPPlayerController.h"
#include "LootNPop.h"

ALNPGameMode::ALNPGameMode()
{
	GameStateClass = ALNPGameState::StaticClass();
	PlayerControllerClass = ALNPPlayerController::StaticClass();
	PlayerStateClass = ALNPPlayerState::StaticClass();
}

void ALNPGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	check(World);

	ALNPGameState* GS = World->GetGameState<ALNPGameState>();
	check(GS);

	// Seed 결정: config로 설정되지 않았으면 지금 생성하여 모든 클라이언트가 동일한 값을 사용
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

	UE_LOG(LogLootNPop, Log, TEXT("ALNPGameMode: Server init complete. Spawning %d pending players."), PendingPlayers.Num());

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
	// 향후 사용 예약: 예) 게임 시작 전 모든 클라이언트 대기
}
