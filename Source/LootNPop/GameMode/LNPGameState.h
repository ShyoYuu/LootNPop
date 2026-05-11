// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LNPGameState.generated.h"

UENUM(BlueprintType)
enum class ELNPInitPhase : uint8
{
	WorldGeneration  UMETA(DisplayName = "World Generation"),
	SurfaceBaking    UMETA(DisplayName = "Surface Baking"),
	EntitySpawning   UMETA(DisplayName = "Entity Spawning"),
	Complete         UMETA(DisplayName = "Complete"),
};

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

	/** Seed for deterministic world generation. Set by GameMode at startup; 0 means random (resolved server-side). */
	UPROPERTY(Config, EditAnywhere, ReplicatedUsing=OnRep_OctantGenSeed, Category = "LNP|World Settings")
	int32 OctantGenSeed = 0;

	/** Global seed offset for PCG generation consistency */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "LNP|PCG")
	int32 PCGSeedOffset = 0;

	/** Server-authoritative initialization phase, replicated so clients can react */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ServerPhase, Category = "LNP|Init")
	ELNPInitPhase ServerPhase = ELNPInitPhase::WorldGeneration;

	// Replication setup
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_OctantGenSeed();

	UFUNCTION()
	void OnRep_ServerPhase();

	/** Fired on client when its own octant loading finishes. Triggers baking if server phase already advanced. */
	UFUNCTION()
	void OnClientWorldGenerationFinished();

private:
	void TryBeginClientBaking();
};
