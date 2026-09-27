// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "IO/IoHash.h"
#include "SurfaceNavigation/LNPSupportAtlas.h"

struct FLNPBakeTriangleMesh;

/** 옥탄트 이음매 변. 번호는 그 변이 놓인 평면의 축(x=0, y=0, z=0)이다. */
enum class ELNPCrustSeamEdge : uint8
{
	X0 = 0,
	Y0 = 1,
	Z0 = 2,
};

/** slot 하나의 이음매 변. */
struct FLNPCrustSeamEdgeRef
{
	int32 Slot = INDEX_NONE;
	ELNPCrustSeamEdge Edge = ELNPCrustSeamEdge::X0;
};

/**
 * 같은 월드 변을 공유하는 두 변 인스턴스. bReversed면 A의 step s가 B의 step N-s와 같은 방향이다.
 */
struct FLNPCrustSeamPair
{
	FLNPCrustSeamEdgeRef A;
	FLNPCrustSeamEdgeRef B;
	bool bReversed = false;
};

/**
 * 지각(Layer 0) 전용 규약: 이음매 스냅 rasterization과 이음매 샘플 순서·짝·hash(`design/SurfaceBaking.md` "지각 Atlas 규약").
 * 격자·codec·조회는 모든 Layer 공용인 `LNPSupportAtlas`가 맡는다. 베이커 핵심 계산이므로 runtime 모듈의 순수 함수로 둔다(D-034).
 */
namespace LNPCrustAtlas
{
	/**
	 * 이음매 평면 근처 정점 성분을 0으로 맞춘 뒤 옥탄트 전체 배치로 `LNPSupportAtlas::Rasterize`한다.
	 * 앞면 교차가 둘 이상인 방향(overhang)이 있으면 베이크 오류다.
	 */
	LOOTNPOP_API bool Rasterize(
		const FLNPBakeTriangleMesh& Crust,
		const FLNPSupportRasterSettings& Settings,
		FLNPSupportLayerRaster& OutRaster,
		FString& OutError);

	/**
	 * 변 위 격자점 (i, j)를 규약 순서로 돌려준다. 각 변은 로컬 축 번호가 작은 꼭짓점에서 큰 꼭짓점 쪽으로
	 * N+1개다(z=0은 +X→+Y, x=0은 +Y→+Z, y=0은 +X→+Z).
	 */
	LOOTNPOP_API FIntPoint GetSeamSampleCoord(int32 Subdivisions, ELNPCrustSeamEdge Edge, int32 Step);

	/** 변 샘플(양자화 반지름·Valid)을 규약 순서로 hash한다. 법선은 넣지 않는다. Crust는 전체 배치여야 한다. */
	LOOTNPOP_API void ComputeSeamHashes(const FLNPSupportAtlasLayer& Crust, FIoHash (&OutHashes)[3]);

	/**
	 * slot 회전으로 변 양 끝 꼭짓점을 월드 축에 놓아 변 인스턴스를 월드 변별로 짝짓는다.
	 * 결과는 (A.Slot, A.Edge) 오름차순이고 A.Slot < B.Slot이다. 옥탄트 8개를 모두 채우는 회전 집합이면
	 * 24개 변 인스턴스가 12쌍이 된다. 어떤 월드 변에 인스턴스가 둘이 아니면 오류다.
	 */
	LOOTNPOP_API bool ComputeSeamPairs(
		TConstArrayView<FRotator> SlotRotations,
		TArray<FLNPCrustSeamPair>& OutPairs,
		FString& OutError);
}
