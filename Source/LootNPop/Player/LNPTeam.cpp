// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Player/LNPTeam.h"
#include "Player/LNPPlayerState.h"
#include "Character/LNPPlayerCharacter.h"

#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

uint8 LNPTeam::GetTeam(const AActor* Actor)
{
	const ALNPPlayerCharacter* PlayerPawn = Cast<ALNPPlayerCharacter>(Actor);
	if (PlayerPawn == nullptr)
		return NPCTeam;

	const ALNPPlayerState* PS = PlayerPawn->GetPlayerState<ALNPPlayerState>();
	return PS ? PS->GetTeamId() : DefaultPlayerTeam;
}

uint8 LNPTeam::GetLocalViewerTeam(const UWorld* World)
{
	// GetFirstPlayerController는 서버에서 원격 플레이어의 컨트롤러를 돌려줄 수 있다 — 로컬 플레이어로 고른다.
	const APlayerController* PC = (GEngine && World) ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
	const ALNPPlayerState*   PS = PC ? PC->GetPlayerState<ALNPPlayerState>() : nullptr;
	return PS ? PS->GetTeamId() : DefaultPlayerTeam;
}
