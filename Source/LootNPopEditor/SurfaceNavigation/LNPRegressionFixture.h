// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurfaceNavigation/LNPFixtureMesh.h"

/**
 * 정적 회귀 fixture LVI의 사례 배치(`design/RegressionMap.md` §3). 빌더(`LNP.SurfaceNav.BuildRegressionFixture`)와
 * 자동화 `WorldCollision.RegressionFixture`가 이 값을 공유한다. 좌표는 source 옥탄트(+X,+Y,+Z) 로컬이고,
 * 모든 사례는 위도 25~40°에 두어 이음매·꼭짓점과 int16 캡에서 떨어뜨린다.
 */
namespace LNPRegressionFixture
{
	const TCHAR* const FixtureFolder = TEXT("/Game/Maps/SurfaceNavigation/Fixtures");
	const TCHAR* const LevelName = TEXT("LVI_Octant_Fixture_Regression");
	const TCHAR* const LevelPath =
		TEXT("/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Regression.LVI_Octant_Fixture_Regression");
	const TCHAR* const CrustMeshName = TEXT("SM_FixtureRegressionCrust_R30000");
	const TCHAR* const BoxTopMeshName = TEXT("SM_FixtureBoxTop");
	const TCHAR* const BoxBodyMeshName = TEXT("SM_FixtureBoxBody");

	/** 이 배치가 가정하는 기준 지각 반지름(D-046). 설정값이 다르면 빌더가 거부한다. */
	constexpr double CrustRadius = 30000.0;
	/** 완전 구면 지각의 격자 분할 수. 중심 간격 약 287cm, 평면 삼각형의 반지름 오차는 0.4cm 이하다. */
	constexpr int32 CrustSubdivisions = 256;
	/** 섬 판의 두께(cm). */
	constexpr double IslandThickness = 300.0;

	/** 사례 하나의 위치 틀. Tangent는 방위 증가, Bitangent는 위도 증가 방향이다. */
	struct FCaseFrame
	{
		FVector Radial;
		FVector Tangent;
		FVector Bitangent;

		FCaseFrame(double LatDeg, double AzDeg)
		{
			const double Lat = FMath::DegreesToRadians(LatDeg);
			const double Az = FMath::DegreesToRadians(AzDeg);
			Radial = LNPFixtureMesh::DirectionFromLatAz(LatDeg, AzDeg);
			Tangent = FVector(-FMath::Sin(Az), FMath::Cos(Az), 0.0);
			Bitangent = FVector(-FMath::Sin(Lat) * FMath::Cos(Az), -FMath::Sin(Lat) * FMath::Sin(Az), FMath::Cos(Lat));
		}

		FVector At(double Radius, double TangentOffset = 0.0, double BitangentOffset = 0.0) const
		{
			return Radial * Radius + Tangent * TangentOffset + Bitangent * BitangentOffset;
		}
		double RadiusOf(const FVector& Point) const { return FVector::DotProduct(Point, Radial); }
		double TangentOf(const FVector& Point) const { return FVector::DotProduct(Point, Tangent); }

		/** 로컬 +Z = Up(중심 방향), +Y = Tangent. */
		FQuat UpRotation() const { return FRotationMatrix::MakeFromZY(-Radial, Tangent).ToQuat(); }
	};

	/** 사례 위도·방위(도). */
	inline FCaseFrame BasicCrust() { return FCaseFrame(30.0, 8.0); }
	inline FCaseFrame IslandOne() { return FCaseFrame(32.0, 18.0); }
	inline FCaseFrame IslandTwo() { return FCaseFrame(32.0, 30.0); }
	inline FCaseFrame IslandEdge() { return FCaseFrame(32.0, 42.0); }
	/** 공동 중심 방향. 통로는 위도 증가 쪽으로 오르며 약 위도 34°에서 지각에 입구를 낸다. */
	inline FCaseFrame Cave() { return FCaseFrame(28.0, 56.0); }
	inline FCaseFrame StaticProps() { return FCaseFrame(32.0, 72.0); }

	/** 섬 윗면 반지름과 크기(위도 방향 × 방위 방향, cm). */
	constexpr double IslandOneTop = 28000.0;
	constexpr double IslandOneSize = 1300.0;
	constexpr double IslandTwoOuterTop = 28200.0;
	constexpr double IslandTwoOuterSize = 1400.0;
	constexpr double IslandTwoInnerTop = 26400.0;
	constexpr double IslandTwoInnerSize = 1100.0;
	constexpr double IslandEdgeTop = 28000.0;
	constexpr double IslandEdgeLength = 1600.0;
	constexpr double IslandEdgeWidth = 1200.0;

	/** 정적 프랍: 나무 원기둥(지름 180, 높이 700)과 바위(비균일 구)의 접선 위치. */
	constexpr double TreeTangent = -350.0;
	constexpr double TreeDiameter = 180.0;
	constexpr double TreeHeight = 700.0;
	constexpr double RockTangent = 350.0;
	constexpr double DecorationTangent = 700.0;
}
