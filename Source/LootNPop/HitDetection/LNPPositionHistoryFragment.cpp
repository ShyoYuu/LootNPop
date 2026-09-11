// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPPositionHistoryFragment.h"
#include "Replication/LNPMassReplication.h"   // 복제 LOD 거리 경계·갱신 주기 — 보간 지연의 출처
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarCompensateInterpolationLag(
		TEXT("LNP.HitDetection.CompensateInterpolationLag"), 1,
		TEXT("Add the client interpolation delay to the server rewind for interpolated pure entities. 0: off (RTT/2 only), 1: on."),
		ECVF_Cheat);

	// 계측 — 보정 유무 두 판정을 모두 돌려 "보정이 뒤집은 명중"을 센다.
	TAutoConsoleVariable<int32> CVarMeasureInterpolationLag(
		TEXT("LNP.HitDetection.MeasureInterpolationLag"), 0,
		TEXT("Log every guest hit on a pure entity twice - with and without the interpolation lag rewind - to measure how many hits the compensation rescues. 0: off, 1: on."),
		ECVF_Cheat);
}

bool LNPHitDetection::IsMeasuringInterpolationLag()
{
	return CVarMeasureInterpolationLag.GetValueOnAnyThread() != 0;
}

float LNPHitDetection::GetInterpolationLagSeconds(const float DistSqFromViewer, const bool bIgnoreCVar)
{
	// bIgnoreCVar: 계측 경로가 "보정을 켰다면 얼마였을까"를 CVar 값과 무관하게 물을 때 쓴다.
	if (!bIgnoreCVar && CVarCompensateInterpolationLag.GetValueOnAnyThread() == 0)
		return 0.f;

	const EMassLOD::Type Tier = LNP::Replication::GetReplicationLODByDistanceSq(DistSqFromViewer);
	return LNP::Replication::LODUpdateInterval[Tier];
}

// 링버퍼가 되감기 최댓값(핑 항 상한 + 보간 지연 항 최댓값)을 덮는지 못 박는다.
// 모자라면 GetInterpolatedLocation이 조용히 최고령 샘플로 클램프해 되감기가 소리 없이 짧아진다.
static_assert(
	(FLNPPositionHistoryFragment::MaxSamples - 1) * 0.05 >= 0.2 + 0.3,
	"Position history must span MaxPingRewindSeconds + the longest replication update interval (Low = 0.3s).");

const TCHAR* LNPHitDetection::GetReplicationLODName(const float DistSqFromViewer)
{
	switch (LNP::Replication::GetReplicationLODByDistanceSq(DistSqFromViewer))
	{
	case EMassLOD::High:   return TEXT("High");
	case EMassLOD::Medium: return TEXT("Medium");
	default:               return TEXT("Low");
	}
}

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
