// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicTerrain/LNPConditionalPatchTypes.h"
#include "SurfaceNavigation/LNPNavData.h"
#include "SurfaceNavigation/LNPSupportAtlas.h"

/** 양방향 edge 주소. base node 또는 이 patch의 추가 node만 가리킨다. */
struct FLNPConditionalPatchEdge
{
	FLNPLocalNavNodeRef A;
	FLNPLocalNavNodeRef B;
};

/** source 번호는 마커의 PatchSources 배열 번호다. face map은 전역 asset-local Layer ID를 담는다. */
struct FLNPConditionalPatchFaceMap
{
	uint16 SourceIndex = 0;
	FLNPSupportFaceMap FaceMap;
};

/** 정적 Layer 뒤에 배정된 stable ID와 base 주소로만 저장하는 마커 하나의 patch. */
struct FLNPConditionalPatch
{
	FGuid MarkerId;
	ELNPConditionalPatchCondition Condition = ELNPConditionalPatchCondition::Intact;
	uint16 FirstSupportLayer = 0;
	TArray<FLNPSupportAtlasLayer> SupportLayers;
	TArray<FLNPConditionalPatchFaceMap> FaceMaps;
	/** Layer ID는 FirstSupportLayer부터, component ID는 이 patch 안에서 0부터다. */
	FLNPNavData Navigation;
	TArray<FLNPLocalNavNodeRef> BlockedNodes;
	TArray<FLNPConditionalPatchEdge> BlockedEdges;
	TArray<FLNPConditionalPatchEdge> WalkEdges;
};

struct FLNPConditionalPatchData
{
	/** MarkerId A/B/C/D 사전식 오름차순. */
	TArray<FLNPConditionalPatch> Patches;
};

namespace LNPConditionalPatch
{
	constexpr uint16 CodecVersion = 1;
	LOOTNPOP_API bool MarkerLess(const FGuid& A, const FGuid& B);
	LOOTNPOP_API bool NodeLess(const FLNPLocalNavNodeRef& A, const FLNPLocalNavNodeRef& B);
	LOOTNPOP_API void Canonicalize(FLNPConditionalPatch& Patch);
	/** 임시 base+patch Nav 베이크에서 추가 Layer와 명시적 연결만 추출한다. 정적 node 주소를 바꾸지 않는다. */
	LOOTNPOP_API bool ExtractNavigationDelta(const FLNPNavData& BaseNavigation,
		const FLNPNavData& CombinedNavigation, const FLNPNavTraversalData& CombinedTraversal,
		FLNPConditionalPatch& OutPatch, FString& OutError);
	LOOTNPOP_API bool Validate(const FLNPConditionalPatchData& Data,
		const FLNPSupportAtlas& BaseSupport, const FLNPNavData& BaseNavigation, FString& OutError);
	LOOTNPOP_API bool Encode(const FLNPConditionalPatchData& Data,
		const FLNPSupportAtlas& BaseSupport, const FLNPNavData& BaseNavigation,
		TArray<uint8>& OutPayload, FString& OutError);
	LOOTNPOP_API bool Decode(TConstArrayView<uint8> Payload,
		const FLNPSupportAtlas& BaseSupport, const FLNPNavData& BaseNavigation,
		FLNPConditionalPatchData& OutData, FString& OutError);
}
