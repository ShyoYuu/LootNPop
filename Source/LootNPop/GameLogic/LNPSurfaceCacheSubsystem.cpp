// Copyright (c) 2026 LootNPop. All rights reserved.

#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "Config/LNPSettings.h"
#include "LootNPop.h"

#include "Engine/World.h"

// --------- 공용 격자 샘플링 ---------

namespace
{
	using FPoint = FLNPSurfaceCacheSnapshot::FPoint;

	/**
	 * 등장방형(Equirectangular) 격자에서 주어진 World 방향의 표면 지점을 조회하는 공용 구현.
	 * FLNPSurfaceCacheSnapshot::GetPoint(임의 Thread)와 ULNPSurfaceCacheSubsystem::GetSurfacePoint(게임 Thread)가
	 * 동일한 로직을 공유한다. 읽기 전용 배열만 참조하므로 Thread-Safe.
	 *
	 * 1) 방향 벡터를 위도-경도 분수 Index로 변환 (0.0 = 셀 0의 중심, 1.0 = 셀 1의 중심)
	 * 2) 주변 4개 셀을 바이리니어 보간 — 셀 경계의 불연속 제거
	 * 3) 이웃 셀이 하나라도 무효하면 nearest-neighbor로 Fallback (주로 극지 부근).
	 *    극지방은 셀이 매우 촘촘하므로 nearest-neighbor로도 계단 현상이 거의 없다.
	 */
	bool SampleSurfaceGrid(const TArray<FPoint>& Pts, int32 LatRes, int32 LonRes, const FVector& WorldDirection, FVector& OutPoint)
	{
		if (Pts.IsEmpty() || LatRes <= 0 || LonRes <= 0)
			return false;

		const FVector Dir = WorldDirection.GetSafeNormal();
		const float Lat = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.0f, 1.0f)));
		float Lon = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		if (Lon < 0.0f)
			Lon += 360.0f;

		const float LatFrac = (Lat + 90.0f) / 180.0f * LatRes - 0.5f;
		const float LonFrac = Lon / 360.0f * LonRes - 0.5f;

		// 위도는 극에서 Clamp, 경도는 0°/360° 경계에서 Wrap
		const int32 LatLo = FMath::Clamp(FMath::FloorToInt(LatFrac), 0, LatRes - 1);
		const int32 LatHi = FMath::Min(LatLo + 1, LatRes - 1);
		const int32 LonLo = ((FMath::FloorToInt(LonFrac) % LonRes) + LonRes) % LonRes;
		const int32 LonHi = (LonLo + 1) % LonRes;

		const float tLat = LatFrac - FMath::FloorToInt(LatFrac);
		const float tLon = LonFrac - FMath::FloorToInt(LonFrac);

		auto SafeGet = [&Pts, LonRes](int32 La, int32 Lo) -> const FVector*
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

		// 최근접 이웃 Fallback
		const int32 LatNearest = FMath::Clamp(FMath::RoundToInt(LatFrac), 0, LatRes - 1);
		const int32 LonNearest = ((FMath::RoundToInt(LonFrac) % LonRes) + LonRes) % LonRes;
		const int32 FallbackIdx = LatNearest * LonRes + LonNearest;
		if (Pts.IsValidIndex(FallbackIdx) && Pts[FallbackIdx].bValid)
		{
			OutPoint = Pts[FallbackIdx].Loc;
			return true;
		}

		return false;
	}
}

// --------- FLNPSurfaceCacheSnapshot ---------

bool FLNPSurfaceCacheSnapshot::GetPoint(const FVector& WorldDirection, FVector& OutPoint) const
{
	if (!Points.IsValid())
		return false;
	return SampleSurfaceGrid(*Points, LatRes, LonRes, WorldDirection, OutPoint);
}

// --------- ULNPSurfaceCacheSubsystem ---------

void ULNPSurfaceCacheSubsystem::BeginBaking()
{
	// 완료 후 재호출도 차단한다. 베이킹은 머신당 1회이며, 이 가드가 없으면 Mass 워커 Thread가
	// CacheData를 읽는 도중 게임 Thread가 SharedPtr를 재대입해 참조 카운트 조작이 깨진다.
	// 향후 매치 재시작으로 재베이킹이 필요해지면, 여기를 푸는 대신 모든 워커 접근이 멈춘 것을
	// 보장하는 명시적 리셋 진입점을 따로 만들 것.
	if (bIsBaking || bBakingComplete.load(std::memory_order_acquire))
		return;

	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	const float CellSpacing = FMath::Max(1.0f, Settings->SurfaceCacheCellSpacing);
	SphereRadius = Settings->SphereRadius;
	SamplesPerFrame = FMath::Max(1, Settings->SurfaceCacheSamplesPerFrame);

	// 적도의 목표 호 길이 간격으로부터 격자 해상도 유도
	LatResolution = FMath::Max(1, FMath::RoundToInt(PI * SphereRadius / CellSpacing));
	LonResolution = FMath::Max(1, FMath::RoundToInt(2.0f * PI * SphereRadius / CellSpacing));

	TotalSamples = LatResolution * LonResolution;

	// 베이킹 사이클마다 새 할당 — 이전 매치의 라이브 Snapshot은
	// TSharedPtr를 계속 소유하며 해제될 때까지 유효하다.
	CacheData = MakeShared<TArray<FPoint>>();
	CacheData->SetNum(TotalSamples);
	CompletedCount = 0;
	NextSampleToIssue = 0;
	bBakingComplete.store(false, std::memory_order_relaxed);
	bIsBaking = true;

	TraceDelegate.BindUObject(this, &ULNPSurfaceCacheSubsystem::OnAsyncTraceComplete);

	// 실제 발사는 Tick이 나눠서 수행한다. 여기서 전량을 쏘면 다음 프레임의 UWorld::ResetAsyncTrace가
	// WaitForAllAsyncTraceTasks로 게임 Thread를 막고 전체 Callback을 한 번에 쏟아내 큰 히치가 된다.
	UE_LOG(LogLootNPop, Log, TEXT("LNPSurfaceCacheSubsystem: Baking started - %d samples (%dx%d) at %.1f cm spacing, %d per frame."),
		TotalSamples, LatResolution, LonResolution, CellSpacing, SamplesPerFrame);
}

void ULNPSurfaceCacheSubsystem::IssuePendingTraces(int32 Count)
{
	UWorld* World = GetWorld();
	if (nullptr == World)
		return;

	const FCollisionQueryParams Params(NAME_None, false);
	const int32 EndIndex = FMath::Min(NextSampleToIssue + Count, TotalSamples);

	// 엔진이 결과마다 OnAsyncTraceComplete를 호출하며 Sample Index는 UserData로 넘김.
	for (int32 i = NextSampleToIssue; i < EndIndex; ++i)
	{
		const int32 LatIdx = i / LonResolution;
		const int32 LonIdx = i % LonResolution;
		const FVector Dir = IndexToDirection(LatIdx, LonIdx, LatResolution, LonResolution);
		const FVector Start = Dir * (SphereRadius * 0.5f);
		const FVector End   = Dir * (SphereRadius * 1.5f);
		World->AsyncLineTraceByChannel(EAsyncTraceType::Single, Start, End, ECC_WorldStatic, Params,
			FCollisionResponseParams::DefaultResponseParam, &TraceDelegate, static_cast<uint32>(i));
	}

	NextSampleToIssue = EndIndex;
}

void ULNPSurfaceCacheSubsystem::Tick(float DeltaTime)
{
	// FTickableGameObject의 Tick은 UWorld::Tick 안에서 ResetAsyncTrace와 FinishAsyncTrace 사이에 돈다.
	// 즉 비동기 트레이스 요청이 허용된(bAsyncAllowed) 구간이므로 여기서 발사해도 안전하다.
	// 발사가 끝난 뒤에도 CompletedCount가 채워질 때까지 bIsBaking이 유지되어 IsTickable()이 살아 있는다.
	if (bIsBaking && NextSampleToIssue < TotalSamples)
	{
		IssuePendingTraces(SamplesPerFrame);
	}
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

		// ★ release 게시 — 이 store 이전의 배열 쓰기가 전부, 이 플래그를 acquire로 읽어 true를 본
		//   워커 Thread에게 반드시 보인다. 락 없이 스레드 안전이 성립하는 지점이 정확히 여기다.
		bBakingComplete.store(true, std::memory_order_release);

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
	// 베이킹 중에는 격자가 채워지는 중이므로 조회를 허용하지 않는다.
	// acquire 로드 — true를 봤다면 베이킹이 채운 배열 내용도 전부 보인다 (OnAsyncTraceComplete의 release와 짝).
	if (!bBakingComplete.load(std::memory_order_acquire) || !CacheData.IsValid())
		return false;
	return SampleSurfaceGrid(*CacheData, LatResolution, LonResolution, WorldDirection, OutPoint);
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
	if (bBakingComplete.load(std::memory_order_acquire))
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
