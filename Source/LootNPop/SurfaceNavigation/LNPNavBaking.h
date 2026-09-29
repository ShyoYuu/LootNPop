// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPNavData.h"
#include "SurfaceNavigation/LNPSupportAtlas.h"

/** Phase 7a coarse Nav bake 설정. 모든 값은 editor BakeSettingsHash에 들어간다. */
struct LOOTNPOP_API FLNPNavBakeSettings
{
	double CrustSpacing = 200.0;
	double LayerSpacing = 100.0;
	/** 서로 다른 Layer의 coarse grid 경계가 최대 두 지각 cell 떨어져도 연속 sweep으로 검증할 탐색 거리. */
	double PortalSearchDistance = 800.0;
	double ClearanceClassStep = 25.0;
	FLNPNavAgentProfile Agent;
};

struct LOOTNPOP_API FLNPNavBakeReport
{
	int32 LayerCount = 0;
	int32 TileCount = 0;
	int32 CellCount = 0;
	int32 StaticComponentCount = 0;
	int32 PortalCount = 0;
	int32 PortalDistanceCandidateCount = 0;
	int32 PortalStepCandidateCount = 0;
	int32 PortalClearanceCandidateCount = 0;
	int32 SeamEndpointCount = 0;
};

/** node 발 위치에 bake agent 캡슐을 놓을 수 있는지 판정한다. */
using FLNPNavNodeClearance = TFunctionRef<bool(
	uint16 LocalNavLayerId, const FVector3d& Position, const FVector3f& Normal)>;

/** 두 node의 agent capsule center를 연속 sweep할 수 있는지 판정한다. Layer가 다르면 정적 portal 후보 검사다. */
using FLNPNavEdgeClearance = TFunctionRef<bool(
	uint16 FromLayer, const FVector3d& FromPosition, const FVector3f& FromNormal,
	uint16 ToLayer, const FVector3d& ToPosition, const FVector3f& ToNormal)>;

/**
 * decoded Support Atlas와 결정론적 clearance oracle에서 Navigation/Traversal read model을 만든다.
 * UObject와 World를 참조하지 않으며 editor baker와 runtime automation이 공유한다(D-034).
 */
namespace LNPNavBaking
{
	LOOTNPOP_API bool Build(
		const FLNPSupportAtlas& Support,
		const FLNPNavBakeSettings& Settings,
		FLNPNavNodeClearance HasNodeClearance,
		FLNPNavEdgeClearance HasEdgeClearance,
		FLNPNavData& OutNavigation,
		FLNPNavTraversalData& OutTraversal,
		FLNPNavBakeReport& OutReport,
		FString& OutError);
}
