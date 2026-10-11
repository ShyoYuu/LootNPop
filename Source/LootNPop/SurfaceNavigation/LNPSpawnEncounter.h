// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "LNPSpawnEncounter.generated.h"

/** Pod 기준 표면 위 접근 집단. 실제 출현점은 베이크된 안전 후보에서 선택한다. */
USTRUCT()
struct FLNPSpawnEnemyFormation
{
	GENERATED_BODY()

	/** 전역 MassSpawnConfig에 등록된 EnemyEntityConfig의 전체 오브젝트 경로. */
	UPROPERTY(EditAnywhere, Category = "LNP|Encounter")
	FName EnemyConfigPath;

	UPROPERTY(EditAnywhere, Category = "LNP|Encounter", meta = (ClampMin = "1"))
	int32 Count = 1;

	/** Pod의 로컬 X/Y 방향으로 집단 중심까지의 거리(cm). */
	UPROPERTY(EditAnywhere, Category = "LNP|Encounter")
	FVector2D Offset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "LNP|Encounter", meta = (ClampMin = "1"))
	float Radius = 600.0f;
};
