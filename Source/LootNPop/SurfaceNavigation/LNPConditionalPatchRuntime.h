// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicTerrain/LNPPlacementTypes.h"

struct FLNPSurfaceDataSnapshot;

namespace LNPConditionalPatchRuntime
{
	/** 모든 patch 주소를 로드 때 고정한다. 입력은 아직 정적 payload인 snapshot이다. */
	LOOTNPOP_API bool Prepare(FLNPSurfaceDataSnapshot& Snapshot, FString& OutError);
	/** 비활성 기준 snapshot에서 활성 view를 합성한다. 실패는 출력을 수정하지 않는다. */
	LOOTNPOP_API bool BuildActiveSnapshot(const FLNPSurfaceDataSnapshot& Base,
		TConstArrayView<FLNPPlacementId> Active, const FLNPSurfaceDataSnapshot& Previous,
		FLNPSurfaceDataSnapshot& OutSnapshot, FString& OutError);
}
