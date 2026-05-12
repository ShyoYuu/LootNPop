// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "Config/LNPSettings.h"
#include "Engine/World.h"

void ULNPSurfaceCacheSubsystem::BeginBaking()
{
	if (bIsBaking || bBakingComplete)
		return;

	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	const float CellSpacing = FMath::Max(1.0f, Settings->SurfaceCacheCellSpacing);
	TracesPerFrame = FMath::Max(1, Settings->SurfaceCacheTracesPerFrame);
	SphereRadius = Settings->SphereRadius;

	// Derive grid resolution from target arc-length spacing at the equator
	LatResolution = FMath::Max(1, FMath::RoundToInt(PI * SphereRadius / CellSpacing));
	LonResolution = FMath::Max(1, FMath::RoundToInt(2.0f * PI * SphereRadius / CellSpacing));

	TotalSamples = LatResolution * LonResolution;
	Cache.SetNum(TotalSamples);
	NextSampleIndex = 0;
	bIsBaking = true;

	UE_LOG(LogTemp, Log, TEXT("LNPSurfaceCacheSubsystem: Baking %d samples (%dx%d) at %.1f cm spacing, %d traces/frame."),
		TotalSamples, LatResolution, LonResolution, CellSpacing, TracesPerFrame);
}

void ULNPSurfaceCacheSubsystem::Tick(float DeltaTime)
{
	if (false == bIsBaking)
		return;

	UWorld* World = GetWorld();
	const int32 EndIndex = FMath::Min(NextSampleIndex + TracesPerFrame, TotalSamples);
	FCollisionQueryParams Params(NAME_None, false);

	while (NextSampleIndex < EndIndex)
	{
		const int32 LatIdx = NextSampleIndex / LonResolution;
		const int32 LonIdx = NextSampleIndex % LonResolution;
		const FVector Dir = IndexToDirection(LatIdx, LonIdx, LatResolution, LonResolution);

		// Shoot from inside the sphere outward to hit the inner wall
		const FVector Start = Dir * (SphereRadius * 0.5f);
		const FVector End   = Dir * (SphereRadius * 1.5f);

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			Cache[NextSampleIndex].SurfacePoint = Hit.ImpactPoint;
			Cache[NextSampleIndex].bValid = true;
		}

		++NextSampleIndex;
	}

	if (NextSampleIndex >= TotalSamples)
	{
		bIsBaking = false;
		bBakingComplete = true;
		UE_LOG(LogTemp, Log, TEXT("LNPSurfaceCacheSubsystem: Baking complete."));
		OnBakingComplete.Broadcast();
	}
}

TStatId ULNPSurfaceCacheSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULNPSurfaceCacheSubsystem, STATGROUP_Tickables);
}

bool ULNPSurfaceCacheSubsystem::GetSurfacePoint(const FVector& WorldDirection, FVector& OutPoint) const
{
	if (false == bBakingComplete)
		return false;

	const FVector Dir = WorldDirection.GetSafeNormal();

	const float Lat = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.0f, 1.0f)));
	float Lon = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	if (Lon < 0.0f)
		Lon += 360.0f;

	// Fractional indices: 0.0 = center of cell 0, 1.0 = center of cell 1
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
		return (Cache.IsValidIndex(Idx) && Cache[Idx].bValid) ? &Cache[Idx].SurfacePoint : nullptr;
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

	// Fallback to nearest-neighbor if any neighbor is invalid (e.g. at poles)
	const int32 LatNearest = FMath::Clamp(FMath::RoundToInt(LatFrac), 0, LatResolution - 1);
	const int32 LonNearest = ((FMath::RoundToInt(LonFrac) % LonResolution) + LonResolution) % LonResolution;
	const int32 FallbackIdx = LatNearest * LonResolution + LonNearest;
	if (Cache.IsValidIndex(FallbackIdx) && Cache[FallbackIdx].bValid)
	{
		OutPoint = Cache[FallbackIdx].SurfacePoint;
		return true;
	}

	return false;
}

float ULNPSurfaceCacheSubsystem::GetBakingProgress() const
{
	if (bBakingComplete)
		return 1.0f;
	if (TotalSamples == 0)
		return 0.0f;
	return (float)NextSampleIndex / (float)TotalSamples;
}

// static
FVector ULNPSurfaceCacheSubsystem::IndexToDirection(int32 LatIdx, int32 LonIdx, int32 LatRes, int32 LonRes)
{
	// Use cell centers: offset by 0.5 within each cell
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
