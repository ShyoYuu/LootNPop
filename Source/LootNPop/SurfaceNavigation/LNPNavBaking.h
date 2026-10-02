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
	/** 같은 component 쌍의 portal 중점 사이 최소 3D 거리(cm). 비지각 Nav 두 칸을 기본으로 둔다. */
	double PortalMinSpacing = 200.0;
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
	int32 PortalSpacingRejectCount = 0;
	double PortalBakeSeconds = 0.0;
	int32 SeamEndpointCount = 0;
	/**
	 * walkable 지각 seam 좌표 중 capsule clearance로 탈락한 node 수. 이웃 slot 사본과 비대칭이 되어
	 * runtime에서 막힘으로 합쳐진다(D-060). 이음매 근처 정적 배치 금지 규칙의 콘텐츠 경고 지표다.
	 */
	int32 SeamClearanceRejectCount = 0;
};

/** node 발 위치에 bake agent 캡슐을 놓을 수 있는지 판정한다. */
using FLNPNavNodeClearance = TFunctionRef<bool(
	uint16 LocalNavLayerId, const FVector3d& Position, const FVector3f& Normal)>;

/**
 * 두 node 사이를 걸어서 이동할 수 있는지 판정한다. 사이 지형의 step·경사와 agent capsule 연속 sweep을 모두 본다.
 * Layer가 다르면 정적 portal 후보 검사다.
 */
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
