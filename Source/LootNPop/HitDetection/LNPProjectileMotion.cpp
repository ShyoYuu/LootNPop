// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPProjectileMotion.h"
#include "GameLogic/LNPSurfaceCacheSubsystem.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

#include "HAL/IConsoleManager.h"

namespace
{
	/** 호 길이를 따라 고르게 ArcPointCount개로 재표집한다. 양 끝(총구·착탄점)은 그대로 보존한다. */
	void ResampleByArcLength(const TArray<FVector>& Raw, TArray<FVector>& Out)
	{
		using namespace LNPProjectileMotion;

		Out.Reset(ArcPointCount);

		if (Raw.Num() <= 1)
		{
			const FVector Only = Raw.IsEmpty() ? FVector::ZeroVector : Raw[0];
			for (int32 i = 0; i < ArcPointCount; ++i)
				Out.Add(Only);
			return;
		}

		// 누적 호 길이
		TArray<float> Cumulative;
		Cumulative.Reserve(Raw.Num());
		Cumulative.Add(0.f);
		for (int32 i = 1; i < Raw.Num(); ++i)
			Cumulative.Add(Cumulative[i - 1] + static_cast<float>(FVector::Dist(Raw[i - 1], Raw[i])));

		const float TotalLength = Cumulative.Last();
		if (TotalLength <= KINDA_SMALL_NUMBER)
		{
			for (int32 i = 0; i < ArcPointCount; ++i)
				Out.Add(Raw[0]);
			return;
		}

		int32 Seg = 0;
		for (int32 i = 0; i < ArcPointCount; ++i)
		{
			const float Target = TotalLength * i / static_cast<float>(ArcPointCount - 1);
			while (Seg + 2 < Raw.Num() && Cumulative[Seg + 1] < Target)
				++Seg;

			const float SegLength = Cumulative[Seg + 1] - Cumulative[Seg];
			const float Alpha     = (SegLength > KINDA_SMALL_NUMBER) ? (Target - Cumulative[Seg]) / SegLength : 0.f;
			Out.Add(FMath::Lerp(Raw[Seg], Raw[Seg + 1], FMath::Clamp(Alpha, 0.f, 1.f)));
		}
	}

	/**
	 * 궤적을 지면에 닿거나 수명이 다할 때까지 적분하고 ArcPointCount개로 재표집한다.
	 * HitSegment(PrevPos, Pos, OutImpact)가 참이면 OutImpact를 마지막 점으로 두고 멈춘다.
	 * 지면에 닿지 않고 수명이 다한 경우 실제 발사체도 그 자리에서 폭발하므로 끝점을 그대로 쓴다.
	 */
	void SimulateArc(const FVector& Start, const FVector& Velocity, const float GravityAccel, const float MaxSeconds,
		TArray<FVector>& OutPoints, TFunctionRef<bool(const FVector&, const FVector&, FVector&)> HitSegment)
	{
		using namespace LNPProjectileMotion;

		TArray<FVector> Raw;
		Raw.Reserve(MaxArcSimSteps + 1);
		Raw.Add(Start);

		FVector Pos     = Start;
		FVector Vel     = Velocity;
		float   Elapsed = 0.f;

		for (int32 SimStep = 0; SimStep < MaxArcSimSteps && Elapsed < MaxSeconds; ++SimStep)
		{
			const float   Dt      = FMath::Min(ArcStepSeconds, MaxSeconds - Elapsed);
			const FVector PrevPos = Pos;

			Step(Pos, Vel, GravityAccel, Dt);
			Elapsed += Dt;

			FVector Impact;
			if (HitSegment(PrevPos, Pos, Impact))
			{
				Raw.Add(Impact);
				break;
			}

			Raw.Add(Pos);
		}

		ResampleByArcLength(Raw, OutPoints);
	}

	int32 GProjectileExactWorldCollision = 0;
	FAutoConsoleVariableRef CVarProjectileExactWorldCollision(
		TEXT("LNP.SurfaceNav.ProjectileExact"), GProjectileExactWorldCollision,
		TEXT("0 = legacy projectile ground impact (SurfaceCache IsUnderSurface, default). ")
		TEXT("1 = exact LNPWorldExact line trace of PreviousPos->CurrentPos for server projectiles, client ghosts and the ADS arc guide."));
}

bool LNPProjectileMotion::IsUnderSurface(const ULNPSurfaceCacheSubsystem& SurfaceCache, const FVector& Pos)
{
	FVector SurfacePoint;
	return SurfaceCache.GetSurfacePoint(Pos.GetSafeNormal(), SurfacePoint)
		&& Pos.SizeSquared() >= SurfacePoint.SizeSquared();
}

bool LNPProjectileMotion::PredictArc(const ULNPSurfaceCacheSubsystem& SurfaceCache,
	const FVector& Start, const FVector& Velocity, const float GravityAccel,
	const float MaxSeconds, TArray<FVector>& OutPoints)
{
	// 베이킹 미완이면 조회가 통째로 false다. 이때 궤적을 그리면 지면을 무시하고 수명 끝까지
	// 뻗어 나가므로, 아예 실패로 돌려 호출자가 가이드를 숨기게 한다.
	FVector Probe;
	if (!SurfaceCache.GetSurfacePoint(Start.GetSafeNormal(), Probe))
		return false;

	SimulateArc(Start, Velocity, GravityAccel, MaxSeconds, OutPoints,
		[&SurfaceCache](const FVector& PrevPos, const FVector& Pos, FVector& OutImpact)
		{
			if (!IsUnderSurface(SurfaceCache, Pos))
				return false;

			// 스텝 하나(33ms)를 이분 탐색으로 좁힌다. 조회가 O(1)이라 반복 비용은 무시할 수 있고,
			// 한 스텝 구간에서는 포물선을 직선으로 봐도 오차가 표면 격자 간격(200cm)보다 작다.
			FVector Above = PrevPos;
			FVector Below = Pos;
			for (int32 Iter = 0; Iter < 4; ++Iter)
			{
				const FVector Mid = (Above + Below) * 0.5f;
				(IsUnderSurface(SurfaceCache, Mid) ? Below : Above) = Mid;
			}
			OutImpact = Below;
			return true;
		});
	return true;
}

bool LNPProjectileMotion::UseExactWorldCollision()
{
	return GProjectileExactWorldCollision != 0;
}

bool LNPProjectileMotion::TraceWorld(const ULNPMassWorldCollisionSubsystem& WorldCollision, const FVector& From, const FVector& To,
	FLNPWorldHit& OutHit)
{
	return WorldCollision.RaycastWorld(From, To, FLNPWorldQueryParams(ELNPWorldQueryClass::ProjectileMandatory), OutHit);
}

void LNPProjectileMotion::PredictArcExact(const ULNPMassWorldCollisionSubsystem& WorldCollision,
	const FVector& Start, const FVector& Velocity, const float GravityAccel,
	const float MaxSeconds, TArray<FVector>& OutPoints)
{
	// 실탄은 프레임마다 PreviousPos→현재 위치를 같은 함수로 검사한다. 스텝 선분 안에서 포물선을 직선으로
	// 보는 오차만 남고, 이분 탐색 없이 hit 지점이 곧 착탄점이다.
	SimulateArc(Start, Velocity, GravityAccel, MaxSeconds, OutPoints,
		[&WorldCollision](const FVector& PrevPos, const FVector& Pos, FVector& OutImpact)
		{
			FLNPWorldHit Hit;
			if (!TraceWorld(WorldCollision, PrevPos, Pos, Hit))
				return false;
			OutImpact = Hit.ImpactPoint;
			return true;
		});
}
