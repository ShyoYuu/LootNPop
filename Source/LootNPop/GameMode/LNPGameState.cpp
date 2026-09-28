// Copyright (c) 2026 LootNPop. All rights reserved.

#include "LNPGameState.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
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

	if (ServerPhase >= ELNPInitPhase::SurfaceBaking)
	{
		// 조건 1 충족. 중간 참여에서 SurfaceBaking을 건너뛴 복제값을 받아도 로드를 놓치지 않는다.
		TryBeginClientSurfaceDataLoading();
	}
}

void ALNPGameState::OnClientWorldGenerationFinished()
{
	// 조건 2 충족. 서버가 이미 SurfaceBaking으로 진행된 경우에만 snapshot 로드·게시.
	if (ServerPhase >= ELNPInitPhase::SurfaceBaking)
	{
		TryBeginClientSurfaceDataLoading();
	}
}

void ALNPGameState::TryBeginClientSurfaceDataLoading()
{
	ULNPOctantSpawnSubsystem* OctantSub = GetWorld()->GetSubsystem<ULNPOctantSpawnSubsystem>();
	if (!OctantSub || !OctantSub->bGenerationComplete)
		return;

	if (ULNPSurfaceDataSubsystem* SurfaceData = GetWorld()->GetSubsystem<ULNPSurfaceDataSubsystem>())
	{
		SurfaceData->BeginLoading(); // no-op if already loading, ready, or failed
	}
}
