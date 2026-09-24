// Copyright (c) 2026 LootNPop. All rights reserved.

#include "HitDetection/LNPProjectileMotion.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

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
}

bool LNPProjectileMotion::TraceWorld(const ULNPMassWorldCollisionSubsystem& WorldCollision, const FVector& From, const FVector& To,
	FLNPWorldHit& OutHit)
{
	return WorldCollision.RaycastWorld(From, To, FLNPWorldQueryParams(ELNPWorldQueryClass::ProjectileMandatory), OutHit);
}

void LNPProjectileMotion::PredictArc(const ULNPMassWorldCollisionSubsystem& WorldCollision,
	const FVector& Start, const FVector& Velocity, const float GravityAccel,
	const float MaxSeconds, TArray<FVector>& OutPoints)
{
	// 실탄은 프레임마다 PreviousPos→현재 위치를 같은 함수로 검사한다. 스텝 선분 안에서 포물선을 직선으로
	// 보는 오차만 남고, 이분 탐색 없이 hit 지점이 곧 착탄점이다.
	const float EnvelopeRadius = WorldCollision.GetWorldEnvelopeRadius();
	SimulateArc(Start, Velocity, GravityAccel, MaxSeconds, OutPoints,
		[&WorldCollision, EnvelopeRadius](const FVector& PrevPos, const FVector& Pos, FVector& OutImpact)
		{
			FLNPWorldHit Hit;
			if (TraceWorld(WorldCollision, PrevPos, Pos, Hit))
			{
				OutImpact = Hit.ImpactPoint;
				return true;
			}
			if (IsOutsideWorldEnvelope(EnvelopeRadius, Pos))
			{
				OutImpact = Pos;
				return true;
			}
			return false;
		});
}
