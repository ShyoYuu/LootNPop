// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPNavData.h"
#include "SurfaceNavigation/LNPSupportAtlas.h"

/** 이음매 양쪽 slot에 같은 월드 위치로 존재하는 지각 node 사본 두 개. 7b 경로 탐색의 비용 0 전이다. */
struct LOOTNPOP_API FLNPNavSeamLink
{
	FLNPNavNodeRef A;
	FLNPNavNodeRef B;
};

/** 같은 slot 안의 두 인접 node 사이 edge. */
struct LOOTNPOP_API FLNPNavEdgeRef
{
	FLNPNavNodeRef From;
	FLNPNavNodeRef To;
};

/** 8-slot 조립 입력 하나. decoded payload는 같은 asset을 쓰는 slot끼리 공유된다. */
struct LOOTNPOP_API FLNPNavSlotInput
{
	TSharedPtr<const FLNPSupportAtlas, ESPMode::ThreadSafe> Support;
	TSharedPtr<const FLNPNavData, ESPMode::ThreadSafe> Navigation;
	TSharedPtr<const FLNPNavTraversalData, ESPMode::ThreadSafe> Traversal;
};

/**
 * 한 Surface snapshot generation의 immutable Nav·정적 연결성 view.
 * runtime Nav Layer ID는 `SlotLayerBase[Slot] + LocalNavLayerId`이고,
 * runtime StaticNavComponent는 portal과 12개 world seam까지 합친 뒤의 연결 요소다.
 */
struct LOOTNPOP_API FLNPNavSnapshot
{
	uint64 SnapshotGeneration = 0;
	/** slot별 runtime Nav Layer ID 시작값. 원소 9개이며 마지막 값이 전체 runtime Layer 수다. */
	TArray<uint16> SlotLayerBase;
	/** slot별 asset-local StaticNavComponent 시작 index. 원소 9개. */
	TArray<uint32> SlotComponentBase;
	/** `SlotComponentBase[Slot] + LocalStaticComponentId` → runtime StaticNavComponent ID. */
	TArray<uint32> RuntimeStaticComponentByLocal;
	uint32 RuntimeStaticComponentCount = 0;
	/** 12개 world seam의 node 사본 쌍. (A slot, B slot, seam step) 순서다. */
	TArray<FLNPNavSeamLink> SeamLinks;
	/**
	 * 한쪽 slot 사본에서만 통과한 seam 줄 node(통과한 사본)와 seam 방향 edge(열린 사본).
	 * 월드 충돌은 양쪽 지오메트리의 합집합이므로 둘 다 막힘으로 취급한다(D-060). 7b 경로 탐색이 제외한다.
	 */
	TArray<FLNPNavNodeRef> BlockedSeamNodes;
	TArray<FLNPNavEdgeRef> BlockedSeamEdges;
	/** BlockedSeamNodes의 `LNPNavRuntime::MakeNodeKey` 오름차순. 조회용이다. */
	TArray<uint64> BlockedSeamNodeKeys;
	/**
	 * runtime StaticNavComponent → ReachabilityGroup. active Traversal Link까지 union한 결과다.
	 * 7a에는 동적 link가 없으므로 1:1이다. link 상태가 실제로 바뀔 때만 ConnectivityGraphVersion이 증가한다.
	 */
	TArray<uint32> ReachabilityGroupByStaticComponent;
	uint32 ReachabilityGroupCount = 0;
	uint32 ConnectivityGraphVersion = 0;
	/** 게시 전 검증에서 측정한 seam 양쪽 지면 반지름·법선 차이. */
	double MaxSeamRadiusDelta = 0.0;
	double MinSeamNormalDot = 1.0;

	bool IsValid() const { return SnapshotGeneration != 0 && SlotLayerBase.Num() == 9; }
};

namespace LNPNavRuntime
{
	/** seam 양쪽 지각 반지름 허용 오차(cm). Support seam은 같은 양자화 샘플이므로 보간 오차만 허용한다. */
	constexpr double MaxSeamRadiusDelta = 1.0;

	/**
	 * slot 회전 표로 12개 world seam의 ordered endpoint를 맞추고 asset-local component를 union해 runtime view를 만든다.
	 * 한쪽 사본에만 있는 seam node·seam 방향 edge는 막힘으로 기록하고 연결하지 않는다(D-060).
	 * agent profile·해상도·지면 반지름·법선·clearance class 불일치는 전체 실패다.
	 */
	LOOTNPOP_API bool BuildSnapshot(
		TConstArrayView<FLNPNavSlotInput> Slots,
		TConstArrayView<FRotator> SlotRotations,
		uint64 SnapshotGeneration,
		FLNPNavSnapshot& OutSnapshot,
		FString& OutError);

	LOOTNPOP_API bool MakeRuntimeNodeRef(
		const FLNPNavSnapshot& Snapshot, int32 Slot, const FLNPLocalNavNodeRef& Local, FLNPNavNodeRef& OutNode);

	/** generation이 다르거나 범위를 벗어난 ref는 false다. */
	LOOTNPOP_API bool ResolveRuntimeNodeRef(
		const FLNPNavSnapshot& Snapshot, const FLNPNavNodeRef& Node, int32& OutSlot, FLNPLocalNavNodeRef& OutLocal);

	/** 범위를 벗어나면 MAX_uint32다. */
	LOOTNPOP_API uint32 GetRuntimeStaticComponent(
		const FLNPNavSnapshot& Snapshot, int32 Slot, uint16 LocalStaticComponentId);

	/** generation을 뺀 runtime node 주소의 정렬 가능한 key. */
	inline uint64 MakeNodeKey(const FLNPNavNodeRef& Node)
	{
		return (static_cast<uint64>(Node.RuntimeNavLayerId) << 32)
			| (static_cast<uint64>(Node.TileId) << 16) | Node.LocalCellIndex;
	}

	/** D-060으로 막힌 이음매 node 사본인지 본다. generation이 다른 ref는 false다. */
	LOOTNPOP_API bool IsBlockedSeamNode(const FLNPNavSnapshot& Snapshot, const FLNPNavNodeRef& Node);

	LOOTNPOP_API uint64 GetAllocatedBytes(const FLNPNavData& Navigation);
	LOOTNPOP_API uint64 GetAllocatedBytes(const FLNPNavTraversalData& Traversal);
	LOOTNPOP_API uint64 GetAllocatedBytes(const FLNPNavSnapshot& Snapshot);
}
