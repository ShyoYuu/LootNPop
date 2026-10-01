// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Nav runtime overlay의 revision view(`phases/Phase07b_PathExecution.md` §3.6). immutable이며 바뀔 때마다 새 객체로 교체한다.
 * 전역 Revision은 다중 프레임 요청의 Stale 판정에, Tile revision은 경로·cache의 TraversedRevisionFingerprint 대조에 쓴다.
 */
struct LOOTNPOP_API FLNPNavOverlay
{
	uint32 Revision = 0;
	/** `LNPNavGraph::GetTileKey` → revision. 한 번도 바뀌지 않은 Tile은 없고 0으로 본다. */
	TMap<uint32, uint32> TileRevisions;

	uint32 GetTileRevision(const uint32 TileKey) const
	{
		const uint32* Found = TileRevisions.Find(TileKey);
		return Found ? *Found : 0;
	}
};

namespace LNPNavOverlay
{
	/** overlay가 아직 없으면 revision 0이다. */
	inline uint32 GetRevision(const FLNPNavOverlay* Overlay) { return Overlay ? Overlay->Revision : 0; }
	inline uint32 GetTileRevision(const FLNPNavOverlay* Overlay, const uint32 TileKey)
	{
		return Overlay ? Overlay->GetTileRevision(TileKey) : 0;
	}
}
