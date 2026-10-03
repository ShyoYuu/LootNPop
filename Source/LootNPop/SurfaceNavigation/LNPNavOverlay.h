// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/BitArray.h"
#include "SurfaceNavigation/LNPSurfaceTypes.h"
#include "SurfaceNavigation/LNPNavData.h"
#include "DynamicTerrain/LNPPlacementTypes.h"

struct FLNPSurfaceDataSnapshot;

/** 서버가 소유하는 정지 Pod blocker 하나. */
struct LOOTNPOP_API FLNPNavPodBlocker
{
	int32 PodID = 0;
	FVector3d Location = FVector3d::ZeroVector;
	FLNPSurfaceHandle Surface;
};

/** 활성 마커 하나의 Nav 점유. 외부 주소는 generation이 포함된 node ref로만 받는다. */
struct LOOTNPOP_API FLNPNavMarkerBlocker
{
	FLNPPlacementId Id;
	TArray<FLNPNavNodeRef> Nodes;
};

/**
 * Nav runtime overlay의 revision view(`phases/Phase07b_PathExecution.md` §3.6). immutable이며 바뀔 때마다 새 객체로 교체한다.
 * 전역 Revision은 다중 프레임 요청의 Stale 판정에, Tile revision은 경로·cache의 TraversedRevisionFingerprint 대조에 쓴다.
 */
struct LOOTNPOP_API FLNPNavOverlay
{
	uint32 Revision = 0;
	/** `LNPNavGraph::GetTileKey` → revision. 한 번도 바뀌지 않은 Tile은 없고 0으로 본다. */
	TMap<uint32, uint32> TileRevisions;
	/** 조밀 전역 node index 기준 차단 mask. */
	TBitArray<> BlockedNodes;

	bool IsBlocked(const int32 Node) const
	{
		return BlockedNodes.IsValidIndex(Node) && BlockedNodes[Node];
	}

	uint32 GetTileRevision(const uint32 TileKey) const
	{
		const uint32* Found = TileRevisions.Find(TileKey);
		return Found ? *Found : 0;
	}
};

namespace LNPNavOverlay
{
	/** 활성 Pod와 마커 점유를 합성한다. 겹친 source 하나가 사라져도 다른 source의 점유는 유지한다. */
	// true는 차단 상태 변경이다. false는 동일 상태 또는 잘못된 마커 주소이며, 잘못된 주소는 OutOverlay를 수정하지 않는다.
	LOOTNPOP_API bool BuildOverlay(const FLNPSurfaceDataSnapshot& Snapshot,
		TConstArrayView<FLNPNavPodBlocker> Pods, TConstArrayView<FLNPNavMarkerBlocker> Markers,
		const FLNPNavOverlay* Previous, FLNPNavOverlay& OutOverlay);
	/** Pod 차단 mask를 다시 만들고 차단 상태가 바뀐 Tile의 revision만 올린다. */
	LOOTNPOP_API bool BuildPodOverlay(const FLNPSurfaceDataSnapshot& Snapshot,
		TConstArrayView<FLNPNavPodBlocker> Pods, const FLNPNavOverlay* Previous, FLNPNavOverlay& OutOverlay);
	/** overlay가 아직 없으면 revision 0이다. */
	inline uint32 GetRevision(const FLNPNavOverlay* Overlay) { return Overlay ? Overlay->Revision : 0; }
	inline bool IsBlocked(const FLNPNavOverlay* Overlay, const int32 Node)
	{
		return Overlay && Overlay->IsBlocked(Node);
	}
	inline uint32 GetTileRevision(const FLNPNavOverlay* Overlay, const uint32 TileKey)
	{
		return Overlay ? Overlay->GetTileRevision(TileKey) : 0;
	}
}
