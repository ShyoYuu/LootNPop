// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPSurfaceTypes.h"
#include "SurfaceNavigation/LNPNavOverlay.h"

/** 서버 구간 배회. 캡슐 중심을 저장하고 지상 보간점은 현재 Layer에 투영한다. */
struct FLNPEnemyWanderSegment
{
	FVector Start = FVector::ZeroVector;
	FVector Goal = FVector::ZeroVector;
	double StartTime = 0.0;
	double ArrivalTime = 0.0;
	double StopUntil = 0.0;
	uint64 Generation = 0;
	float LocalYaw = 0.f;
	struct FTile { uint32 Key = 0; uint32 Revision = 0; };
	FTile Tiles[16];
	uint8 TileCount = 0;
	bool bValid = false;

	FVector Evaluate(double Now) const
	{
		const double Alpha = ArrivalTime > StartTime
			? FMath::Clamp((Now - StartTime) / (ArrivalTime - StartTime), 0.0, 1.0) : 1.0;
		return FMath::Lerp(Start, Goal, Alpha);
	}

	bool IsCurrent(uint64 InGeneration, const FLNPNavOverlay* Overlay) const
	{
		if (!bValid || Generation != InGeneration)
			return false;
		for (uint8 I = 0; I < TileCount; ++I)
			if (LNPNavOverlay::GetTileRevision(Overlay, Tiles[I].Key) != Tiles[I].Revision)
				return false;
		return true;
	}
};
