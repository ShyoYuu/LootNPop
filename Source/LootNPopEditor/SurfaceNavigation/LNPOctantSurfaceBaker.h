// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPCrustAtlas.h"

class ULNPOctantSurfaceData;

/** 옥탄트 베이크 설정. 모든 값이 BakeSettingsHash에 들어간다. */
struct LOOTNPOPEDITOR_API FLNPOctantBakeOptions
{
	/** 지각 Atlas 옥탄트 중심 샘플 간격(cm). 격자 분할 수 N은 이 값과 기준 반지름으로 정한다. */
	double CrustSpacing = 100.0;

	/** 0 이하면 `ULNPSettings::SphereRadius`를 쓴다. */
	double BaseRadius = 0.0;

	FLNPCrustRasterSettings Raster;
	FLNPCrustCodecSettings Codec;
};

/** 베이크 통계. 로그와 오차 측정 자동화가 읽는다. */
struct LOOTNPOPEDITOR_API FLNPOctantBakeReport
{
	FString CrustName;
	int32 SupportSourceCount = 0;
	int32 CrustTriangleCount = 0;
	int32 Subdivisions = 0;
	int32 SampleCount = 0;
	int32 ValidCount = 0;
	int32 WalkableCount = 0;
	int32 NeedsExactCount = 0;
	double MinRadius = 0.0;
	double MaxRadius = 0.0;
	int64 PayloadBytes = 0;
	double CollectSeconds = 0.0;
	double ExtractSeconds = 0.0;
	double RasterSeconds = 0.0;
	double EncodeSeconds = 0.0;

	FString ToString() const;
};

/**
 * 옥탄트 LVI → `ULNPOctantSurfaceData`. source 수집·삼각형 추출·저장만 맡고 핵심 계산은 runtime 순수 함수에 맡긴다(D-034).
 * 4a는 지각 한 장의 Support Atlas만 굽는다. 비지각 Support는 검증만 한다(`phases/Phase04a_CrustAtlasAndSeams.md` §3.3).
 */
class LOOTNPOPEDITOR_API FLNPOctantSurfaceBaker
{
public:
	/** 베이커 schema 버전. 베이크 규칙이 바뀌면 올린다. BakeSettingsHash에 들어간다. */
	static constexpr uint32 BakerSchemaVersion = 1;

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
