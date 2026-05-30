// Fill out your copyright notice in the Description page of Project Settings.

#include "LNPGameState.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "LootNPop.h"

#include "Net/UnrealNetwork.h"

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
			// 클라이언트 측 로딩 완료 시 알 수 있도록 구독
			OctantSub->OnWorldGenerationFinished.AddDynamic(this, &ALNPGameState::OnClientWorldGenerationFinished);
			OctantSub->StartWorldGeneration();
		}
	}
}

void ALNPGameState::OnRep_ServerPhase()
{
	UE_LOG(LogLootNPop, Log, TEXT("LNPGameState: Server phase changed to %d"), (int32)ServerPhase);

	if (ServerPhase == ELNPInitPhase::SurfaceBaking)
	{
		// 조건 1 충족. 클라이언트 Octant도 준비된 경우에만 베이킹.
		TryBeginClientBaking();
	}
}

void ALNPGameState::OnClientWorldGenerationFinished()
{
	// 조건 2 충족. 서버가 이미 SurfaceBaking으로 진행된 경우에만 베이킹.
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
