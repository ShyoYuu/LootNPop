// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPSupportLayers.h"

/** snapshot generation 안에서 Support Layer를 가리키는 안정 handle. */
struct FLNPSurfaceHandle
{
	uint16 OctantSlot = MAX_uint16;
	uint16 LocalLayerId = LNPSupportLayers::NoLayer;
	uint64 Generation = 0;

	bool IsValid() const
	{
		return OctantSlot != MAX_uint16 && LocalLayerId != LNPSupportLayers::NoLayer && Generation != 0;
	}

	bool operator==(const FLNPSurfaceHandle&) const = default;
};
