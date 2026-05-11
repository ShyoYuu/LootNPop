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

	/** Gate pawn spawn until server init is complete */
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** Called via RPC from client when its local baking is done */
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
