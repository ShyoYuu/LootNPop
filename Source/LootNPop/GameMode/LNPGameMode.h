// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LNPGameMode.generated.h"

class ALNPPlayerController;

UCLASS()
class LOOTNPOP_API ALNPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALNPGameMode();

	virtual void BeginPlay() override;

	/** 서버 초기화 완료 전까지 Pawn 스폰을 차단한다 */
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** 클라이언트의 로컬 베이킹이 완료되면 RPC를 통해 호출된다 */
	void OnClientReady(ALNPPlayerController* PC);

private:
	UFUNCTION()
	void OnWorldGenerationComplete();

	UFUNCTION()
	void OnSurfaceBakingComplete();

	UFUNCTION()
	void OnEntitySpawningComplete();

	bool bServerInitComplete = false;
	TArray<TWeakObjectPtr<AController>> PendingPlayers;
	/** ServerNotifyClientReady() RPC를 수신한 클라이언트 집합 — 폰 스폰의 두 번째 게이트 */
	TSet<TWeakObjectPtr<ALNPPlayerController>> ReadyClients;
};
