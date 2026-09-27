// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FLNPBakeTriangleMesh;

/**
 * 지각 Support Atlas 샘플 플래그. 세부 원인 비트(NearCoverageEdge·HeightDiscontinuity·SteepSlope)는
 * codec에 자리만 두고 소비자가 생길 때 채운다(`phases/Phase04a_CrustAtlasAndSeams.md` §3.5).
 */
enum class ELNPSupportSampleFlags : uint8
{
	None = 0,
	/** 광선이 지각 앞면을 정확히 한 번 맞혔다. 아니면 coverage hole이다. */
	Valid = 1 << 0,
	/** 법선과 지역 Up(구 중심 방향)의 dot이 walkable 기준 이상이다. */
	Walkable = 1 << 1,
	/** 이 샘플 주변은 보간하지 않고 exact query로 판정해야 한다. */
	NeedsExact = 1 << 2,
};
ENUM_CLASS_FLAGS(ELNPSupportSampleFlags);

/** 양자화 전 샘플. Radius는 구 중심에서 hit까지 거리(cm), Normal은 hit 삼각형의 앞면 법선이다. */
struct FLNPCrustSample
{
	double Radius = 0.0;
	FVector3f Normal = FVector3f::ZeroVector;
	ELNPSupportSampleFlags Flags = ELNPSupportSampleFlags::None;
};

struct FLNPCrustRasterSettings
{
	/** 격자 분할 수 N. 샘플 수는 (N+1)(N+2)/2다. */
	int32 Subdivisions = 0;

	/** walkable 판정의 최소 dot(Normal, Up). exact 이동(`FLNPEnemyExactMovement`)과 같은 약 45도다. */
	double WalkableMinDot = 0.71;

	/** 이웃 샘플 법선 각도 차가 이 값을 넘으면 NeedsExact다. 초안 값이며 구현 단위 3 오차 측정으로 확정한다. */
	double MaxNeighborNormalAngleDeg = 25.0;

	/** 한 광선에서 이 거리(cm) 안의 앞면 교차는 삼각형 모서리 중복으로 보고 하나로 합친다. */
	double HitMergeDistance = 0.1;
};

struct FLNPCrustAtlasRaster
{
	int32 Subdivisions = 0;
	TArray<FLNPCrustSample> Samples;
};

/**
 * 옥탄트 면 x+y+z=1 위 꼭짓점 중심 삼각 격자(`phases/Phase04a_CrustAtlasAndSeams.md` §3.4)와 지각 rasterization.
 * 격자점 (i, j, k=N-i-j)의 방향은 normalize(i, j, k)이고, 변 위 격자점은 이웃 옥탄트와 정확히 같은 방향이다.
 * 베이커 핵심 계산이므로 runtime 모듈의 순수 함수로 둔다(D-034).
 */
namespace LNPCrustAtlas
{
	LOOTNPOP_API int32 GetSampleCount(int32 Subdivisions);

	/** j 행 우선, 행 안에서 i 증가 순서의 인덱스. */
	LOOTNPOP_API int32 GetSampleIndex(int32 Subdivisions, int32 I, int32 J);

	LOOTNPOP_API FVector3d GetSampleDirection(int32 Subdivisions, int32 I, int32 J);

	/** 가장 큰 셀인 옥탄트 중심 간격(R·√6/N)이 Spacing 이하가 되는 최소 N. */
	LOOTNPOP_API int32 ComputeSubdivisionsForSpacing(double Radius, double Spacing);

	/**
	 * 각 격자 방향으로 구 중심에서 바깥쪽 광선을 쏴 지각 앞면 교차를 샘플링한다.
	 * exact query처럼 앞면만 센다. 뒷면은 Chaos 단순 trimesh 쿼리가 맞히지 않기 때문이다.
	 * 앞면 교차가 둘 이상인 방향(overhang)이 있으면 베이크 오류다.
	 */
	LOOTNPOP_API bool Rasterize(
		const FLNPBakeTriangleMesh& Crust,
		const FLNPCrustRasterSettings& Settings,
		FLNPCrustAtlasRaster& OutRaster,
		FString& OutError);
}
