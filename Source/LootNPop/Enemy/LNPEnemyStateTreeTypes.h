// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "MassEntityHandle.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "LNPEnemyStateTreeTypes.generated.h"

/** Consolidated instance data for Enemy State Evaluation */
USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPEnemyStateEvaluatorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	ELNPTargetingState TargetingState = ELNPTargetingState::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	float DistanceToTarget = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	float AttackRange = 0.0f;
};

/** Shared data structure for Tasks (if needed for output/state) */
USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPEnemyTaskInstanceData
{
	GENERATED_BODY()

	/** Next update time to throttle logic if needed */
	UPROPERTY()
	double NextUpdate = 0.0;
};
