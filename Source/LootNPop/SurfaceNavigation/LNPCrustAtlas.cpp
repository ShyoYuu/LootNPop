// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPCrustAtlas.h"

#include "Async/ParallelFor.h"
#include "IndexTypes.h"
#include "Intersection/IntrRay3Triangle3.h"
#include "Spatial/MeshAABBTree3.h"
#include "SurfaceNavigation/LNPSurfaceBakeGeometry.h"

namespace
{
/** TMeshAABBTree3가 요구하는 최소 mesh 인터페이스. 베이크 동안 mesh는 바뀌지 않는다. */
struct FBakeMeshAdapter
{
	const FLNPBakeTriangleMesh* Mesh = nullptr;

	bool IsTriangle(int32 Index) const { return Mesh->Triangles.IsValidIndex(Index); }
	int32 MaxTriangleID() const { return Mesh->Triangles.Num(); }
	int32 TriangleCount() const { return Mesh->Triangles.Num(); }
	uint64 GetChangeStamp() const { return 1; }
	FVector3d GetVertex(int32 Index) const { return Mesh->Vertices[Index]; }

	UE::Geometry::FIndex3i GetTriangle(int32 Index) const
	{
		const FIntVector3& Triangle = Mesh->Triangles[Index];
		return UE::Geometry::FIndex3i(Triangle.X, Triangle.Y, Triangle.Z);
	}

	void GetTriVertices(int32 Index, FVector3d& A, FVector3d& B, FVector3d& C) const
	{
		const FIntVector3& Triangle = Mesh->Triangles[Index];
		A = Mesh->Vertices[Triangle.X];
		B = Mesh->Vertices[Triangle.Y];
		C = Mesh->Vertices[Triangle.Z];
	}
};

/** 삼각 격자의 6-이웃 (di, dj). */
constexpr int32 NeighborOffsets[6][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, -1}, {-1, 1}};
}

int32 LNPCrustAtlas::GetSampleCount(int32 Subdivisions)
{
	return (Subdivisions + 1) * (Subdivisions + 2) / 2;
}

int32 LNPCrustAtlas::GetSampleIndex(int32 Subdivisions, int32 I, int32 J)
{
	return J * (Subdivisions + 1) - J * (J - 1) / 2 + I;
}

FVector3d LNPCrustAtlas::GetSampleDirection(int32 Subdivisions, int32 I, int32 J)
{
	return FVector3d(I, J, Subdivisions - I - J).GetSafeNormal();
}

int32 LNPCrustAtlas::ComputeSubdivisionsForSpacing(double Radius, double Spacing)
{
	return FMath::CeilToInt32(Radius * UE_DOUBLE_SQRT_3 * UE_DOUBLE_SQRT_2 / Spacing);
}

bool LNPCrustAtlas::Rasterize(
	const FLNPBakeTriangleMesh& Crust,
	const FLNPCrustRasterSettings& Settings,
	FLNPCrustAtlasRaster& OutRaster,
	FString& OutError)
{
	OutRaster = FLNPCrustAtlasRaster();
	const int32 N = Settings.Subdivisions;
	if (N < 1 || GetSampleCount(N) <= 0)
	{
		OutError = FString::Printf(TEXT("Invalid crust Atlas subdivisions %d."), N);
		return false;
	}

	TArray<FVector3d> TriangleNormals;
	TriangleNormals.SetNumUninitialized(Crust.Triangles.Num());
	for (int32 TriangleIndex = 0; TriangleIndex < Crust.Triangles.Num(); ++TriangleIndex)
	{
		TriangleNormals[TriangleIndex] = Crust.GetTriangleNormal(TriangleIndex);
	}

	const FBakeMeshAdapter Adapter{&Crust};
	const UE::Geometry::TMeshAABBTree3<FBakeMeshAdapter> Tree(&Adapter);

	OutRaster.Subdivisions = N;
	OutRaster.Samples.SetNum(GetSampleCount(N));
	TArray<uint8> FrontHitCounts;
	FrontHitCounts.SetNumZeroed(OutRaster.Samples.Num());

	// 샘플마다 독립이라 병렬 결과도 결정론적이다.
	ParallelFor(N + 1, [&](int32 J)
	{
		TArray<MeshIntersection::FHitIntersectionResult> Hits;
		for (int32 I = 0; I + J <= N; ++I)
		{
			const int32 Index = GetSampleIndex(N, I, J);
			const FVector3d Direction = GetSampleDirection(N, I, J);
			Hits.Reset();
			Tree.FindAllHitTriangles(UE::Geometry::FWatertightRay3d(FVector3d::ZeroVector, Direction), Hits);

			FLNPCrustSample& Sample = OutRaster.Samples[Index];
			int32 FrontCount = 0;
			double LastFrontDistance = 0.0;
			for (const MeshIntersection::FHitIntersectionResult& Hit : Hits)
			{
				const FVector3d& Normal = TriangleNormals[Hit.TriangleId];
				if (FVector3d::DotProduct(Normal, Direction) >= 0.0)
				{
					continue;
				}
				if (FrontCount > 0 && Hit.Distance - LastFrontDistance <= Settings.HitMergeDistance)
				{
					continue;
				}
				if (FrontCount == 0)
				{
					Sample.Radius = Hit.Distance;
					Sample.Normal = FVector3f(Normal);
				}
				++FrontCount;
				LastFrontDistance = Hit.Distance;
			}

			FrontHitCounts[Index] = static_cast<uint8>(FMath::Min(FrontCount, 255));
			if (FrontCount == 1)
			{
				Sample.Flags |= ELNPSupportSampleFlags::Valid;
				if (FVector3d::DotProduct(FVector3d(Sample.Normal), -Direction) >= Settings.WalkableMinDot)
				{
					Sample.Flags |= ELNPSupportSampleFlags::Walkable;
				}
			}
		}
	});

	int32 OverhangCount = 0;
	int32 FirstOverhang = INDEX_NONE;
	for (int32 Index = 0; Index < FrontHitCounts.Num(); ++Index)
	{
		if (FrontHitCounts[Index] > 1)
		{
			++OverhangCount;
			FirstOverhang = FirstOverhang == INDEX_NONE ? Index : FirstOverhang;
		}
	}
	if (OverhangCount > 0)
	{
		OutError = FString::Printf(
			TEXT("Crust has %d Atlas direction(s) with more than one front-facing floor (overhang); first sample index %d has %d."),
			OverhangCount, FirstOverhang, FrontHitCounts[FirstOverhang]);
		OutRaster = FLNPCrustAtlasRaster();
		return false;
	}

	// 이웃 사이 선분 경사가 walkable 기준보다 가파르면 그 사이에 단차·절벽이 있어 보간할 수 없다.
	const double TanWalkable = FMath::Tan(FMath::Acos(FMath::Clamp(Settings.WalkableMinDot, 0.0, 1.0)));
	const double MinNeighborNormalDot = FMath::Cos(FMath::DegreesToRadians(Settings.MaxNeighborNormalAngleDeg));
	for (int32 J = 0; J <= N; ++J)
	{
		for (int32 I = 0; I + J <= N; ++I)
		{
			FLNPCrustSample& Sample = OutRaster.Samples[GetSampleIndex(N, I, J)];
			bool bNeedsExact = !EnumHasAllFlags(Sample.Flags, ELNPSupportSampleFlags::Valid | ELNPSupportSampleFlags::Walkable);
			const FVector3d Direction = GetSampleDirection(N, I, J);
			for (int32 NeighborIndex = 0; NeighborIndex < UE_ARRAY_COUNT(NeighborOffsets) && !bNeedsExact; ++NeighborIndex)
			{
				const int32 NI = I + NeighborOffsets[NeighborIndex][0];
				const int32 NJ = J + NeighborOffsets[NeighborIndex][1];
				if (NI < 0 || NJ < 0 || NI + NJ > N)
				{
					continue;
				}

				const FLNPCrustSample& Neighbor = OutRaster.Samples[GetSampleIndex(N, NI, NJ)];
				if (!EnumHasAnyFlags(Neighbor.Flags, ELNPSupportSampleFlags::Valid))
				{
					bNeedsExact = true;
					break;
				}

				const double Angle = FMath::Acos(FMath::Clamp(
					FVector3d::DotProduct(Direction, GetSampleDirection(N, NI, NJ)), -1.0, 1.0));
				const double Arc = Angle * 0.5 * (Sample.Radius + Neighbor.Radius);
				bNeedsExact = FMath::Abs(Sample.Radius - Neighbor.Radius) > Arc * TanWalkable
					|| FVector3f::DotProduct(Sample.Normal, Neighbor.Normal) < MinNeighborNormalDot;
			}

			if (bNeedsExact)
			{
				Sample.Flags |= ELNPSupportSampleFlags::NeedsExact;
			}
		}
	}
	return true;
}
