// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "LNPPositionHistoryFragment.generated.h"

/** Lag Compensation용 위치 스냅샷 1개. */
USTRUCT()
struct FLNPPositionHistorySample
{
	GENERATED_BODY()

	double  Timestamp = 0.0;   // World->GetTimeSeconds() 기준
	FVector Location  = FVector::ZeroVector;
};

/**
 * 서버 전용 위치 히스토리 링버퍼. Enemy NPC·플레이어 캐릭터 아키타입에 부착된다.
 * ULNPPositionHistoryRecordProcessor가 50ms 간격으로 기록하며, 최대 200ms(5샘플) 되감기를 지원한다.
 * 클라이언트에서는 기록하지 않는다 (Lag Compensation은 서버 판정 전용).
 */
USTRUCT()
struct LOOTNPOP_API FLNPPositionHistoryFragment : public FMassFragment
{
	GENERATED_BODY()

	static constexpr int32 MaxSamples = 5;

	FLNPPositionHistorySample Samples[MaxSamples];
	int32  Count          = 0;   // 채워진 샘플 수 (포화 전에는 MaxSamples 미만)
	int32  NextWriteIdx   = 0;   // 다음에 덮어쓸 링버퍼 인덱스
	double LastRecordTime = 0.0;

	/** QueryTime 시점의 위치를 샘플 사이 선형 보간해 반환한다. 범위를 벗어나면 최고령/최신 샘플로 클램프한다. */
	FVector GetInterpolatedLocation(double QueryTime) const;
};
