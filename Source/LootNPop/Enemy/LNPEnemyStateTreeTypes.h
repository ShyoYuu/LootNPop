// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "MassEntityHandle.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "LNPEnemyStateTreeTypes.generated.h"

/** Enemy 상태 평가용 통합 instance data */
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
 * 모든 LNP Mass StateTree 태스크에 필요한 instance data.
 *
 * Mass StateTree 프레임워크는 GetInstanceDataType()이 null이 아닌 UScriptStruct를 반환하도록 요구한다.
 * nullptr을 반환하면 StateTree 에디터에서 "Malformed task, missing instance value" 컴파일 오류가 발생한다.
 * 이 구조체를 삭제하거나 GetInstanceDataType()이 nullptr을 반환하도록 변경하지 말 것.
 */
USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPEnemyTaskInstanceData
{
	GENERATED_BODY()
};

