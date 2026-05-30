// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "Config/LNPSettings.h"
#include "LootNPop.h"

#include "Engine/World.h"

// --------- FLNPSurfaceCacheSnapshot ---------

bool FLNPSurfaceCacheSnapshot::GetPoint(const FVector& WorldDirection, FVector& OutPoint) const
{
	if (!Points.IsValid() || Points->IsEmpty())
		return false;

	const TArray<FPoint>& Pts = *Points;

	const FVector Dir = WorldDirection.GetSafeNormal();
	const float Lat = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.0f, 1.0f)));
	float Lon = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	if (Lon < 0.0f) Lon += 360.0f;

	const float LatFrac = (Lat + 90.0f) / 180.0f * LatRes - 0.5f;
	const float LonFrac = Lon / 360.0f * LonRes - 0.5f;

	const int32 LatLo = FMath::Clamp(FMath::FloorToInt(LatFrac), 0, LatRes - 1);
	const int32 LatHi = FMath::Min(LatLo + 1, LatRes - 1);
	const int32 LonLo = ((FMath::FloorToInt(LonFrac) % LonRes) + LonRes) % LonRes;
	const int32 LonHi = (LonLo + 1) % LonRes;

	const float tLat = LatFrac - FMath::FloorToInt(LatFrac);
	const float tLon = LonFrac - FMath::FloorToInt(LonFrac);

	auto SafeGet = [&](int32 La, int32 Lo) -> const FVector*
	{
		const int32 Idx = La * LonRes + Lo;
		return (Pts.IsValidIndex(Idx) && Pts[Idx].bValid) ? &Pts[Idx].Loc : nullptr;
	};

	const FVector* P00 = SafeGet(LatLo, LonLo);
	const FVector* P01 = SafeGet(LatLo, LonHi);
	const FVector* P10 = SafeGet(LatHi, LonLo);
	const FVector* P11 = SafeGet(LatHi, LonHi);

	if (P00 && P01 && P10 && P11)
	{
		OutPoint = FMath::Lerp(FMath::Lerp(*P00, *P01, tLon), FMath::Lerp(*P10, *P11, tLon), tLat);
		return true;
	}

	// 최근접 이웃 폴백 (예: 극지 근처)
	const int32 LatN = FMath::Clamp(FMath::RoundToInt(LatFrac), 0, LatRes - 1);
	const int32 LonN = ((FMath::RoundToInt(LonFrac) % LonRes) + LonRes) % LonRes;
	const int32 FbIdx = LatN * LonRes + LonN;
	if (Pts.IsValidIndex(FbIdx) && Pts[FbIdx].bValid)
	{
		OutPoint = Pts[FbIdx].Loc;
		return true;
	}

	return false;
}

// --------- ULNPSurfaceCacheSubsystem ---------

void ULNPSurfaceCacheSubsystem::BeginBaking()
{
	if (bIsBaking)
		return;

	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	const float CellSpacing = FMath::Max(1.0f, Settings->SurfaceCacheCellSpacing);
	SphereRadius = Settings->SphereRadius;

	// 적도의 목표 호 길이 간격으로부터 격자 해상도 유도
	LatResolution = FMath::Max(1, FMath::RoundToInt(PI * SphereRadius / CellSpacing));
	LonResolution = FMath::Max(1, FMath::RoundToInt(2.0f * PI * SphereRadius / CellSpacing));

	TotalSamples = LatResolution * LonResolution;

	// 베이킹 사이클마다 새 할당 — 이전 매치의 라이브 Snapshot은
	// TSharedPtr를 계속 소유하며 해제될 때까지 유효하다.
	CacheData = MakeShared<TArray<FPoint>>();
	CacheData->SetNum(TotalSamples);
	CompletedCount = 0;
	bBakingComplete = false;
	bIsBaking = true;

	TraceDelegate.BindUObject(this, &ULNPSurfaceCacheSubsystem::OnAsyncTraceComplete);

	UWorld* World = GetWorld();
	FCollisionQueryParams Params(NAME_None, false);

	// 모든 트레이스를 한 번에 발사. 엔진이 결과마다 OnAsyncTraceComplete를 호출하며 Sample Index는 UserData로 넘김.
	for (int32 i = 0; i < TotalSamples; ++i)
	{
		const int32 LatIdx = i / LonResolution;
		const int32 LonIdx = i % LonResolution;
		const FVector Dir = IndexToDirection(LatIdx, LonIdx, LatResolution, LonResolution);
		const FVector Start = Dir * (SphereRadius * 0.5f);
		const FVector End   = Dir * (SphereRadius * 1.5f);
		World->AsyncLineTraceByChannel(EAsyncTraceType::Single, Start, End, ECC_WorldStatic, Params,
			FCollisionResponseParams::DefaultResponseParam, &TraceDelegate, static_cast<uint32>(i));
	}

	UE_LOG(LogLootNPop, Log, TEXT("LNPSurfaceCacheSubsystem: Fired %d async traces (%dx%d) at %.1f cm spacing."),
		TotalSamples, LatResolution, LonResolution, CellSpacing);
}

void ULNPSurfaceCacheSubsystem::Tick(float DeltaTime)
{
	// GetBakingProgress() 함수를 위해 bIsBaking 동안 IsTickable()을 유지
}

void ULNPSurfaceCacheSubsystem::OnAsyncTraceComplete(const FTraceHandle& Handle, FTraceDatum& Data)
{
	const int32 Index = static_cast<int32>(Data.UserData);
	if (CacheData->IsValidIndex(Index))
	{
		if (Data.OutHits.Num() > 0)
		{
			(*CacheData)[Index].Loc    = Data.OutHits[0].ImpactPoint;
			(*CacheData)[Index].bValid = true;
		}
		else
		{
			const int32 LatIdx = Index / LonResolution;
			const int32 LonIdx = Index % LonResolution;
			UE_LOG(LogLootNPop, Warning, TEXT("LNPSurfaceCacheSubsystem: Sample %d (%d, %d) missed (no hit). Check sphere geometry."), Index, LatIdx, LonIdx);
		}
	}

	if (++CompletedCount >= TotalSamples)
	{
		bIsBaking = false;
		bBakingComplete = true;
		TraceDelegate.Unbind();
		UE_LOG(LogLootNPop, Log, TEXT("LNPSurfaceCacheSubsystem: Baking complete (%d samples)."), TotalSamples);
		OnBakingComplete.Broadcast();
	}
}

TStatId ULNPSurfaceCacheSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPSurfaceCacheSubsystem, STATGROUP_Tickables);
}

bool ULNPSurfaceCacheSubsystem::GetSurfacePoint(const FVector& WorldDirection, FVector& OutPoint) const
{
	if (!bBakingComplete || !CacheData.IsValid())
		return false;

	const TArray<FPoint>& Pts = *CacheData;
	const FVector Dir = WorldDirection.GetSafeNormal();

	const float Lat = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.0f, 1.0f)));
	float Lon = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	if (Lon < 0.0f)
		Lon += 360.0f;

	// 분수 Index: 0.0 = 셀 0의 중심, 1.0 = 셀 1의 중심
	const float LatFrac = (Lat + 90.0f) / 180.0f * LatResolution - 0.5f;
	const float LonFrac = Lon / 360.0f * LonResolution - 0.5f;

	const int32 LatLo = FMath::Clamp(FMath::FloorToInt(LatFrac), 0, LatResolution - 1);
	const int32 LatHi = FMath::Min(LatLo + 1, LatResolution - 1);
	const int32 LonLo = ((FMath::FloorToInt(LonFrac) % LonResolution) + LonResolution) % LonResolution;
	const int32 LonHi = (LonLo + 1) % LonResolution;

	const float tLat = LatFrac - FMath::FloorToInt(LatFrac);
	const float tLon = LonFrac - FMath::FloorToInt(LonFrac);

	auto SafeGet = [&](int32 La, int32 Lo) -> const FVector*
	{
		const int32 Idx = La * LonResolution + Lo;
		return (Pts.IsValidIndex(Idx) && Pts[Idx].bValid) ? &Pts[Idx].Loc : nullptr;
	};

	const FVector* P00 = SafeGet(LatLo, LonLo);
	const FVector* P01 = SafeGet(LatLo, LonHi);
	const FVector* P10 = SafeGet(LatHi, LonLo);
	const FVector* P11 = SafeGet(LatHi, LonHi);

	if (P00 && P01 && P10 && P11)
	{
		OutPoint = FMath::Lerp(
			FMath::Lerp(*P00, *P01, tLon),
			FMath::Lerp(*P10, *P11, tLon),
			tLat);
		return true;
	}

	// 인접 셀이 유효하지 않으면 nearest-neighbor로 Fallback (예: 극지)
	// 극지방은 셀이 매우 촘촘하므로 nearest-neighbor로도 계단 현상이 거의 없음
	const int32 LatNearest = FMath::Clamp(FMath::RoundToInt(LatFrac), 0, LatResolution - 1);
	const int32 LonNearest = ((FMath::RoundToInt(LonFrac) % LonResolution) + LonResolution) % LonResolution;
	const int32 FallbackIdx = LatNearest * LonResolution + LonNearest;
	if (Pts.IsValidIndex(FallbackIdx) && Pts[FallbackIdx].bValid)
	{
		OutPoint = Pts[FallbackIdx].Loc;
		return true;
	}

	return false;
}

FLNPSurfaceCacheSnapshot ULNPSurfaceCacheSubsystem::TakeSnapshot() const
{
	FLNPSurfaceCacheSnapshot Snap;
	Snap.Points = CacheData;  // 원자적 참조 횟수 +1, 데이터 복사 없음
	Snap.LatRes = LatResolution;
	Snap.LonRes = LonResolution;
	return Snap;
}

float ULNPSurfaceCacheSubsystem::GetBakingProgress() const
{
	if (bBakingComplete)
		return 1.0f;
	if (TotalSamples == 0)
		return 0.0f;
	return (float)CompletedCount / (float)TotalSamples;
}

// static
FVector ULNPSurfaceCacheSubsystem::IndexToDirection(int32 LatIdx, int32 LonIdx, int32 LatRes, int32 LonRes)
{
	// 셀 중심 사용: 각 셀 내에서 0.5 오프셋
	const float Lat = ((LatIdx + 0.5f) / LatRes) * 180.0f - 90.0f;
	const float Lon = ((LonIdx + 0.5f) / LonRes) * 360.0f;

	const float LatRad = FMath::DegreesToRadians(Lat);
	const float LonRad = FMath::DegreesToRadians(Lon);
	const float CosLat = FMath::Cos(LatRad);

	return FVector(CosLat * FMath::Cos(LonRad), CosLat * FMath::Sin(LonRad), FMath::Sin(LatRad));
}

// static
void ULNPSurfaceCacheSubsystem::DirectionToIndex(const FVector& Dir, int32 LatRes, int32 LonRes, int32& OutLatIdx, int32& OutLonIdx)
{
	const float Lat = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.0f, 1.0f)));
	float Lon = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	if (Lon < 0.0f)
		Lon += 360.0f;

	OutLatIdx = FMath::Clamp(FMath::FloorToInt((Lat + 90.0f) / 180.0f * LatRes), 0, LatRes - 1);
	OutLonIdx = FMath::Clamp(FMath::FloorToInt(Lon / 360.0f * LonRes), 0, LonRes - 1);
}
