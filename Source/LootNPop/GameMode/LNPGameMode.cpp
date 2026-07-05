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

	// 클라이언트 준비까지 완료된 플레이어만 즉시 스폰. 미완료 플레이어는 OnClientReady에서 처리.
	TArray<TWeakObjectPtr<AController>> StillPending;
	int32 SpawnedCount = 0;
	for (TWeakObjectPtr<AController>& Ctrl : PendingPlayers)
	{
		if (!Ctrl.IsValid()) continue;
		ALNPPlayerController* PC = Cast<ALNPPlayerController>(Ctrl.Get());
		if (!PC || ReadyClients.Contains(PC))
		{
			Super::RestartPlayer(Ctrl.Get());
			++SpawnedCount;
		}
		else
		{
			StillPending.Add(Ctrl);
		}
	}
	PendingPlayers = MoveTemp(StillPending);

	UE_LOG(LogLootNPop, Log, TEXT("ALNPGameMode: Server init complete. Spawned %d players, %d awaiting client ready."),
		SpawnedCount, PendingPlayers.Num());
}

void ALNPGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!bServerInitComplete)
	{
		PendingPlayers.AddUnique(NewPlayer);
		return;
	}
	// 서버 초기화 완료 후에도 해당 클라이언트가 로컬 베이킹을 끝낼 때까지 대기
	ALNPPlayerController* PC = Cast<ALNPPlayerController>(NewPlayer);
	if (PC && !ReadyClients.Contains(PC))
	{
		PendingPlayers.AddUnique(NewPlayer);
		return;
	}
	Super::RestartPlayer(NewPlayer);
}

void ALNPGameMode::OnClientReady(ALNPPlayerController* PC)
{
	ReadyClients.Add(PC);

	if (bServerInitComplete)
	{
		// 서버 완료 후 뒤늦게 준비된 클라이언트 (중간 참여 포함) — 지금 스폰
		PendingPlayers.RemoveAll([PC](const TWeakObjectPtr<AController>& Weak)
		{
			return Weak.Get() == PC;
		});
		Super::RestartPlayer(PC);
	}
}
