// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "IO/IoHash.h"
#include "SurfaceNavigation/LNPSupportLayers.h"

struct FLNPBakeTriangleMesh;

/**
 * Support Atlas 샘플 플래그. 세부 원인 비트(NearCoverageEdge·HeightDiscontinuity·SteepSlope)는
 * codec에 자리만 두고 소비자가 생길 때 채운다(`phases/Phase04a_CrustAtlasAndSeams.md` §3.5).
 */
enum class ELNPSupportSampleFlags : uint8
{
	None = 0,
	/** 광선이 Layer 앞면을 정확히 한 번 맞혔다. 아니면 coverage hole이다. */
	Valid = 1 << 0,
	/** 법선과 지역 Up(구 중심 방향)의 dot이 walkable 기준 이상이다. */
	Walkable = 1 << 1,
	/** 이 샘플 주변은 보간하지 않고 exact query로 판정해야 한다. */
	NeedsExact = 1 << 2,
};
ENUM_CLASS_FLAGS(ELNPSupportSampleFlags);

/** 양자화 전 샘플. Radius는 구 중심에서 hit까지 거리(cm), Normal은 hit 삼각형의 앞면 법선이다. */
struct FLNPSupportSample
{
	double Radius = 0.0;
	FVector3f Normal = FVector3f::ZeroVector;
	ELNPSupportSampleFlags Flags = ELNPSupportSampleFlags::None;
};

struct FLNPSupportRasterSettings
{
	/** 격자 분할 수. 지각은 N, 비지각 Layer는 N의 정수배(D-057)다. */
	int32 Subdivisions = 0;

	/** walkable 판정의 최소 dot(Normal, Up). exact 이동(`FLNPEnemyExactMovement`)과 같은 약 45도다. */
	double WalkableMinDot = 0.71;

	/** 이웃 샘플 법선 각도 차가 이 값을 넘으면 NeedsExact다. */
	double MaxNeighborNormalAngleDeg = 25.0;

	/** 한 광선에서 이 거리(cm) 안의 앞면 교차는 삼각형 모서리 중복으로 보고 하나로 합친다. */
	double HitMergeDistance = 0.1;

	/**
	 * 지각 전용. 이음매 평면에서 이 거리(cm) 안의 정점 성분은 0으로 맞춘다. 변 위 샘플 광선은 이음매 평면 위를 지나므로
	 * 경계 정점이 부동소수점 잡음만큼 안쪽에 있어도 경계 변을 스쳐 빗나간다(`Meadow_00` 실측 |d| < 5e-7cm).
	 */
	double SeamSnapDistance = 1e-3;
};

/** 격자 행 하나의 i 구간 [IStart, IStart+Count). */
struct FLNPSupportRowSpan
{
	int32 IStart = 0;
	int32 Count = 0;
};

/**
 * Layer 샘플 배치(D-057). j 범위 [J0, J0+Rows.Num())와 행별 i 구간만 가진다. 지각은 모든 행을 채운 배치라
 * 샘플 인덱스가 `LNPSupportAtlas::GetSampleIndex`와 같다.
 */
struct LOOTNPOP_API FLNPSupportLayout
{
	int32 Subdivisions = 0;
	int32 J0 = 0;
	TArray<FLNPSupportRowSpan> Rows;
	/** 행마다 첫 샘플 인덱스. Rows.Num()+1개이고 마지막 값이 샘플 수다. `BuildOffsets`가 채운다. */
	TArray<int32> RowOffsets;

	/** 옥탄트 삼각형 전체를 채운 배치. */
	static FLNPSupportLayout MakeFull(int32 Subdivisions);

	void BuildOffsets();
	int32 Num() const { return RowOffsets.IsEmpty() ? 0 : RowOffsets.Last(); }
	bool IsFull() const;

	/** (i, j)의 샘플 인덱스. 구간 밖이면 INDEX_NONE이다. */
	int32 Find(int32 I, int32 J) const;
};

/** Layer 하나의 양자화 전 샘플. 샘플 순서는 Layout의 행 우선, 행 안에서 i 증가다. */
struct FLNPSupportLayerRaster
{
	FLNPSupportLayout Layout;
	TArray<FLNPSupportSample> Samples;
	/** codec이 기록하는 source 표 번호. */
	int32 SourceIndex = INDEX_NONE;
};

struct FLNPSupportCodecSettings
{
	/** 지각 Layer 반지름 양자화 기준. 옥탄트 기준 지각 반지름이다. 비지각 Layer는 자기 샘플 반지름으로 기준을 정한다. */
	double BaseRadius = 0.0;

	/** 반지름 양자화 step(cm). Layer 기준 반지름 대비 offset이 step × 32,767을 넘으면 인코딩 오류다. */
	double RadiusStep = 0.25;
};

/** 디코딩한 Layer. 샘플은 양자화 값 그대로 두고 조회 때 복원한다. */
struct LOOTNPOP_API FLNPSupportAtlasLayer
{
	FLNPSupportLayout Layout;
	int32 SourceIndex = INDEX_NONE;
	double BaseRadius = 0.0;
	double RadiusStep = 0.0;
	TArray<int16> RadiusQ;
	/** octahedral 인코딩 법선, 샘플당 2개. */
	TArray<int16> NormalQ;
	TArray<uint8> Flags;

	int32 Num() const { return Flags.Num(); }
	double GetRadius(int32 Index) const { return BaseRadius + RadiusQ[Index] * RadiusStep; }
	FVector3f GetNormal(int32 Index) const;
	ELNPSupportSampleFlags GetFlags(int32 Index) const { return static_cast<ELNPSupportSampleFlags>(Flags[Index]); }
};

/** source 하나. Key는 `FLNPBakeSupportSource::Key`, FaceMap 값은 LocalLayerId다. */
struct FLNPSupportAtlasSource
{
	FString Key;
	FLNPSupportFaceMap FaceMap;
};

/** 옥탄트 하나의 다층 Support Atlas. Layer 배열 위치가 LocalLayerId이고 Layer 0은 지각이다. */
struct FLNPSupportAtlas
{
	TArray<FLNPSupportAtlasLayer> Layers;
	/** Key 오름차순. */
	TArray<FLNPSupportAtlasSource> Sources;
	/** 지각 변 샘플(양자화 반지름·Valid)을 규약 순서로 hash한 값. ELNPCrustSeamEdge 순서다. */
	FIoHash SeamHashes[3];
};

/** Layer 하나를 한 방향에서 조회한 결과. */
struct FLNPSupportLayerQuery
{
	/** 방향을 담은 격자 삼각형 꼭짓점 중 Layer 구간 안에서 Valid인 수. 0이면 이 Layer는 후보가 아니다. */
	int32 ValidCorners = 0;
	/** 세 꼭짓점이 모두 Valid이고 NeedsExact가 아니어서 보간했다. */
	bool bInterpolated = false;
	/** bInterpolated일 때만 의미가 있다. */
	double Radius = 0.0;
	FVector3f Normal = FVector3f::ZeroVector;
	/** ValidCorners > 0일 때 Valid 꼭짓점 반지름 범위. */
	double MinCornerRadius = 0.0;
	double MaxCornerRadius = 0.0;
};

enum class ELNPSupportQueryResult : uint8
{
	/** 탐색 창 안의 Layer 하나를 보간으로 골랐다. */
	Supported,
	/** 창에 걸친 Layer가 보간되지 않는다. 호출자는 exact query를 쓴다. */
	NeedsExact,
	/** 창 안에 지지면이 없다. */
	NoSupport,
};

struct FLNPSupportLayerHit
{
	uint16 Layer = LNPSupportLayers::NoLayer;
	double Radius = 0.0;
	FVector3f Normal = FVector3f::ZeroVector;
};

/**
 * 옥탄트 면 x+y+z=1 위 꼭짓점 중심 삼각 격자와 Layer rasterization·codec·조회(`design/SurfaceBaking.md` "지각 Atlas 규약",
 * `phases/Phase04b_MultiLayerSupport.md` §3.2·§3.5~§3.7). 격자점 (i, j, k=N-i-j)의 방향은 normalize(i, j, k)다.
 * 베이커 핵심 계산이므로 runtime 모듈의 순수 함수로 둔다(D-034).
 */
namespace LNPSupportAtlas
{
	LOOTNPOP_API int32 GetSampleCount(int32 Subdivisions);

	/** 옥탄트 삼각형 전체를 채운 배치에서 j 행 우선, 행 안에서 i 증가 순서의 인덱스. */
	LOOTNPOP_API int32 GetSampleIndex(int32 Subdivisions, int32 I, int32 J);

	LOOTNPOP_API FVector3d GetSampleDirection(int32 Subdivisions, int32 I, int32 J);

	/** 가장 큰 셀인 옥탄트 중심 간격(R·√6/N)이 Spacing 이하가 되는 최소 N. */
	LOOTNPOP_API int32 ComputeSubdivisionsForSpacing(double Radius, double Spacing);

	/** 법선의 octahedral 2×int16 인코딩과 복원. 복원 결과는 정규화돼 있다. */
	LOOTNPOP_API void EncodeNormal(const FVector3f& Normal, int16& OutX, int16& OutY);
	LOOTNPOP_API FVector3f DecodeNormal(int16 X, int16 Y);

	/**
	 * 삼각형들을 옥탄트 면에 중심 투영한 영역 안의 격자점을 모두 담는 최소 행 구간. 광선은 투영 영역 밖에서 삼각형을
	 * 맞힐 수 없으므로 구간 밖 샘플은 모두 coverage hole이다.
	 */
	LOOTNPOP_API FLNPSupportLayout ComputeFootprint(const FLNPBakeTriangleMesh& Mesh, int32 Subdivisions);

	/**
	 * Layout의 격자 방향마다 구 중심에서 바깥쪽 광선을 쏴 Mesh의 앞면 교차를 샘플링한다. 뒷면은 Chaos 단순 trimesh 쿼리가
	 * 맞히지 않으므로 세지 않는다. 앞면 교차가 둘 이상인 방향(지각 overhang, Layer의 접힌 sheet)이 있으면 베이크 오류다.
	 * NeedsExact는 자신이 invalid·non-walkable이거나, 구간 밖을 포함한 6-이웃 중 invalid가 있거나, 이웃과의 선분 경사가
	 * walkable 기준보다 가파르거나, 이웃과 법선 각도 차가 기준을 넘을 때다. 옥탄트 밖 이웃은 보지 않는다.
	 * Settings.Subdivisions는 무시하고 Layout.Subdivisions를 쓴다.
	 */
	LOOTNPOP_API bool Rasterize(
		const FLNPBakeTriangleMesh& Mesh,
		const FLNPSupportRasterSettings& Settings,
		const FLNPSupportLayout& Layout,
		FLNPSupportLayerRaster& OutRaster,
		FString& OutError);

	/** payload codec 버전. 레이아웃을 바꾸면 올리고 `FLNPSurfaceBakeHeader::CurrentDataVersion`도 올린다. */
	constexpr uint16 CodecVersion = 2;

	/**
	 * codec v2 payload(`design/SurfaceBaking.md` "codec v2"). 모든 값은 little-endian이다.
	 *   header: uint16 CodecVersion, uint16 LayerCount, uint16 SourceCount, uint16 Reserved, double RadiusStep,
	 *           FIoHash SeamHashes[3]
	 *   Layer 표(Layer마다): uint16 SourceIndex, uint16 Reserved, double BaseRadius, int32 Subdivisions, int32 J0,
	 *           int32 RowCount, {int32 IStart, int32 Count}[RowCount]
	 *   body(Layer마다 SoA): int16 RadiusQ[n], int16 NormalQ[2n], uint8 Flags[n]
	 *   source 표(source마다): int32 KeyBytes, UTF-8 Key, uint8 FaceMapKind(0 컴포넌트 단위, 1 face 단위),
	 *           kind 0이면 uint16 Layer, kind 1이면 int32 Count와 uint16 Layer[Count]
	 * Layer 0은 지각이며 전체 배치이고 기준 반지름이 Settings.BaseRadius다. 나머지 Layer의 분할 수는 지각 N의 정수배이고,
	 * 기준 반지름은 Valid 샘플 반지름 범위의 중간값을 step 단위로 반올림한 값이다. source는 Key 오름차순이어야 한다.
	 * invalid 샘플의 반지름·법선은 0이다. 반지름 offset이 int16 범위를 넘거나 값이 유한하지 않으면 오류다.
	 */
	LOOTNPOP_API bool Encode(
		TConstArrayView<FLNPSupportLayerRaster> Layers,
		TConstArrayView<FLNPSupportAtlasSource> Sources,
		const FLNPSupportCodecSettings& Settings,
		TArray<uint8>& OutPayload,
		FString& OutError);

	LOOTNPOP_API bool Decode(TConstArrayView<uint8> Payload, FLNPSupportAtlas& OutAtlas, FString& OutError);

	/**
	 * 옥탄트 로컬 방향(성분이 모두 0 이상)을 담은 격자 삼각형 세 꼭짓점으로 Layer 하나를 보간한다. 세 꼭짓점이 모두
	 * Valid이고 NeedsExact가 아닐 때만 true다(`design/SurfaceBaking.md` "보간 규칙"). false면 nearest 샘플로 지면을
	 * 연장하지 않는다. 방향이 이 옥탄트가 아니면 false이고 ValidCorners는 0이다.
	 */
	LOOTNPOP_API bool QueryLayer(const FLNPSupportAtlasLayer& Layer, const FVector3d& LocalDirection, FLNPSupportLayerQuery& OutQuery);

	/**
	 * 같은 방향에 겹친 Layer 중 발 위치 기준 하나를 고른다(`phases/Phase04b_MultiLayerSupport.md` §3.5). 반지름이 작을수록 위다.
	 * 탐색 창은 [FeetRadius - MaxStepUp, FeetRadius + MaxDrop]이다.
	 * 1. 창에 Valid 꼭짓점 반지름 범위가 걸치는데 보간되지 않는 Layer가 하나라도 있으면 NeedsExact다.
	 *    가장자리에서 가까운 Layer를 건너뛰고 먼 Layer로 떨어지지 않게 하기 위해서다.
	 * 2. 아니면 보간 반지름이 창 안인 Layer 중 PreferredLayer를 우선하고, 없으면 가장 위 Layer를 고른다.
	 * 3. 창 안에 없으면 NoSupport다. 방향이 이 옥탄트가 아니면 NeedsExact다.
	 */
	LOOTNPOP_API ELNPSupportQueryResult QueryLayers(
		const FLNPSupportAtlas& Atlas,
		const FVector3d& LocalDirection,
		double FeetRadius,
		double MaxStepUp,
		double MaxDrop,
		uint16 PreferredLayer,
		FLNPSupportLayerHit& OutHit);
}
