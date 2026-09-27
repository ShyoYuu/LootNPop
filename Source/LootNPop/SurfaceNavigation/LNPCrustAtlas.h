// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "IO/IoHash.h"

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

	/**
	 * 이음매 평면에서 이 거리(cm) 안의 정점 성분은 0으로 맞춘다. 변 위 샘플 광선은 이음매 평면 위를 지나므로
	 * 경계 정점이 부동소수점 잡음만큼 안쪽에 있어도 경계 변을 스쳐 빗나간다(`Meadow_00` 실측 |d| < 5e-7cm).
	 */
	double SeamSnapDistance = 1e-3;
};

struct FLNPCrustAtlasRaster
{
	int32 Subdivisions = 0;
	TArray<FLNPCrustSample> Samples;
};

struct FLNPCrustCodecSettings
{
	/** 반지름 양자화 기준. 옥탄트 기준 지각 반지름이다. */
	double BaseRadius = 0.0;

	/** 반지름 양자화 step(cm). 기준 반지름 대비 offset이 step × 32,767을 넘으면 인코딩 오류다. */
	double RadiusStep = 0.25;
};

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
 * 디코딩한 지각 Atlas. 샘플은 양자화 값 그대로 두고 조회 때 복원한다.
 * payload(codec v1) 레이아웃은 `Encode` 주석을 따른다.
 */
struct LOOTNPOP_API FLNPCrustAtlas
{
	int32 Subdivisions = 0;
	double BaseRadius = 0.0;
	double RadiusStep = 0.0;
	TArray<int16> RadiusQ;
	/** octahedral 인코딩 법선, 샘플당 2개. */
	TArray<int16> NormalQ;
	TArray<uint8> Flags;
	/** 변 샘플(양자화 반지름·Valid)을 규약 순서로 hash한 값. ELNPCrustSeamEdge 순서다. */
	FIoHash SeamHashes[3];

	int32 Num() const { return Flags.Num(); }
	double GetRadius(int32 Index) const { return BaseRadius + RadiusQ[Index] * RadiusStep; }
	FVector3f GetNormal(int32 Index) const;
	ELNPSupportSampleFlags GetFlags(int32 Index) const { return static_cast<ELNPSupportSampleFlags>(Flags[Index]); }
};

/** 지각 Atlas 조회 결과. 보간할 수 없으면 조회 함수가 false를 돌려주고 호출자는 exact query를 쓴다. */
struct FLNPCrustSupportHit
{
	double Radius = 0.0;
	FVector3f Normal = FVector3f::ZeroVector;
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

	/** payload codec 버전. 레이아웃을 바꾸면 올리고 `FLNPSurfaceBakeHeader::CurrentDataVersion`도 올린다. */
	constexpr uint16 CodecVersion = 1;

	/**
	 * 변 위 격자점 (i, j)를 규약 순서로 돌려준다. 각 변은 로컬 축 번호가 작은 꼭짓점에서 큰 꼭짓점 쪽으로
	 * N+1개다(z=0은 +X→+Y, x=0은 +Y→+Z, y=0은 +X→+Z).
	 */
	LOOTNPOP_API FIntPoint GetSeamSampleCoord(int32 Subdivisions, ELNPCrustSeamEdge Edge, int32 Step);

	/**
	 * slot 회전으로 변 양 끝 꼭짓점을 월드 축에 놓아 변 인스턴스를 월드 변별로 짝짓는다.
	 * 결과는 (A.Slot, A.Edge) 오름차순이고 A.Slot < B.Slot이다. 옥탄트 8개를 모두 채우는 회전 집합이면
	 * 24개 변 인스턴스가 12쌍이 된다. 어떤 월드 변에 인스턴스가 둘이 아니면 오류다.
	 */
	LOOTNPOP_API bool ComputeSeamPairs(
		TConstArrayView<FRotator> SlotRotations,
		TArray<FLNPCrustSeamPair>& OutPairs,
		FString& OutError);

	/**
	 * 법선의 octahedral 2×int16 인코딩과 복원. 복원 결과는 정규화돼 있다.
	 */
	LOOTNPOP_API void EncodeNormal(const FVector3f& Normal, int16& OutX, int16& OutY);
	LOOTNPOP_API FVector3f DecodeNormal(int16 X, int16 Y);

	/**
	 * codec v1 payload. 모든 값은 little-endian이다.
	 *   header: uint16 CodecVersion, uint8 LayerCount(=1), uint8 Reserved, int32 N, double BaseRadius,
	 *           double RadiusStep, uint32 SampleCount, FIoHash SeamHashes[3]
	 *   body(SoA): int16 RadiusQ[SampleCount], int16 NormalQ[2·SampleCount], uint8 Flags[SampleCount]
	 * invalid 샘플의 반지름·법선은 0이다. 반지름 offset이 int16 범위를 넘거나 값이 유한하지 않으면 오류다.
	 */
	LOOTNPOP_API bool Encode(
		const FLNPCrustAtlasRaster& Raster,
		const FLNPCrustCodecSettings& Settings,
		TArray<uint8>& OutPayload,
		FString& OutError);

	LOOTNPOP_API bool Decode(TConstArrayView<uint8> Payload, FLNPCrustAtlas& OutAtlas, FString& OutError);

	/**
	 * 옥탄트 로컬 방향(성분이 모두 0 이상)의 지면을 격자 삼각형 세 꼭짓점으로 보간한다.
	 * 세 꼭짓점이 모두 Valid이고 NeedsExact가 아닐 때만 true다(`design/SurfaceBaking.md` "보간 규칙").
	 * NeedsExact는 invalid·non-walkable 이웃, 가파른 선분, 큰 법선 변화를 이미 담고 있다.
	 * false면 nearest 샘플로 지면을 연장하지 않는다. 호출자는 exact query를 쓴다.
	 */
	LOOTNPOP_API bool QuerySupport(const FLNPCrustAtlas& Atlas, const FVector3d& LocalDirection, FLNPCrustSupportHit& OutHit);
}
