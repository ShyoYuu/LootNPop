// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPSupportAtlas.h"

class ULNPOctantSurfaceData;

/** 옥탄트 베이크 설정. 모든 값이 BakeSettingsHash에 들어간다. */
struct LOOTNPOPEDITOR_API FLNPOctantBakeOptions
{
	/** 지각 Atlas 옥탄트 중심 샘플 간격(cm). 격자 분할 수 N은 이 값과 기준 반지름으로 정한다. */
	double CrustSpacing = 100.0;

	/** 0 이하면 `ULNPSettings::SphereRadius`를 쓴다. */
	double BaseRadius = 0.0;

	/** 비지각 Layer 격자 분할 수 = 지각 N × 이 값(D-057). 2는 50cm, 4는 25cm 간격이다. 구현 단위 3 오차 측정으로 확정한다. */
	int32 LayerSubdivisionMultiplier = 2;

	/** 두 Layer가 같은 방향에서 이 반지름 차(cm) 안에 있으면 겹침으로 보고서에 센다. 베이크 결과에는 영향이 없다. */
	double OverlapReportHeight = 200.0;

	/** Subdivisions는 무시하고 CrustSpacing으로 정한다. */
	FLNPSupportRasterSettings Raster;
	FLNPSupportCodecSettings Codec;
};

/** Layer 하나의 베이크 통계. */
struct LOOTNPOPEDITOR_API FLNPOctantBakeLayerReport
{
	FString SourceKey;
	int32 Subdivisions = 0;
	int32 TriangleCount = 0;
	int32 RowCount = 0;
	int32 SampleCount = 0;
	int32 ValidCount = 0;
	int32 NeedsExactCount = 0;
	double MinRadius = 0.0;
	double MaxRadius = 0.0;
	/** 샘플 body 바이트. Layer 표·source 표는 빠진다. */
	int64 BodyBytes = 0;
	double RasterSeconds = 0.0;
};

/** Layer A(비지각)의 Valid 샘플 중 같은 방향 Layer B(B < A)의 Valid 꼭짓점이 겹침 높이 안에 있는 수. */
struct FLNPOctantBakeLayerOverlap
{
	int32 LayerA = INDEX_NONE;
	int32 LayerB = INDEX_NONE;
	int32 SampleCount = 0;
};

/** 베이크 통계. 로그와 오차 측정 자동화가 읽는다. 지각 필드는 Layer 0 값이다. */
struct LOOTNPOPEDITOR_API FLNPOctantBakeReport
{
	FString CrustName;
	int32 SupportSourceCount = 0;
	int32 SupportLayerCount = 0;
	int32 CrustTriangleCount = 0;
	int32 Subdivisions = 0;
	int32 SampleCount = 0;
	int32 ValidCount = 0;
	int32 WalkableCount = 0;
	int32 NeedsExactCount = 0;
	double MinRadius = 0.0;
	double MaxRadius = 0.0;
	/** 모든 Layer 샘플 수. `Header.Support.ElementCount`와 같다. */
	int32 TotalSampleCount = 0;
	int64 PayloadBytes = 0;
	double CollectSeconds = 0.0;
	double ExtractSeconds = 0.0;
	double RasterSeconds = 0.0;
	double LayerRasterSeconds = 0.0;
	double EncodeSeconds = 0.0;
	/** LocalLayerId 순서. 0은 지각이다. */
	TArray<FLNPOctantBakeLayerReport> Layers;
	TArray<FLNPOctantBakeLayerOverlap> Overlaps;

	FString ToString() const;
};

/**
 * 옥탄트 LVI → `ULNPOctantSurfaceData`. source 수집·삼각형 추출·저장만 맡고 핵심 계산은 runtime 순수 함수에 맡긴다(D-034).
 * Support source를 Layer로 나눠 지각 dense Atlas와 비지각 sparse Atlas를 한 SupportPayload(codec v2)에 굽는다
 * (`phases/Phase04b_MultiLayerSupport.md` §3).
 */
class LOOTNPOPEDITOR_API FLNPOctantSurfaceBaker
{
public:
	/** 베이커 schema 버전. 베이크 규칙이 바뀌면 올린다. BakeSettingsHash에 들어간다. */
	static constexpr uint32 BakerSchemaVersion = 2;

	/** OutData의 Header와 SupportPayload를 채운다. 같은 입력이면 같은 payload를 만든다. */
	static bool Bake(
		const FSoftObjectPath& SourceLevel,
		const FLNPOctantBakeOptions& Options,
		ULNPOctantSurfaceData& OutData,
		FLNPOctantBakeReport& OutReport,
		FString& OutError);

	/** LVI 옆 `DA_OctantSurface_<이름>` 경로. `LVI_Octant_` 접두사는 뺀다. */
	static FString GetSurfaceDataPackageName(const FSoftObjectPath& SourceLevel);

	/** 베이크한 뒤 SurfaceData 에셋을 만들거나 제자리 갱신하고 저장한다. */
	static bool BakeAndSave(
		const FSoftObjectPath& SourceLevel,
		const FLNPOctantBakeOptions& Options,
		FLNPOctantBakeReport& OutReport,
		FString& OutError);
};
