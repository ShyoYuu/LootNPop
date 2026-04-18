// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LNPSphereWorldGenerator.generated.h"

/**
 * Generates a spherical world by assembling 8 octant pieces (Packed Level Blueprints).
 */
UCLASS()
class LOOTNPOP_API ALNPSphereWorldGenerator : public AActor
{
	GENERATED_BODY()
	
public:	
	ALNPSphereWorldGenerator();

protected:
	virtual void BeginPlay() override;

	/** Spawns 8 octants to form a complete sphere. */
	void GenerateWorld();

public:
	/** List of Octant Level assets (LVI) to randomly pick from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|World Generation")
	TObjectPtr<class ULNPOctantPoolData> OctantPoolData;

	/** Seed for deterministic random generation. If 0, uses a random seed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LNP|World Generation")
	int32 Seed = 0;

private:
	/** Static rotation values for each of the 8 octants to form a sphere. */
	static const FRotator OctantRotations[8];
};
