// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPPositionHistoryFragment.h"

FVector FLNPPositionHistoryFragment::GetInterpolatedLocation(double QueryTime) const
{
	if (Count == 0)
		return FVector::ZeroVector;

	// 링버퍼를 오래된 순 → 최신 순으로 순회하기 위한 시간순 인덱스 변환
	auto SampleAt = [this](int32 ChronoIdx) -> const FLNPPositionHistorySample&
	{
		const int32 RingIdx = (NextWriteIdx - Count + ChronoIdx + MaxSamples * 2) % MaxSamples;
		return Samples[RingIdx];
	};

	if (QueryTime <= SampleAt(0).Timestamp)
		return SampleAt(0).Location;
	if (QueryTime >= SampleAt(Count - 1).Timestamp)
		return SampleAt(Count - 1).Location;

	for (int32 i = 0; i < Count - 1; ++i)
	{
		const FLNPPositionHistorySample& A = SampleAt(i);
		const FLNPPositionHistorySample& B = SampleAt(i + 1);
		if (A.Timestamp <= QueryTime && QueryTime <= B.Timestamp)
		{
			const double Span  = B.Timestamp - A.Timestamp;
			const float  Alpha = Span > KINDA_SMALL_NUMBER ? (float)((QueryTime - A.Timestamp) / Span) : 0.f;
			return FMath::Lerp(A.Location, B.Location, Alpha);
		}
	}
	return SampleAt(Count - 1).Location;
}
