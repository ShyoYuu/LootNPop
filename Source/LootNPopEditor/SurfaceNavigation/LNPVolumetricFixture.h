// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "SurfaceNavigation/LNPRegressionFixture.h"

/** Phase 7c 입체 회귀의 고정 좌표. 로컬 +X는 위도 증가, +Y는 방위 증가, +Z는 구 중심 방향이다. */
namespace LNPVolumetricFixture
{
	constexpr double FloorRadius = 33500.0;
	constexpr double RoomHalf = 2200.0;
	constexpr double RoomHeight = 3200.0;
	constexpr double DoorY = 1200.0;
	constexpr double PassageWidth = 600.0;
	constexpr double PassageHeight = 400.0;
	constexpr double RampLength = 8000.0;
	constexpr double UpperHeight = 1200.0;
	constexpr double ShaftY = -5400.0;
	constexpr double ShaftHalf = 700.0;
	constexpr double ShaftOpeningHalf = 300.0;
	constexpr double LowHeight = 350.0;
	constexpr int32 SpiralSegments = 104;
	constexpr double SpiralTurns = 1.3;
	constexpr double SpiralInner = 450.0;
	constexpr double SpiralOuter = 850.0;

	inline LNPRegressionFixture::FCaseFrame Frame() { return {35.264389682754654, 45.0}; }
	inline FTransform Transform() { return FTransform(Frame().UpRotation(), Frame().Radial * FloorRadius); }
	inline double FloorZ(double X, double Y)
	{
		return FloorRadius - FMath::Sqrt(FloorRadius * FloorRadius - X * X - Y * Y);
	}
	inline FVector FloorPoint(double X, double Y, double Height = 0.0) { return {X, Y, FloorZ(X, Y) + Height}; }
	inline double RampRise()
	{
		const double X = RoomHalf + RampLength;
		return FloorRadius - FloorZ(RoomHalf, DoorY)
			- FMath::Sqrt(LNPRegressionFixture::CrustRadius * LNPRegressionFixture::CrustRadius - X * X - DoorY * DoorY);
	}
	inline FVector RampPoint(double Distance, double Y)
	{
		return {RoomHalf + Distance, Y, FloorZ(RoomHalf, DoorY) + RampRise() * Distance / RampLength};
	}
	inline FVector SpiralPoint(int32 Segment, double Radius)
	{
		const double T = static_cast<double>(Segment) / SpiralSegments;
		const double Angle = UE_TWO_PI * SpiralTurns * T;
		return FloorPoint(-700.0 + Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), UpperHeight * T);
	}
}
