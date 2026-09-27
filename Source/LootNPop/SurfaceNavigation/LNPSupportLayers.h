// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FLNPBakeSupportSource;
struct FLNPBakeTriangleMesh;

struct FLNPSupportLayerSettings
{
	/** walkable 판정의 최소 dot(면 법선, 지역 Up). 지각 Atlas·exact 이동과 같은 약 45도다. */
	double WalkableMinDot = 0.71;

	/** 연결성 판정 전에 이 거리(cm) 안의 정점을 같은 점으로 합친다. 지각 광선 교차 합침 거리와 같다. */
	double WeldDistance = 0.1;
};

/** Support Layer 하나. 배열 위치가 LocalLayerId다. */
struct FLNPSupportLayer
{
	/** 입력 source 배열의 인덱스. */
	int32 SourceIndex = INDEX_NONE;

	/** 이 Layer의 삼각형(source mesh의 추출 순서 인덱스). 지각은 non-walkable까지 전부다. */
	TArray<int32> Triangles;

	/** 삼각형들의 최소 external face 번호. 같은 source 안의 Layer 순서를 정한다. */
	int32 MinExternalFace = INDEX_NONE;
};

/**
 * source 하나의 face→Layer 표(D-037). 키는 exact hit FaceIndex와 같은 external face 번호다.
 * 모든 face가 같은 값이면 UniformLayer 하나로 두고 LayerByExternalFace는 비운다.
 */
struct LOOTNPOP_API FLNPSupportFaceMap
{
	uint16 UniformLayer = 0;
	/** 비어 있지 않으면 face 단위 표. 길이는 external 번호 최댓값+1, 추출되지 않은 번호와 non-walkable face는 NoLayer다. */
	TArray<uint16> LayerByExternalFace;

	bool IsUniform() const { return LayerByExternalFace.IsEmpty(); }
	uint16 Resolve(int32 ExternalFace) const;
};

struct FLNPSupportLayerSet
{
	TArray<FLNPSupportLayer> Layers;
	/** 입력 source 순서. */
	TArray<FLNPSupportFaceMap> FaceMaps;
};

/**
 * Support source → Layer 분리와 face→Layer 표(`phases/Phase04b_MultiLayerSupport.md` §3.3·§3.4).
 * 베이커 핵심 계산이므로 runtime 모듈의 순수 함수로 둔다(D-034).
 */
namespace LNPSupportLayers
{
	/** Layer에 속하지 않는 face(non-walkable, 추출되지 않은 번호). */
	constexpr uint16 NoLayer = 0xFFFF;

	/**
	 * 지각은 Layer 0 하나다(non-walkable 포함). 나머지 source는 walkable 삼각형의 연결 성분마다 Layer 하나다.
	 * walkable은 앞면 법선과 삼각형 중심의 지역 Up(-normalize(centroid))의 dot이 기준 이상인 삼각형이다.
	 * 연결은 위치로 용접한 정점 기준 모서리 공유다. Layer ID는 지각 뒤에 source Key 오름차순,
	 * 같은 source 안에서는 MinExternalFace 오름차순이다. 모든 source는 ExternalFaceIndices를 가져야 한다.
	 */
	LOOTNPOP_API bool BuildLayers(
		TConstArrayView<FLNPBakeSupportSource> Sources,
		int32 CrustIndex,
		const FLNPSupportLayerSettings& Settings,
		FLNPSupportLayerSet& OutSet,
		FString& OutError);

	/** Layer 삼각형만 남긴 mesh. 정점 배열은 그대로 두고 삼각형과 ExternalFaceIndices만 거른다. */
	LOOTNPOP_API FLNPBakeTriangleMesh MakeLayerMesh(const FLNPBakeTriangleMesh& SourceMesh, const FLNPSupportLayer& Layer);
}
