// Fill out your copyright notice in the Description page of Project Settings.

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
};
