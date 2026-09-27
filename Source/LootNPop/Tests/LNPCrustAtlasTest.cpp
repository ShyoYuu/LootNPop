// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "SurfaceNavigation/LNPCrustAtlas.h"
#include "SurfaceNavigation/LNPSurfaceBakeGeometry.h"

namespace LNPCrustAtlasTest
{
	constexpr double Radius = 30000.0;
	const FVector3d HoleDirection = FVector3d(1.0, 1.0, 1.0).GetSafeNormal();

	/**
	 * octahedral 격자 N으로 삼각분할한 (+X,+Y,+Z) 지각. RadiusOf가 방향별 반지름을, Keep이 삼각형 중심 방향별 유지 여부를 정한다.
	 * 앞면 법선은 구 중심 쪽이다.
	 */
	FLNPBakeTriangleMesh MakeCrust(
		int32 N,
		TFunctionRef<double(const FVector3d&)> RadiusOf,
		TFunctionRef<bool(const FVector3d&)> Keep)
	{
		FLNPBakeTriangleMesh Mesh;
		TArray<int32> Index;
		Index.SetNumUninitialized((N + 1) * (N + 1));
		for (int32 J = 0; J <= N; ++J)
		{
			for (int32 I = 0; I + J <= N; ++I)
			{
				const FVector3d Direction = FVector3d(I, J, N - I - J).GetSafeNormal();
				Index[J * (N + 1) + I] = Mesh.Vertices.Add(Direction * RadiusOf(Direction));
			}
		}
		auto Add = [&Mesh, &Keep](int32 A, int32 B, int32 C)
		{
			const FVector3d Centroid = (Mesh.Vertices[A] + Mesh.Vertices[B] + Mesh.Vertices[C]) / 3.0;
			if (!Keep(Centroid.GetSafeNormal()))
			{
				return;
			}
			const FVector3d Normal = FVector3d::CrossProduct(Mesh.Vertices[B] - Mesh.Vertices[A], Mesh.Vertices[C] - Mesh.Vertices[A]);
			if (FVector3d::DotProduct(Normal, Centroid) < 0.0)
			{
				Mesh.Triangles.Emplace(A, B, C);
			}
			else
			{
				Mesh.Triangles.Emplace(A, C, B);
			}
		};
		for (int32 J = 0; J < N; ++J)
		{
			for (int32 I = 0; I + J < N; ++I)
			{
				Add(Index[J * (N + 1) + I], Index[J * (N + 1) + I + 1], Index[(J + 1) * (N + 1) + I]);
				if (I + J < N - 1)
				{
					Add(Index[J * (N + 1) + I + 1], Index[(J + 1) * (N + 1) + I + 1], Index[(J + 1) * (N + 1) + I]);
				}
			}
		}
		return Mesh;
	}

	FLNPBakeTriangleMesh MakeSphereCrust(int32 N)
	{
		return MakeCrust(N, [](const FVector3d&) { return Radius; }, [](const FVector3d&) { return true; });
	}

	/** Center 방향에 반지름 CapRadius의 작은 부채꼴 캡을 Mesh에 덧붙인다. bFacingCenter면 앞면이 구 중심 쪽이다. */
	void AppendCap(FLNPBakeTriangleMesh& Mesh, const FVector3d& Center, double CapRadius, double AngleDeg, bool bFacingCenter)
	{
		constexpr int32 Segments = 16;
		FVector3d TangentA, TangentB;
		Center.FindBestAxisVectors(TangentA, TangentB);
		const int32 CenterIndex = Mesh.Vertices.Add(Center * CapRadius);
		const double Angle = FMath::DegreesToRadians(AngleDeg);
		for (int32 Segment = 0; Segment < Segments; ++Segment)
		{
			const double Phi = UE_TWO_PI * Segment / Segments;
			const FVector3d Direction = Center * FMath::Cos(Angle)
				+ (TangentA * FMath::Cos(Phi) + TangentB * FMath::Sin(Phi)) * FMath::Sin(Angle);
			Mesh.Vertices.Add(Direction * CapRadius);
		}
		for (int32 Segment = 0; Segment < Segments; ++Segment)
		{
			const int32 A = CenterIndex + 1 + Segment;
			const int32 B = CenterIndex + 1 + (Segment + 1) % Segments;
			const FVector3d Normal = FVector3d::CrossProduct(Mesh.Vertices[A] - Mesh.Vertices[CenterIndex], Mesh.Vertices[B] - Mesh.Vertices[CenterIndex]);
			const bool bFacesCenter = FVector3d::DotProduct(Normal, Center) < 0.0;
			if (bFacesCenter == bFacingCenter)
			{
				Mesh.Triangles.Emplace(CenterIndex, A, B);
			}
			else
			{
				Mesh.Triangles.Emplace(CenterIndex, B, A);
			}
		}
	}

	bool HasFlags(const FLNPCrustSample& Sample, ELNPSupportSampleFlags Flags)
	{
		return EnumHasAllFlags(Sample.Flags, Flags);
	}

	FLNPCrustRasterSettings MakeSettings(int32 Subdivisions)
	{
		FLNPCrustRasterSettings Settings;
		Settings.Subdivisions = Subdivisions;
		return Settings;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPCrustAtlasGridTest,
	"LootNPop.SurfaceNavigation.Bake.CrustAtlasGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPCrustAtlasGridTest::RunTest(const FString& Parameters)
{
	constexpr int32 N = 7;
	TestEqual(TEXT("Sample count is (N+1)(N+2)/2"), LNPCrustAtlas::GetSampleCount(N), 36);

	int32 Expected = 0;
	bool bRowMajor = true;
	for (int32 J = 0; J <= N; ++J)
	{
		for (int32 I = 0; I + J <= N; ++I)
		{
			bRowMajor &= LNPCrustAtlas::GetSampleIndex(N, I, J) == Expected++;
		}
	}
	TestTrue(TEXT("Index is j-row-major with i increasing and covers every sample once"), bRowMajor);

	TestEqual(TEXT("(0,0) is the +Z corner"), LNPCrustAtlas::GetSampleDirection(N, 0, 0), FVector3d::ZAxisVector);
	TestEqual(TEXT("(N,0) is the +X corner"), LNPCrustAtlas::GetSampleDirection(N, N, 0), FVector3d::XAxisVector);
	TestEqual(TEXT("(0,N) is the +Y corner"), LNPCrustAtlas::GetSampleDirection(N, 0, N), FVector3d::YAxisVector);

	bool bSeamExact = true;
	for (int32 Step = 0; Step <= N; ++Step)
	{
		bSeamExact &= LNPCrustAtlas::GetSampleDirection(N, Step, N - Step).Z == 0.0;
		bSeamExact &= LNPCrustAtlas::GetSampleDirection(N, 0, Step).X == 0.0;
		bSeamExact &= LNPCrustAtlas::GetSampleDirection(N, Step, 0).Y == 0.0;
	}
	TestTrue(TEXT("Seam-edge samples lie exactly on their seam plane"), bSeamExact);

	TestEqual(TEXT("100cm center spacing at 30,000cm needs N=735"),
		LNPCrustAtlas::ComputeSubdivisionsForSpacing(30000.0, 100.0), 735);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPCrustAtlasSphereTest,
	"LootNPop.SurfaceNavigation.Bake.CrustAtlasSphere",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPCrustAtlasSphereTest::RunTest(const FString& Parameters)
{
	using namespace LNPCrustAtlasTest;
	FString Error;

	// Atlas 격자가 mesh 정점과 겹치면 모든 샘플이 정점 위라 반지름이 R과 같다. 정점을 공유한 삼각형 교차는 하나로 합쳐져야 한다.
	{
		constexpr int32 N = 40;
		FLNPCrustAtlasRaster Raster;
		if (!TestTrue(TEXT("Vertex-aligned sphere crust rasterizes"),
			LNPCrustAtlas::Rasterize(MakeSphereCrust(N), MakeSettings(N), Raster, Error)))
		{
			AddError(Error);
			return false;
		}
		double MaxError = 0.0;
		int32 NotClean = 0;
		for (const FLNPCrustSample& Sample : Raster.Samples)
		{
			MaxError = FMath::Max(MaxError, FMath::Abs(Sample.Radius - Radius));
			NotClean += HasFlags(Sample, ELNPSupportSampleFlags::Valid | ELNPSupportSampleFlags::Walkable)
				&& !HasFlags(Sample, ELNPSupportSampleFlags::NeedsExact) ? 0 : 1;
		}
		AddInfo(FString::Printf(TEXT("Vertex-aligned sphere: samples=%d maxRadiusError=%.6fcm"), Raster.Samples.Num(), MaxError));
		TestTrue(TEXT("Vertex-aligned samples hit radius R"), MaxError <= 1e-3);
		TestEqual(TEXT("Every sample is valid, walkable and interpolable"), NotClean, 0);
	}

	// 일반 격자는 chord 안쪽에 떨어진다. 오차는 셀 각도의 sag 한계 안이다.
	{
		constexpr int32 MeshN = 40;
		constexpr int32 AtlasN = 37;
		FLNPCrustAtlasRaster Raster;
		if (!TestTrue(TEXT("Misaligned sphere crust rasterizes"),
			LNPCrustAtlas::Rasterize(MakeSphereCrust(MeshN), MakeSettings(AtlasN), Raster, Error)))
		{
			AddError(Error);
			return false;
		}
		const double SagBound = Radius * (1.0 - FMath::Cos(UE_DOUBLE_SQRT_3 * UE_DOUBLE_SQRT_2 / MeshN));
		double MaxSag = 0.0;
		double MaxNormalAngle = 0.0;
		int32 NotClean = 0;
		for (int32 J = 0; J <= AtlasN; ++J)
		{
			for (int32 I = 0; I + J <= AtlasN; ++I)
			{
				const FLNPCrustSample& Sample = Raster.Samples[LNPCrustAtlas::GetSampleIndex(AtlasN, I, J)];
				MaxSag = FMath::Max(MaxSag, Radius - Sample.Radius);
				const double Dot = FVector3d::DotProduct(FVector3d(Sample.Normal), -LNPCrustAtlas::GetSampleDirection(AtlasN, I, J));
				MaxNormalAngle = FMath::Max(MaxNormalAngle, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0, 1.0))));
				NotClean += HasFlags(Sample, ELNPSupportSampleFlags::Valid | ELNPSupportSampleFlags::Walkable)
					&& !HasFlags(Sample, ELNPSupportSampleFlags::NeedsExact) ? 0 : 1;
			}
		}
		AddInfo(FString::Printf(TEXT("Misaligned sphere: maxSag=%.2fcm (bound %.2fcm) maxNormalAngle=%.2fdeg"),
			MaxSag, SagBound, MaxNormalAngle));
		TestTrue(TEXT("Samples never lie outside the sphere"), MaxSag >= -1e-3);
		TestTrue(TEXT("Chord sag stays inside the cell bound"), MaxSag <= SagBound);
		TestEqual(TEXT("Smooth sphere has no NeedsExact sample"), NotClean, 0);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPCrustAtlasSeamSnapTest,
	"LootNPop.SurfaceNavigation.Bake.CrustAtlasSeamSnap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPCrustAtlasSeamSnapTest::RunTest(const FString& Parameters)
{
	using namespace LNPCrustAtlasTest;
	constexpr int32 MeshN = 40;
	constexpr int32 AtlasN = 37;

	// 경계 정점을 부동소수점 잡음만큼 옥탄트 안쪽으로 민다(`Meadow_00` 실측과 같은 크기).
	FLNPBakeTriangleMesh Crust = MakeSphereCrust(MeshN);
	for (FVector3d& Vertex : Crust.Vertices)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Vertex[Axis] = Vertex[Axis] == 0.0 ? 5e-7 : Vertex[Axis];
		}
	}

	auto CountInvalidSeamSamples = [AtlasN](const FLNPCrustAtlasRaster& Raster)
	{
		int32 Invalid = 0;
		for (int32 Edge = 0; Edge < 3; ++Edge)
		{
			for (int32 Step = 0; Step <= AtlasN; ++Step)
			{
				const FIntPoint Coord = LNPCrustAtlas::GetSeamSampleCoord(AtlasN, static_cast<ELNPCrustSeamEdge>(Edge), Step);
				Invalid += HasFlags(Raster.Samples[LNPCrustAtlas::GetSampleIndex(AtlasN, Coord.X, Coord.Y)], ELNPSupportSampleFlags::Valid) ? 0 : 1;
			}
		}
		return Invalid;
	};

	FString Error;
	FLNPCrustRasterSettings Unsnapped = MakeSettings(AtlasN);
	Unsnapped.SeamSnapDistance = 0.0;
	FLNPCrustAtlasRaster Raster;
	if (!LNPCrustAtlas::Rasterize(Crust, Unsnapped, Raster, Error))
	{
		AddError(Error);
		return false;
	}
	const int32 UnsnappedInvalid = CountInvalidSeamSamples(Raster);

	if (!LNPCrustAtlas::Rasterize(Crust, MakeSettings(AtlasN), Raster, Error))
	{
		AddError(Error);
		return false;
	}
	const int32 SnappedInvalid = CountInvalidSeamSamples(Raster);

	AddInfo(FString::Printf(TEXT("Seam samples invalid: unsnapped=%d snapped=%d of %d"), UnsnappedInvalid, SnappedInvalid, 3 * (AtlasN + 1)));
	TestTrue(TEXT("Without snapping, seam rays graze past inward-shifted boundary edges"), UnsnappedInvalid > 0);
	TestEqual(TEXT("With snapping, every seam sample is valid"), SnappedInvalid, 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPCrustAtlasHoleTest,
	"LootNPop.SurfaceNavigation.Bake.CrustAtlasHole",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPCrustAtlasHoleTest::RunTest(const FString& Parameters)
{
	using namespace LNPCrustAtlasTest;
	constexpr int32 MeshN = 48;
	constexpr int32 AtlasN = 40;
	constexpr double HoleAngleDeg = 6.0;
	const double HoleCos = FMath::Cos(FMath::DegreesToRadians(HoleAngleDeg));
	const FLNPBakeTriangleMesh Crust = MakeCrust(
		MeshN,
		[](const FVector3d&) { return Radius; },
		[HoleCos](const FVector3d& Direction) { return FVector3d::DotProduct(Direction, HoleDirection) <= HoleCos; });

	FLNPCrustAtlasRaster Raster;
	FString Error;
	if (!TestTrue(TEXT("Holed crust rasterizes"), LNPCrustAtlas::Rasterize(Crust, MakeSettings(AtlasN), Raster, Error)))
	{
		AddError(Error);
		return false;
	}

	// mesh 셀 하나(중심 간격)만큼 안쪽은 삼각형 중심 판정의 톱니와 무관하게 확실히 구멍이다.
	const double MeshCellDeg = FMath::RadiansToDegrees(UE_DOUBLE_SQRT_3 * UE_DOUBLE_SQRT_2 / MeshN);
	const double AtlasCellDeg = FMath::RadiansToDegrees(UE_DOUBLE_SQRT_3 * UE_DOUBLE_SQRT_2 / AtlasN);
	int32 InvalidCount = 0;
	int32 GhostFloorCount = 0;
	int32 UnguardedEdgeCount = 0;
	int32 FarNeedsExactCount = 0;
	for (int32 J = 0; J <= AtlasN; ++J)
	{
		for (int32 I = 0; I + J <= AtlasN; ++I)
		{
			const FLNPCrustSample& Sample = Raster.Samples[LNPCrustAtlas::GetSampleIndex(AtlasN, I, J)];
			const double AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
				FVector3d::DotProduct(LNPCrustAtlas::GetSampleDirection(AtlasN, I, J), HoleDirection), -1.0, 1.0)));
			const bool bValid = HasFlags(Sample, ELNPSupportSampleFlags::Valid);
			InvalidCount += bValid ? 0 : 1;
			GhostFloorCount += bValid && AngleDeg < HoleAngleDeg - MeshCellDeg ? 1 : 0;
			FarNeedsExactCount += AngleDeg > HoleAngleDeg + MeshCellDeg + 2.0 * AtlasCellDeg
				&& HasFlags(Sample, ELNPSupportSampleFlags::NeedsExact) ? 1 : 0;
			if (!bValid)
			{
				continue;
			}
			// 구멍 둘레: invalid 이웃을 가진 valid 샘플은 반드시 NeedsExact다.
			constexpr int32 Offsets[6][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, -1}, {-1, 1}};
			for (const auto& Offset : Offsets)
			{
				const int32 NI = I + Offset[0];
				const int32 NJ = J + Offset[1];
				if (NI >= 0 && NJ >= 0 && NI + NJ <= AtlasN
					&& !HasFlags(Raster.Samples[LNPCrustAtlas::GetSampleIndex(AtlasN, NI, NJ)], ELNPSupportSampleFlags::Valid)
					&& !HasFlags(Sample, ELNPSupportSampleFlags::NeedsExact))
				{
					++UnguardedEdgeCount;
					break;
				}
			}
		}
	}
	AddInfo(FString::Printf(TEXT("Hole: invalid=%d ghostFloor=%d unguardedEdge=%d farNeedsExact=%d"),
		InvalidCount, GhostFloorCount, UnguardedEdgeCount, FarNeedsExactCount));
	TestTrue(TEXT("Hole produces invalid samples"), InvalidCount > 0);
	TestEqual(TEXT("No valid ghost floor inside the hole"), GhostFloorCount, 0);
	TestEqual(TEXT("Every valid sample next to the hole needs exact"), UnguardedEdgeCount, 0);
	TestEqual(TEXT("Samples away from the hole stay interpolable"), FarNeedsExactCount, 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPCrustAtlasOverhangTest,
	"LootNPop.SurfaceNavigation.Bake.CrustAtlasOverhang",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPCrustAtlasOverhangTest::RunTest(const FString& Parameters)
{
	using namespace LNPCrustAtlasTest;
	constexpr int32 N = 40;
	FString Error;
	FLNPCrustAtlasRaster Raster;

	// 지각 안쪽에 중심을 향한 바닥이 하나 더 있으면 한 방향에 바닥이 둘이다.
	FLNPBakeTriangleMesh Overhang = MakeSphereCrust(N);
	AppendCap(Overhang, HoleDirection, Radius - 500.0, 3.0, /*bFacingCenter=*/true);
	TestFalse(TEXT("Second front-facing floor is a bake error"),
		LNPCrustAtlas::Rasterize(Overhang, MakeSettings(N), Raster, Error));
	TestTrue(TEXT("Error names the overhang"), Error.Contains(TEXT("overhang")));
	TestEqual(TEXT("Failed bake returns no samples"), Raster.Samples.Num(), 0);

	// 뒷면만 보이는 판은 exact 단순 trimesh 쿼리처럼 무시한다. 지각 반지름이 그대로 남아야 한다.
	FLNPBakeTriangleMesh BackFacing = MakeSphereCrust(N);
	AppendCap(BackFacing, HoleDirection, Radius - 500.0, 3.0, /*bFacingCenter=*/false);
	if (!TestTrue(TEXT("Back-facing geometry is ignored"),
		LNPCrustAtlas::Rasterize(BackFacing, MakeSettings(N), Raster, Error)))
	{
		AddError(Error);
		return false;
	}
	double MinRadius = TNumericLimits<double>::Max();
	for (const FLNPCrustSample& Sample : Raster.Samples)
	{
		MinRadius = FMath::Min(MinRadius, Sample.Radius);
	}
	TestTrue(TEXT("No sample lands on the back-facing plate"), MinRadius > Radius - 100.0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPCrustAtlasCliffTest,
	"LootNPop.SurfaceNavigation.Bake.CrustAtlasCliff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPCrustAtlasCliffTest::RunTest(const FString& Parameters)
{
	using namespace LNPCrustAtlasTest;
	// 벽이 실제로 가파라야 한다. mesh 셀 약 574cm에 단차 1,000cm면 벽 경사가 약 60도다.
	// 격자 간격보다 완만한 단차는 Atlas 입장에서 걸을 수 있는 비탈이므로 절벽 사례가 되지 않는다.
	constexpr int32 MeshN = 128;
	constexpr int32 AtlasN = 100;
	constexpr double CliffX = 0.55;
	constexpr double CliffHeight = 1000.0;
	const FLNPBakeTriangleMesh Crust = MakeCrust(
		MeshN,
		[](const FVector3d& Direction) { return Direction.X < CliffX ? Radius : Radius - CliffHeight; },
		[](const FVector3d&) { return true; });

	FLNPCrustAtlasRaster Raster;
	FString Error;
	if (!TestTrue(TEXT("Cliff crust rasterizes"), LNPCrustAtlas::Rasterize(Crust, MakeSettings(AtlasN), Raster, Error)))
	{
		AddError(Error);
		return false;
	}

	int32 UnflaggedJumpCount = 0;
	int32 FarNeedsExactCount = 0;
	int32 SteepCount = 0;
	constexpr int32 Offsets[6][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, -1}, {-1, 1}};
	for (int32 J = 0; J <= AtlasN; ++J)
	{
		for (int32 I = 0; I + J <= AtlasN; ++I)
		{
			const FLNPCrustSample& Sample = Raster.Samples[LNPCrustAtlas::GetSampleIndex(AtlasN, I, J)];
			const bool bNeedsExact = HasFlags(Sample, ELNPSupportSampleFlags::NeedsExact);
			SteepCount += HasFlags(Sample, ELNPSupportSampleFlags::Valid) && !HasFlags(Sample, ELNPSupportSampleFlags::Walkable) ? 1 : 0;
			const double X = LNPCrustAtlas::GetSampleDirection(AtlasN, I, J).X;
			FarNeedsExactCount += FMath::Abs(X - CliffX) > 0.1 && bNeedsExact ? 1 : 0;
			for (const auto& Offset : Offsets)
			{
				const int32 NI = I + Offset[0];
				const int32 NJ = J + Offset[1];
				if (NI >= 0 && NJ >= 0 && NI + NJ <= AtlasN && !bNeedsExact
					&& FMath::Abs(Raster.Samples[LNPCrustAtlas::GetSampleIndex(AtlasN, NI, NJ)].Radius - Sample.Radius) > 0.5 * CliffHeight)
				{
					++UnflaggedJumpCount;
				}
			}
		}
	}
	AddInfo(FString::Printf(TEXT("Cliff: steepSamples=%d unflaggedJump=%d farNeedsExact=%d"),
		SteepCount, UnflaggedJumpCount, FarNeedsExactCount));
	TestTrue(TEXT("Rays that hit the cliff wall are not walkable"), SteepCount > 0);
	TestEqual(TEXT("Both samples across a cliff need exact"), UnflaggedJumpCount, 0);
	TestEqual(TEXT("Flat terraces away from the cliff stay interpolable"), FarNeedsExactCount, 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPCrustAtlasCodecTest,
	"LootNPop.SurfaceNavigation.Bake.CrustAtlasCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPCrustAtlasCodecTest::RunTest(const FString& Parameters)
{
	using namespace LNPCrustAtlasTest;
	constexpr int32 MeshN = 64;
	constexpr int32 AtlasN = 50;
	const FLNPBakeTriangleMesh Crust = MakeCrust(
		MeshN,
		[](const FVector3d& Direction) { return Radius + 300.0 * FMath::Sin(5.0 * Direction.X) * FMath::Cos(7.0 * Direction.Y); },
		[](const FVector3d&) { return true; });

	FLNPCrustAtlasRaster Raster;
	FString Error;
	if (!TestTrue(TEXT("Rolling crust rasterizes"), LNPCrustAtlas::Rasterize(Crust, MakeSettings(AtlasN), Raster, Error)))
	{
		AddError(Error);
		return false;
	}

	FLNPCrustCodecSettings Codec;
	Codec.BaseRadius = Radius;
	TArray<uint8> Payload;
	if (!TestTrue(TEXT("Raster encodes"), LNPCrustAtlas::Encode(Raster, Codec, Payload, Error)))
	{
		AddError(Error);
		return false;
	}
	FLNPCrustAtlas Atlas;
	if (!TestTrue(TEXT("Payload decodes"), LNPCrustAtlas::Decode(Payload, Atlas, Error)))
	{
		AddError(Error);
		return false;
	}

	TestEqual(TEXT("Decoded subdivisions"), Atlas.Subdivisions, AtlasN);
	TestEqual(TEXT("Decoded sample count"), Atlas.Num(), Raster.Samples.Num());
	double MaxRadiusError = 0.0;
	double MaxNormalAngle = 0.0;
	int32 FlagMismatch = 0;
	for (int32 Index = 0; Index < Raster.Samples.Num(); ++Index)
	{
		const FLNPCrustSample& Sample = Raster.Samples[Index];
		FlagMismatch += Atlas.GetFlags(Index) == Sample.Flags ? 0 : 1;
		if (!HasFlags(Sample, ELNPSupportSampleFlags::Valid))
		{
			continue;
		}
		MaxRadiusError = FMath::Max(MaxRadiusError, FMath::Abs(Atlas.GetRadius(Index) - Sample.Radius));
		const double Dot = FVector3f::DotProduct(Atlas.GetNormal(Index), Sample.Normal);
		MaxNormalAngle = FMath::Max(MaxNormalAngle, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0, 1.0))));
	}
	AddInfo(FString::Printf(TEXT("Codec: payload=%d bytes maxRadiusError=%.4fcm maxNormalAngle=%.5fdeg"),
		Payload.Num(), MaxRadiusError, MaxNormalAngle));
	TestEqual(TEXT("Flags survive the codec"), FlagMismatch, 0);
	TestTrue(TEXT("Radius error is at most half a step"), MaxRadiusError <= Codec.RadiusStep * 0.5 + 1e-9);
	TestTrue(TEXT("Normal error is below 0.05 degree"), MaxNormalAngle < 0.05);

	// 법선 인코딩은 아래 반구와 축 방향도 복원한다.
	for (const FVector3f Normal : {FVector3f(0.0f, 0.0f, -1.0f), FVector3f(1.0f, 0.0f, 0.0f), FVector3f(-0.3f, 0.5f, -0.8f).GetSafeNormal()})
	{
		int16 X, Y;
		LNPCrustAtlas::EncodeNormal(Normal, X, Y);
		TestTrue(FString::Printf(TEXT("Normal %s round-trips"), *Normal.ToString()),
			FVector3f::DotProduct(LNPCrustAtlas::DecodeNormal(X, Y), Normal) > 0.99999f);
	}

	TestFalse(TEXT("Truncated payload is rejected"),
		LNPCrustAtlas::Decode(MakeArrayView(Payload.GetData(), Payload.Num() - 1), Atlas, Error));

	FLNPCrustCodecSettings TinyStep = Codec;
	TinyStep.RadiusStep = 0.001;
	TArray<uint8> Rejected;
	TestFalse(TEXT("Radius offset beyond the int16 range is an encoding error"),
		LNPCrustAtlas::Encode(Raster, TinyStep, Rejected, Error));

	// 단일 대칭 이음매 프로필(구면)이면 세 변의 hash가 같다.
	FLNPCrustAtlasRaster SphereRaster;
	TArray<uint8> SpherePayload;
	FLNPCrustAtlas SphereAtlas;
	if (LNPCrustAtlas::Rasterize(MakeSphereCrust(AtlasN), MakeSettings(AtlasN), SphereRaster, Error)
		&& LNPCrustAtlas::Encode(SphereRaster, Codec, SpherePayload, Error)
		&& LNPCrustAtlas::Decode(SpherePayload, SphereAtlas, Error))
	{
		TestTrue(TEXT("Sphere seam hash x=0 equals y=0"), SphereAtlas.SeamHashes[0] == SphereAtlas.SeamHashes[1]);
		TestTrue(TEXT("Sphere seam hash y=0 equals z=0"), SphereAtlas.SeamHashes[1] == SphereAtlas.SeamHashes[2]);
	}
	else
	{
		AddError(Error);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPCrustAtlasQueryTest,
	"LootNPop.SurfaceNavigation.Bake.CrustAtlasQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPCrustAtlasQueryTest::RunTest(const FString& Parameters)
{
	using namespace LNPCrustAtlasTest;
	FString Error;
	FLNPCrustCodecSettings Codec;
	Codec.BaseRadius = Radius;

	auto BuildAtlas = [this, &Codec, &Error](const FLNPBakeTriangleMesh& Crust, int32 AtlasN, FLNPCrustAtlas& OutAtlas)
	{
		FLNPCrustAtlasRaster Raster;
		TArray<uint8> Payload;
		const bool bBuilt = LNPCrustAtlas::Rasterize(Crust, MakeSettings(AtlasN), Raster, Error)
			&& LNPCrustAtlas::Encode(Raster, Codec, Payload, Error)
			&& LNPCrustAtlas::Decode(Payload, OutAtlas, Error);
		if (!bBuilt)
		{
			AddError(Error);
		}
		return bBuilt;
	};

	// 정점 정렬 구면: 모든 방향이 보간되고 반지름은 R, 법선은 중심 방향이다.
	{
		constexpr int32 N = 40;
		FLNPCrustAtlas Atlas;
		if (!BuildAtlas(MakeSphereCrust(N), N, Atlas))
		{
			return false;
		}
		FRandomStream Random(4242);
		int32 Unsupported = 0;
		double MaxRadiusError = 0.0;
		double MaxNormalAngle = 0.0;
		for (int32 Sample = 0; Sample < 5000; ++Sample)
		{
			const FVector3d Direction = Random.GetUnitVector().GetAbs();
			FLNPCrustSupportHit Hit;
			if (!LNPCrustAtlas::QuerySupport(Atlas, Direction, Hit))
			{
				++Unsupported;
				continue;
			}
			MaxRadiusError = FMath::Max(MaxRadiusError, FMath::Abs(Hit.Radius - Radius));
			const double Dot = FVector3d::DotProduct(FVector3d(Hit.Normal), -Direction);
			MaxNormalAngle = FMath::Max(MaxNormalAngle, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0, 1.0))));
		}
		AddInfo(FString::Printf(TEXT("Sphere query: unsupported=%d maxRadiusError=%.6fcm maxNormalAngle=%.3fdeg"),
			Unsupported, MaxRadiusError, MaxNormalAngle));
		TestEqual(TEXT("Every sphere direction interpolates"), Unsupported, 0);
		TestTrue(TEXT("Interpolated radius is R"), MaxRadiusError <= 1e-6);
		TestTrue(TEXT("Interpolated normal stays within one cell angle of the center direction"),
			MaxNormalAngle <= FMath::RadiansToDegrees(UE_DOUBLE_SQRT_3 * UE_DOUBLE_SQRT_2 / N));

		// 이음매·꼭짓점 위 방향과 회전 오차 수준의 음수 성분은 받아들이고, 다른 옥탄트 방향은 거부한다.
		FLNPCrustSupportHit Hit;
		for (const FVector3d Direction : {
			FVector3d::XAxisVector, FVector3d::YAxisVector, FVector3d::ZAxisVector,
			FVector3d(1.0, 1.0, 0.0).GetSafeNormal(), FVector3d(0.0, 1.0, 1.0).GetSafeNormal(), FVector3d(1.0, 0.0, 1.0).GetSafeNormal(),
			FVector3d(-1e-12, 1.0, 1.0).GetSafeNormal()})
		{
			TestTrue(FString::Printf(TEXT("Seam direction %s interpolates"), *Direction.ToString()),
				LNPCrustAtlas::QuerySupport(Atlas, Direction, Hit) && FMath::Abs(Hit.Radius - Radius) <= 1e-6);
		}
		TestFalse(TEXT("Direction in another octant is rejected"),
			LNPCrustAtlas::QuerySupport(Atlas, FVector3d(-0.1, 1.0, 1.0).GetSafeNormal(), Hit));
	}

	// 구멍: 구멍 안은 지면을 연장하지 않고, 멀리 떨어진 곳은 보간된다.
	{
		constexpr int32 MeshN = 48;
		constexpr int32 AtlasN = 40;
		constexpr double HoleAngleDeg = 6.0;
		const double HoleCos = FMath::Cos(FMath::DegreesToRadians(HoleAngleDeg));
		FLNPCrustAtlas Atlas;
		if (!BuildAtlas(MakeCrust(
				MeshN,
				[](const FVector3d&) { return Radius; },
				[HoleCos](const FVector3d& Direction) { return FVector3d::DotProduct(Direction, HoleDirection) <= HoleCos; }),
			AtlasN, Atlas))
		{
			return false;
		}

		const double MeshCellDeg = FMath::RadiansToDegrees(UE_DOUBLE_SQRT_3 * UE_DOUBLE_SQRT_2 / MeshN);
		const double AtlasCellDeg = FMath::RadiansToDegrees(UE_DOUBLE_SQRT_3 * UE_DOUBLE_SQRT_2 / AtlasN);
		FRandomStream Random(777);
		int32 GhostFloor = 0;
		int32 InsideCount = 0;
		int32 FarUnsupported = 0;
		int32 FarCount = 0;
		for (int32 Sample = 0; Sample < 20000; ++Sample)
		{
			const FVector3d Direction = Random.GetUnitVector().GetAbs();
			const double AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
				FVector3d::DotProduct(Direction, HoleDirection), -1.0, 1.0)));
			FLNPCrustSupportHit Hit;
			const bool bSupported = LNPCrustAtlas::QuerySupport(Atlas, Direction, Hit);
			if (AngleDeg < HoleAngleDeg - MeshCellDeg)
			{
				++InsideCount;
				GhostFloor += bSupported ? 1 : 0;
			}
			else if (AngleDeg > HoleAngleDeg + MeshCellDeg + 3.0 * AtlasCellDeg)
			{
				++FarCount;
				FarUnsupported += bSupported ? 0 : 1;
			}
		}
		AddInfo(FString::Printf(TEXT("Hole query: inside=%d ghostFloor=%d far=%d farUnsupported=%d"),
			InsideCount, GhostFloor, FarCount, FarUnsupported));
		TestTrue(TEXT("Random directions land inside the hole"), InsideCount > 0);
		TestEqual(TEXT("No ghost floor inside the hole"), GhostFloor, 0);
		TestEqual(TEXT("Directions away from the hole interpolate"), FarUnsupported, 0);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPCrustSeamPairsTest,
	"LootNPop.SurfaceNavigation.Bake.CrustSeamPairs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPCrustSeamPairsTest::RunTest(const FString& Parameters)
{
	constexpr ELNPCrustSeamEdge X0 = ELNPCrustSeamEdge::X0;
	constexpr ELNPCrustSeamEdge Y0 = ELNPCrustSeamEdge::Y0;
	constexpr ELNPCrustSeamEdge Z0 = ELNPCrustSeamEdge::Z0;
	// `ULNPOctantSpawnSubsystem::OctantRotations`로 손 계산한 대응표. 회전 집합을 바꾸면 이 표도 다시 계산한다.
	// x=0·y=0 변은 서로 정순으로, z=0 변은 z=0 변과 역순으로 만난다.
	const FLNPCrustSeamPair Expected[] = {
		{{0, X0}, {1, Y0}, false},
		{{0, Y0}, {3, X0}, false},
		{{0, Z0}, {7, Z0}, true},
		{{1, X0}, {2, Y0}, false},
		{{1, Z0}, {4, Z0}, true},
		{{2, X0}, {3, Y0}, false},
		{{2, Z0}, {5, Z0}, true},
		{{3, Z0}, {6, Z0}, true},
		{{4, X0}, {7, Y0}, false},
		{{4, Y0}, {5, X0}, false},
		{{5, Y0}, {6, X0}, false},
		{{6, Y0}, {7, X0}, false},
	};

	TArray<FLNPCrustSeamPair> Pairs;
	FString Error;
	if (!TestTrue(TEXT("Seam pairs compute for the slot rotations"),
		LNPCrustAtlas::ComputeSeamPairs(ULNPOctantSpawnSubsystem::OctantRotations, Pairs, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestEqual(TEXT("12 world seams"), Pairs.Num(), static_cast<int32>(UE_ARRAY_COUNT(Expected))))
	{
		return false;
	}
	for (int32 Index = 0; Index < Pairs.Num(); ++Index)
	{
		const FLNPCrustSeamPair& Actual = Pairs[Index];
		const FLNPCrustSeamPair& Want = Expected[Index];
		TestTrue(FString::Printf(TEXT("Seam pair %d is slot %d edge %d <-> slot %d edge %d reversed=%d"), Index,
			Want.A.Slot, static_cast<int32>(Want.A.Edge), Want.B.Slot, static_cast<int32>(Want.B.Edge), Want.bReversed),
			Actual.A.Slot == Want.A.Slot && Actual.A.Edge == Want.A.Edge
			&& Actual.B.Slot == Want.B.Slot && Actual.B.Edge == Want.B.Edge && Actual.bReversed == Want.bReversed);
	}

	// 대응표가 실제로 같은 방향의 샘플을 짝짓는지 확인한다.
	constexpr int32 N = 7;
	for (const FLNPCrustSeamPair& Pair : Pairs)
	{
		for (int32 Step = 0; Step <= N; ++Step)
		{
			const FIntPoint CoordA = LNPCrustAtlas::GetSeamSampleCoord(N, Pair.A.Edge, Step);
			const FIntPoint CoordB = LNPCrustAtlas::GetSeamSampleCoord(N, Pair.B.Edge, Pair.bReversed ? N - Step : Step);
			const FVector3d WorldA = ULNPOctantSpawnSubsystem::OctantRotations[Pair.A.Slot].RotateVector(
				LNPCrustAtlas::GetSampleDirection(N, CoordA.X, CoordA.Y));
			const FVector3d WorldB = ULNPOctantSpawnSubsystem::OctantRotations[Pair.B.Slot].RotateVector(
				LNPCrustAtlas::GetSampleDirection(N, CoordB.X, CoordB.Y));
			// 회전은 FMath::SinCos 다항 근사라 180° slot에서 1e-8 수준 오차가 난다. 기준은 §3.7의 1e-6이다.
			if (!WorldA.Equals(WorldB, 1e-6))
			{
				AddError(FString::Printf(TEXT("Slot %d edge %d step %d does not meet slot %d: %s vs %s"),
					Pair.A.Slot, static_cast<int32>(Pair.A.Edge), Step, Pair.B.Slot, *WorldA.ToString(), *WorldB.ToString()));
			}
		}
	}

	// 같은 월드 변에 인스턴스가 둘이 아니면 오류다. slot 하나면 모든 변이 짝을 잃는다.
	const FRotator SingleSlot[] = {FRotator::ZeroRotator};
	TestFalse(TEXT("Unpaired seams are rejected"), LNPCrustAtlas::ComputeSeamPairs(SingleSlot, Pairs, Error));
	const FRotator TripleSlot[] = {FRotator::ZeroRotator, FRotator::ZeroRotator, FRotator::ZeroRotator};
	TestFalse(TEXT("Seams shared by three instances are rejected"), LNPCrustAtlas::ComputeSeamPairs(TripleSlot, Pairs, Error));
	return !HasAnyErrors();
}

#endif
