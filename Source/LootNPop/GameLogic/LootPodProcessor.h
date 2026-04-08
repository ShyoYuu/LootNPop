// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LootPodProcessor.generated.h"

/**
 * 1. Idle -> Looting State Transition Processor
 */
UCLASS()
class LOOTNPOP_API ULNPIdleToLootingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPIdleToLootingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
	FMassEntityQuery PlayerQuery;
};

/**
 * 2. Unified Looting Processor (Handles Gauge Update and State Transitions: Looting -> Popped/Idle)
 */
UCLASS()
class LOOTNPOP_API ULNPLootingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPLootingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
	FMassEntityQuery PlayerQuery;
};
