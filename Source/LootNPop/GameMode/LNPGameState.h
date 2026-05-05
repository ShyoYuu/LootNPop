// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LNPGameState.generated.h"

/**
 * 
 */
UCLASS()
class LOOTNPOP_API ALNPGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ALNPGameState();

	/** Global flag to toggle Sphere Gravity mode for all players */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "LNP|World Settings")
	bool bIsSphereWorld = false;

	/** Default seed for world generation. 0 means random. */
	UPROPERTY(Config, EditAnywhere, Category = "LNP|World Settings")
	int32 OctantGenSeed = 0;

	/** Global seed offset for PCG generation consistency */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "LNP|PCG")
	int32 PCGSeedOffset = 0;

	// Replication setup
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
