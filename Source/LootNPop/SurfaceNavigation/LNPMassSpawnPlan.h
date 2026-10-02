// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

enum class ELNPPodPlacementSource : uint8
{
	AuthoredForSet,
	AuthoredGeneric,
	RandomCandidate,
};

struct FLNPMassSpawnEnemyPlanInput
{
	int32 RequestedCount = 0;
	int32 AssetIndex = INDEX_NONE;
	float RequiredHeadroom = 0.0f;
};

struct FLNPMassSpawnSetPlanInput
{
	FName SpawnSetId;
	int32 RequestedPods = 0;
	int32 PodAssetIndex = INDEX_NONE;
	TArray<FLNPMassSpawnEnemyPlanInput> Enemies;
	/** 편성된 비행 적의 필요 headroom 최댓값. 비행 없는 세트는 0이다. */
	float RequiredHeadroom = 0.0f;
};

struct FLNPMassSpawnPlannedEnemyGroup
{
	int32 AssetIndex = INDEX_NONE;
	TArray<FTransform> Transforms;
};

struct FLNPMassSpawnPlannedPod
{
	int32 SetInputIndex = INDEX_NONE;
	int32 PodAssetIndex = INDEX_NONE;
	FTransform Transform = FTransform::Identity;
	FLNPSurfaceHandle Surface;
	ELNPPodPlacementSource PlacementSource = ELNPPodPlacementSource::RandomCandidate;
	TArray<FLNPMassSpawnPlannedEnemyGroup> Enemies;
};

struct FLNPMassSpawnSetStats
{
	FName SpawnSetId;
	int32 Requested = 0;
	int32 Authored = 0;
	int32 Generic = 0;
	int32 Random = 0;
	int32 Placed = 0;
	int32 Shortfall = 0;
	int32 UnusedAuthored = 0;
	int32 EnemyRequested = 0;
	int32 EnemyPlaced = 0;
};

struct FLNPMassSpawnPlan
{
	TArray<FLNPMassSpawnPlannedPod> Pods;
	TArray<FLNPMassSpawnSetStats> SetStats;
	int32 UnusedGenericAnchors = 0;
};

namespace LNPMassSpawnPlanning
{
	/** 8-slot Spawn snapshot을 world 후보로 조립하고 config 총량만큼 결정론적으로 할당한다. */
	LOOTNPOP_API bool BuildPlan(
		const FLNPSurfaceDataSnapshot& Snapshot,
		TConstArrayView<FLNPMassSpawnSetPlanInput> Sets,
		int32 WorldSeed,
		float MinDistanceBetweenPods,
		float EnemySpawnRadiusAroundPod,
		FLNPMassSpawnPlan& OutPlan,
		FString& OutError);
}
