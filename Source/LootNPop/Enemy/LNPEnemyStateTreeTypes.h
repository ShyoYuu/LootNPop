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

/**
 * Required instance data for all LNP Mass StateTree tasks.
 *
 * The Mass StateTree framework requires GetInstanceDataType() to return a non-null UScriptStruct.
 * Returning nullptr causes a "Malformed task, missing instance value" compile error in the StateTree editor.
 * Do NOT remove this struct or change GetInstanceDataType() to return nullptr.
 */
USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPEnemyTaskInstanceData
{
	GENERATED_BODY()
};

