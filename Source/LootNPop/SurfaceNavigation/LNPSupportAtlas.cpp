// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPSupportAtlas.h"

#include "Async/ParallelFor.h"
#include "IndexTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Spatial/MeshAABBTree3.h"
#include "SurfaceNavigation/LNPCrustAtlas.h"
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

/** codec v2 header·Layer 표 고정부·행 하나·샘플 하나의 크기(바이트). */
constexpr int64 PayloadHeaderSize = 2 + 2 + 2 + 2 + 8 + 3 * sizeof(FIoHash::ByteArray);
constexpr int64 PayloadLayerTableSize = 2 + 2 + 8 + 4 + 4 + 4;
constexpr int64 PayloadRowSize = 4 + 4;
constexpr int64 PayloadBytesPerSample = 2 + 4 + 1;

/** footprint 경계 판정 여유(격자 단위). 경계 위 격자점을 빠뜨리지 않기 위한 값이며 넘치는 격자점은 coverage hole이 된다. */
constexpr double FootprintEpsilon = 1e-4;

float SignNotZero(float Value)
{
	return Value >= 0.0f ? 1.0f : -1.0f;
}

bool IsInOctant(int32 Subdivisions, int32 I, int32 J)
{
	return I >= 0 && J >= 0 && I + J <= Subdivisions;
}

/**
 * 방향을 담은 격자 삼각형 세 꼭짓점과 barycentric 가중치. 이음매 위 방향은 회전 오차로 성분이 아주 작은 음수일 수 있다.
 * 그보다 크게 벗어나면 이 옥탄트가 아니므로 false다.
 */
bool ComputeCell(int32 N, const FVector3d& LocalDirection, FIntPoint (&OutCorners)[3], double (&OutWeights)[3])
{
	if (N < 1 || LocalDirection.GetMin() < -UE_KINDA_SMALL_NUMBER * LocalDirection.GetAbsMax())
	{
		return false;
	}
	const FVector3d Direction = LocalDirection.ComponentMax(FVector3d::ZeroVector);
	const double Sum = Direction.X + Direction.Y + Direction.Z;
	if (Sum <= 0.0)
	{
		return false;
	}

	const double U = Direction.X / Sum * N;
	const double V = Direction.Y / Sum * N;
	int32 I0 = FMath::Clamp(FMath::FloorToInt32(U), 0, N - 1);
	int32 J0 = FMath::Clamp(FMath::FloorToInt32(V), 0, N - 1);
	// 빗변(k=0) 위 점은 마지막 셀 밖으로 떨어진다. 아래 삼각형이 그 점을 포함하도록 셀을 당긴다.
	while (I0 + J0 > N - 1)
	{
		I0 > 0 ? --I0 : --J0;
	}
	const double FU = U - I0;
	const double FV = V - J0;

	if (FU + FV <= 1.0 || I0 + J0 + 2 > N)
	{
		OutCorners[0] = FIntPoint(I0, J0);
		OutCorners[1] = FIntPoint(I0 + 1, J0);
		OutCorners[2] = FIntPoint(I0, J0 + 1);
		OutWeights[0] = 1.0 - FU - FV;
		OutWeights[1] = FU;
		OutWeights[2] = FV;
	}
	else
	{
		OutCorners[0] = FIntPoint(I0 + 1, J0 + 1);
		OutCorners[1] = FIntPoint(I0 + 1, J0);
		OutCorners[2] = FIntPoint(I0, J0 + 1);
		OutWeights[0] = FU + FV - 1.0;
		OutWeights[1] = 1.0 - FV;
		OutWeights[2] = 1.0 - FU;
	}

	double WeightSum = 0.0;
	for (double& Weight : OutWeights)
	{
		Weight = FMath::Clamp(Weight, 0.0, 1.0);
		WeightSum += Weight;
	}
	for (double& Weight : OutWeights)
	{
		Weight /= WeightSum;
	}
	return true;
}

bool QuantizeLayer(
	const FLNPSupportLayerRaster& Raster,
	double BaseRadius,
	double RadiusStep,
	int32 LayerId,
	FLNPSupportAtlasLayer& OutLayer,
	FString& OutError)
{
	OutLayer = FLNPSupportAtlasLayer();
	OutLayer.Layout = Raster.Layout;
	OutLayer.SourceIndex = Raster.SourceIndex;
	OutLayer.BaseRadius = BaseRadius;
	OutLayer.RadiusStep = RadiusStep;
	OutLayer.RadiusQ.SetNumZeroed(Raster.Samples.Num());
	OutLayer.NormalQ.SetNumZeroed(Raster.Samples.Num() * 2);
	OutLayer.Flags.SetNumZeroed(Raster.Samples.Num());
	for (int32 Index = 0; Index < Raster.Samples.Num(); ++Index)
	{
		const FLNPSupportSample& Sample = Raster.Samples[Index];
		OutLayer.Flags[Index] = static_cast<uint8>(Sample.Flags);
		if (!EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::Valid))
		{
			continue;
		}

		if (!FMath::IsFinite(Sample.Radius) || Sample.Normal.ContainsNaN())
		{
			OutError = FString::Printf(TEXT("Layer %d sample %d has a non-finite radius or normal."), LayerId, Index);
			return false;
		}
		const double Offset = FMath::RoundToDouble((Sample.Radius - BaseRadius) / RadiusStep);
		if (FMath::Abs(Offset) > 32767.0)
		{
			OutError = FString::Printf(
				TEXT("Layer %d sample %d radius %.2f is outside the quantization range %.2f +/- %.2f (step %.4f)."),
				LayerId, Index, Sample.Radius, BaseRadius, 32767.0 * RadiusStep, RadiusStep);
			return false;
		}
		OutLayer.RadiusQ[Index] = static_cast<int16>(Offset);
		LNPSupportAtlas::EncodeNormal(Sample.Normal, OutLayer.NormalQ[Index * 2], OutLayer.NormalQ[Index * 2 + 1]);
	}
	return true;
}

/** 비지각 Layer 기준 반지름: Valid 샘플 반지름 범위의 중간값을 step 단위로 반올림한다. Valid 샘플이 없으면 Fallback이다. */
double ComputeLayerBaseRadius(const FLNPSupportLayerRaster& Raster, double RadiusStep, double Fallback)
{
	double Min = TNumericLimits<double>::Max();
	double Max = TNumericLimits<double>::Lowest();
	for (const FLNPSupportSample& Sample : Raster.Samples)
	{
		if (EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::Valid))
		{
			Min = FMath::Min(Min, Sample.Radius);
			Max = FMath::Max(Max, Sample.Radius);
		}
	}
	if (Min > Max || !FMath::IsFinite(Min) || !FMath::IsFinite(Max))
	{
		return Fallback;
	}
	return FMath::RoundToDouble((Min + Max) * 0.5 / RadiusStep) * RadiusStep;
}

bool IsFaceMapValid(const FLNPSupportFaceMap& FaceMap, int32 LayerCount)
{
	auto IsValidLayer = [LayerCount](uint16 Layer) { return Layer == LNPSupportLayers::NoLayer || Layer < LayerCount; };
	if (FaceMap.IsUniform())
	{
		return IsValidLayer(FaceMap.UniformLayer);
	}
	for (const uint16 Layer : FaceMap.LayerByExternalFace)
	{
		if (!IsValidLayer(Layer))
		{
			return false;
		}
	}
	return true;
}

int64 GetRemaining(FMemoryReaderView& Reader, int32 PayloadSize)
{
	return PayloadSize - Reader.Tell();
}
}

FLNPSupportLayout FLNPSupportLayout::MakeFull(int32 Subdivisions)
{
	FLNPSupportLayout Layout;
	Layout.Subdivisions = Subdivisions;
	Layout.Rows.SetNum(FMath::Max(Subdivisions + 1, 0));
	for (int32 J = 0; J < Layout.Rows.Num(); ++J)
	{
		Layout.Rows[J] = {0, Subdivisions + 1 - J};
	}
	Layout.BuildOffsets();
	return Layout;
}

void FLNPSupportLayout::BuildOffsets()
{
	RowOffsets.SetNumUninitialized(Rows.Num() + 1);
	RowOffsets[0] = 0;
	for (int32 Row = 0; Row < Rows.Num(); ++Row)
	{
		RowOffsets[Row + 1] = RowOffsets[Row] + Rows[Row].Count;
	}
}

bool FLNPSupportLayout::IsFull() const
{
	if (Subdivisions < 1 || J0 != 0 || Rows.Num() != Subdivisions + 1)
	{
		return false;
	}
	for (int32 J = 0; J < Rows.Num(); ++J)
	{
		if (Rows[J].IStart != 0 || Rows[J].Count != Subdivisions + 1 - J)
		{
			return false;
		}
	}
	return true;
}

int32 FLNPSupportLayout::Find(int32 I, int32 J) const
{
	const int32 Row = J - J0;
	if (Row < 0 || Row >= Rows.Num())
	{
		return INDEX_NONE;
	}
	const FLNPSupportRowSpan& Span = Rows[Row];
	if (I < Span.IStart || I >= Span.IStart + Span.Count)
	{
		return INDEX_NONE;
	}
	return RowOffsets[Row] + I - Span.IStart;
}

FVector3f FLNPSupportAtlasLayer::GetNormal(int32 Index) const
{
	return LNPSupportAtlas::DecodeNormal(NormalQ[Index * 2], NormalQ[Index * 2 + 1]);
}

int32 LNPSupportAtlas::GetSampleCount(int32 Subdivisions)
{
	return (Subdivisions + 1) * (Subdivisions + 2) / 2;
}

int32 LNPSupportAtlas::GetSampleIndex(int32 Subdivisions, int32 I, int32 J)
{
	return J * (Subdivisions + 1) - J * (J - 1) / 2 + I;
}

FVector3d LNPSupportAtlas::GetSampleDirection(int32 Subdivisions, int32 I, int32 J)
{
	return FVector3d(I, J, Subdivisions - I - J).GetSafeNormal();
}

int32 LNPSupportAtlas::ComputeSubdivisionsForSpacing(double Radius, double Spacing)
{
	return FMath::CeilToInt32(Radius * UE_DOUBLE_SQRT_3 * UE_DOUBLE_SQRT_2 / Spacing);
}

void LNPSupportAtlas::EncodeNormal(const FVector3f& Normal, int16& OutX, int16& OutY)
{
	const float L1 = FMath::Abs(Normal.X) + FMath::Abs(Normal.Y) + FMath::Abs(Normal.Z);
	if (L1 <= 0.0f)
	{
		OutX = 0;
		OutY = 0;
		return;
	}

	float X = Normal.X / L1;
	float Y = Normal.Y / L1;
	if (Normal.Z < 0.0f)
	{
		const float FoldedX = (1.0f - FMath::Abs(Y)) * SignNotZero(X);
		Y = (1.0f - FMath::Abs(X)) * SignNotZero(Y);
		X = FoldedX;
	}
	OutX = static_cast<int16>(FMath::RoundToInt32(FMath::Clamp(X, -1.0f, 1.0f) * 32767.0f));
	OutY = static_cast<int16>(FMath::RoundToInt32(FMath::Clamp(Y, -1.0f, 1.0f) * 32767.0f));
}

FVector3f LNPSupportAtlas::DecodeNormal(int16 X, int16 Y)
{
	float FX = X / 32767.0f;
	float FY = Y / 32767.0f;
	const float FZ = 1.0f - FMath::Abs(FX) - FMath::Abs(FY);
	if (FZ < 0.0f)
	{
		const float UnfoldedX = (1.0f - FMath::Abs(FY)) * SignNotZero(FX);
		FY = (1.0f - FMath::Abs(FX)) * SignNotZero(FY);
		FX = UnfoldedX;
	}
	return FVector3f(FX, FY, FZ).GetSafeNormal();
}

FLNPSupportLayout LNPSupportAtlas::ComputeFootprint(const FLNPBakeTriangleMesh& Mesh, int32 Subdivisions)
{
	const int32 N = Subdivisions;
	TArray<FIntPoint> RowRanges;
	RowRanges.Init(FIntPoint(MAX_int32, MIN_int32), FMath::Max(N + 1, 0));
	for (const FIntVector3& Triangle : Mesh.Triangles)
	{
		// 중심 투영은 직선을 직선으로 보내므로 삼각형의 투영은 투영한 세 꼭짓점의 삼각형이다.
		FVector2d Points[3];
		bool bProjected = true;
		for (int32 Corner = 0; Corner < 3 && bProjected; ++Corner)
		{
			const FVector3d Vertex = Mesh.Vertices[Triangle[Corner]].ComponentMax(FVector3d::ZeroVector);
			const double Sum = Vertex.X + Vertex.Y + Vertex.Z;
			bProjected = Sum > 0.0;
			Points[Corner] = bProjected ? FVector2d(Vertex.X / Sum * N, Vertex.Y / Sum * N) : FVector2d::ZeroVector;
		}
		if (!bProjected)
		{
			continue;
		}

		const double VMin = FMath::Min3(Points[0].Y, Points[1].Y, Points[2].Y);
		const double VMax = FMath::Max3(Points[0].Y, Points[1].Y, Points[2].Y);
		const int32 JBegin = FMath::Max(0, FMath::CeilToInt32(VMin - FootprintEpsilon));
		const int32 JEnd = FMath::Min(N, FMath::FloorToInt32(VMax + FootprintEpsilon));
		for (int32 J = JBegin; J <= JEnd; ++J)
		{
			double UMin = TNumericLimits<double>::Max();
			double UMax = TNumericLimits<double>::Lowest();
			for (int32 Edge = 0; Edge < 3; ++Edge)
			{
				const FVector2d& A = Points[Edge];
				const FVector2d& B = Points[(Edge + 1) % 3];
				const double DA = A.Y - J;
				const double DB = B.Y - J;
				if (FMath::Min(DA, DB) > FootprintEpsilon || FMath::Max(DA, DB) < -FootprintEpsilon)
				{
					continue;
				}
				if (FMath::Abs(DA - DB) <= UE_DOUBLE_SMALL_NUMBER)
				{
					UMin = FMath::Min3(UMin, A.X, B.X);
					UMax = FMath::Max3(UMax, A.X, B.X);
					continue;
				}
				const double T = FMath::Clamp(DA / (DA - DB), 0.0, 1.0);
				const double U = A.X + T * (B.X - A.X);
				UMin = FMath::Min(UMin, U);
				UMax = FMath::Max(UMax, U);
			}
			const int32 IBegin = FMath::Max(0, FMath::CeilToInt32(UMin - FootprintEpsilon));
			const int32 IEnd = FMath::Min(N - J, FMath::FloorToInt32(UMax + FootprintEpsilon));
			if (UMin <= UMax && IBegin <= IEnd)
			{
				RowRanges[J].X = FMath::Min(RowRanges[J].X, IBegin);
				RowRanges[J].Y = FMath::Max(RowRanges[J].Y, IEnd);
			}
		}
	}

	FLNPSupportLayout Layout;
	Layout.Subdivisions = N;
	int32 First = INDEX_NONE;
	int32 Last = INDEX_NONE;
	for (int32 J = 0; J < RowRanges.Num(); ++J)
	{
		if (RowRanges[J].X <= RowRanges[J].Y)
		{
			First = First == INDEX_NONE ? J : First;
			Last = J;
		}
	}
	if (First != INDEX_NONE)
	{
		Layout.J0 = First;
		for (int32 J = First; J <= Last; ++J)
		{
			const FIntPoint& Range = RowRanges[J];
			Layout.Rows.Add(Range.X <= Range.Y ? FLNPSupportRowSpan{Range.X, Range.Y - Range.X + 1} : FLNPSupportRowSpan());
		}
	}
	Layout.BuildOffsets();
	return Layout;
}

bool LNPSupportAtlas::Rasterize(
	const FLNPBakeTriangleMesh& Mesh,
	const FLNPSupportRasterSettings& Settings,
	const FLNPSupportLayout& Layout,
	FLNPSupportLayerRaster& OutRaster,
	FString& OutError)
{
	OutRaster = FLNPSupportLayerRaster();
	const int32 N = Layout.Subdivisions;
	if (N < 1 || Layout.RowOffsets.Num() != Layout.Rows.Num() + 1)
	{
		OutError = FString::Printf(TEXT("Invalid Atlas layout: subdivisions %d, %d rows."), N, Layout.Rows.Num());
		return false;
	}
	if (Mesh.Triangles.IsEmpty())
	{
		OutError = TEXT("Atlas mesh has no triangles.");
		return false;
	}

	TArray<FVector3d> TriangleNormals;
	TriangleNormals.SetNumUninitialized(Mesh.Triangles.Num());
	for (int32 TriangleIndex = 0; TriangleIndex < Mesh.Triangles.Num(); ++TriangleIndex)
	{
		TriangleNormals[TriangleIndex] = Mesh.GetTriangleNormal(TriangleIndex);
	}

	const FBakeMeshAdapter Adapter{&Mesh};
	const UE::Geometry::TMeshAABBTree3<FBakeMeshAdapter> Tree(&Adapter);

	OutRaster.Layout = Layout;
	OutRaster.Samples.SetNum(Layout.Num());
	TArray<uint8> FrontHitCounts;
	FrontHitCounts.SetNumZeroed(OutRaster.Samples.Num());

	// 샘플마다 독립이라 병렬 결과도 결정론적이다.
	ParallelFor(Layout.Rows.Num(), [&](int32 Row)
	{
		const int32 J = Layout.J0 + Row;
		const FLNPSupportRowSpan& Span = Layout.Rows[Row];
		TArray<MeshIntersection::FHitIntersectionResult> Hits;
		for (int32 I = Span.IStart; I < Span.IStart + Span.Count; ++I)
		{
			const int32 Index = Layout.RowOffsets[Row] + I - Span.IStart;
			const FVector3d Direction = GetSampleDirection(N, I, J);
			Hits.Reset();
			Tree.FindAllHitTriangles(UE::Geometry::FWatertightRay3d(FVector3d::ZeroVector, Direction), Hits);

			FLNPSupportSample& Sample = OutRaster.Samples[Index];
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

	int32 MultiHitCount = 0;
	int32 FirstMultiHit = INDEX_NONE;
	for (int32 Index = 0; Index < FrontHitCounts.Num(); ++Index)
	{
		if (FrontHitCounts[Index] > 1)
		{
			++MultiHitCount;
			FirstMultiHit = FirstMultiHit == INDEX_NONE ? Index : FirstMultiHit;
		}
	}
	if (MultiHitCount > 0)
	{
		OutError = FString::Printf(
			TEXT("%d Atlas direction(s) have more than one front-facing floor (crust overhang or folded Layer sheet); ")
			TEXT("first sample index %d has %d."),
			MultiHitCount, FirstMultiHit, FrontHitCounts[FirstMultiHit]);
		OutRaster = FLNPSupportLayerRaster();
		return false;
	}

	// 이웃 사이 선분 경사가 walkable 기준보다 가파르면 그 사이에 단차·절벽이 있어 보간할 수 없다.
	const double TanWalkable = FMath::Tan(FMath::Acos(FMath::Clamp(Settings.WalkableMinDot, 0.0, 1.0)));
	const double MinNeighborNormalDot = FMath::Cos(FMath::DegreesToRadians(Settings.MaxNeighborNormalAngleDeg));
	for (int32 Row = 0; Row < Layout.Rows.Num(); ++Row)
	{
		const int32 J = Layout.J0 + Row;
		const FLNPSupportRowSpan& Span = Layout.Rows[Row];
		for (int32 I = Span.IStart; I < Span.IStart + Span.Count; ++I)
		{
			FLNPSupportSample& Sample = OutRaster.Samples[Layout.RowOffsets[Row] + I - Span.IStart];
			bool bNeedsExact = !EnumHasAllFlags(Sample.Flags, ELNPSupportSampleFlags::Valid | ELNPSupportSampleFlags::Walkable);
			const FVector3d Direction = GetSampleDirection(N, I, J);
			for (int32 NeighborIndex = 0; NeighborIndex < UE_ARRAY_COUNT(NeighborOffsets) && !bNeedsExact; ++NeighborIndex)
			{
				const int32 NI = I + NeighborOffsets[NeighborIndex][0];
				const int32 NJ = J + NeighborOffsets[NeighborIndex][1];
				if (!IsInOctant(N, NI, NJ))
				{
					continue;
				}

				// 구간 밖 이웃은 광선이 빗나간 샘플과 같다.
				const int32 NeighborSample = Layout.Find(NI, NJ);
				if (NeighborSample == INDEX_NONE
					|| !EnumHasAnyFlags(OutRaster.Samples[NeighborSample].Flags, ELNPSupportSampleFlags::Valid))
				{
					bNeedsExact = true;
					break;
				}

				const FLNPSupportSample& Neighbor = OutRaster.Samples[NeighborSample];
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

bool LNPSupportAtlas::Encode(
	TConstArrayView<FLNPSupportLayerRaster> Layers,
	TConstArrayView<FLNPSupportAtlasSource> Sources,
	const FLNPSupportCodecSettings& Settings,
	TArray<uint8>& OutPayload,
	FString& OutError)
{
	OutPayload.Reset();
	if (!(Settings.BaseRadius > 0.0) || !(Settings.RadiusStep > 0.0) || !FMath::IsFinite(Settings.RadiusStep))
	{
		OutError = FString::Printf(TEXT("Invalid Support codec settings: base radius %.3f, radius step %.4f."),
			Settings.BaseRadius, Settings.RadiusStep);
		return false;
	}
	if (Layers.IsEmpty() || Layers.Num() >= LNPSupportLayers::NoLayer || Sources.IsEmpty() || Sources.Num() > MAX_uint16)
	{
		OutError = FString::Printf(TEXT("Support Atlas has %d Layer(s) and %d source(s)."), Layers.Num(), Sources.Num());
		return false;
	}
	if (!Layers[0].Layout.IsFull())
	{
		OutError = TEXT("Support Layer 0 must be the crust with a full octant layout.");
		return false;
	}

	const int32 CrustSubdivisions = Layers[0].Layout.Subdivisions;
	for (int32 LayerId = 0; LayerId < Layers.Num(); ++LayerId)
	{
		const FLNPSupportLayerRaster& Layer = Layers[LayerId];
		const int32 N = Layer.Layout.Subdivisions;
		if (N < 1 || N % CrustSubdivisions != 0 || Layer.Samples.Num() != Layer.Layout.Num() || !Sources.IsValidIndex(Layer.SourceIndex))
		{
			OutError = FString::Printf(
				TEXT("Support Layer %d has subdivisions %d (crust %d), %d samples for a %d-sample layout, source %d of %d."),
				LayerId, N, CrustSubdivisions, Layer.Samples.Num(), Layer.Layout.Num(), Layer.SourceIndex, Sources.Num());
			return false;
		}
	}
	for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
	{
		if (SourceIndex > 0 && Sources[SourceIndex - 1].Key.Compare(Sources[SourceIndex].Key, ESearchCase::CaseSensitive) >= 0)
		{
			OutError = FString::Printf(TEXT("Support sources must have strictly ascending keys; '%s' follows '%s'."),
				*Sources[SourceIndex].Key, *Sources[SourceIndex - 1].Key);
			return false;
		}
		if (Sources[SourceIndex].Key.IsEmpty() || !IsFaceMapValid(Sources[SourceIndex].FaceMap, Layers.Num()))
		{
			OutError = FString::Printf(TEXT("Support source '%s' has an empty key or a face map that refers to a missing Layer."), *Sources[SourceIndex].Key);
			return false;
		}
	}

	FLNPSupportAtlas Atlas;
	Atlas.Layers.SetNum(Layers.Num());
	for (int32 LayerId = 0; LayerId < Layers.Num(); ++LayerId)
	{
		const double BaseRadius = LayerId == 0
			? Settings.BaseRadius
			: ComputeLayerBaseRadius(Layers[LayerId], Settings.RadiusStep, Settings.BaseRadius);
		if (!QuantizeLayer(Layers[LayerId], BaseRadius, Settings.RadiusStep, LayerId, Atlas.Layers[LayerId], OutError))
		{
			return false;
		}
	}
	LNPCrustAtlas::ComputeSeamHashes(Atlas.Layers[0], Atlas.SeamHashes);

	FMemoryWriter Writer(OutPayload);
	uint16 Version = CodecVersion;
	uint16 LayerCount = static_cast<uint16>(Layers.Num());
	uint16 SourceCount = static_cast<uint16>(Sources.Num());
	uint16 Reserved = 0;
	double RadiusStep = Settings.RadiusStep;
	Writer << Version << LayerCount << SourceCount << Reserved << RadiusStep;
	for (FIoHash& Hash : Atlas.SeamHashes)
	{
		Writer << Hash;
	}

	for (FLNPSupportAtlasLayer& Layer : Atlas.Layers)
	{
		uint16 SourceIndex = static_cast<uint16>(Layer.SourceIndex);
		int32 RowCount = Layer.Layout.Rows.Num();
		Writer << SourceIndex << Reserved << Layer.BaseRadius << Layer.Layout.Subdivisions << Layer.Layout.J0 << RowCount;
		for (FLNPSupportRowSpan& Span : Layer.Layout.Rows)
		{
			Writer << Span.IStart << Span.Count;
		}
	}
	for (FLNPSupportAtlasLayer& Layer : Atlas.Layers)
	{
		Writer.Serialize(Layer.RadiusQ.GetData(), Layer.RadiusQ.Num() * sizeof(int16));
		Writer.Serialize(Layer.NormalQ.GetData(), Layer.NormalQ.Num() * sizeof(int16));
		Writer.Serialize(Layer.Flags.GetData(), Layer.Flags.Num());
	}

	for (const FLNPSupportAtlasSource& Source : Sources)
	{
		FTCHARToUTF8 Key(*Source.Key);
		int32 KeyBytes = Key.Length();
		Writer << KeyBytes;
		Writer.Serialize(const_cast<ANSICHAR*>(Key.Get()), KeyBytes);
		uint8 Kind = Source.FaceMap.IsUniform() ? 0 : 1;
		Writer << Kind;
		if (Kind == 0)
		{
			uint16 Layer = Source.FaceMap.UniformLayer;
			Writer << Layer;
		}
		else
		{
			int32 Count = Source.FaceMap.LayerByExternalFace.Num();
			Writer << Count;
			Writer.Serialize(const_cast<uint16*>(Source.FaceMap.LayerByExternalFace.GetData()), Count * sizeof(uint16));
		}
	}
	return true;
}

bool LNPSupportAtlas::Decode(TConstArrayView<uint8> Payload, FLNPSupportAtlas& OutAtlas, FString& OutError)
{
	OutAtlas = FLNPSupportAtlas();
	auto Fail = [&OutAtlas, &OutError](const FString& Error)
	{
		OutError = Error;
		OutAtlas = FLNPSupportAtlas();
		return false;
	};
	if (Payload.Num() < PayloadHeaderSize)
	{
		return Fail(FString::Printf(TEXT("Support payload is %d bytes, smaller than its header."), Payload.Num()));
	}

	FMemoryReaderView Reader(MakeArrayView(Payload.GetData(), Payload.Num()));
	uint16 Version = 0;
	uint16 LayerCount = 0;
	uint16 SourceCount = 0;
	uint16 Reserved = 0;
	double RadiusStep = 0.0;
	Reader << Version << LayerCount << SourceCount << Reserved << RadiusStep;
	for (FIoHash& Hash : OutAtlas.SeamHashes)
	{
		Reader << Hash;
	}
	if (Version != CodecVersion)
	{
		return Fail(FString::Printf(TEXT("Unsupported Support payload codec %u."), Version));
	}
	if (LayerCount < 1 || LayerCount == LNPSupportLayers::NoLayer || SourceCount < 1 || !(RadiusStep > 0.0) || !FMath::IsFinite(RadiusStep))
	{
		return Fail(FString::Printf(TEXT("Support payload has %u Layer(s), %u source(s), radius step %.4f."),
			LayerCount, SourceCount, RadiusStep));
	}

	int64 TotalSamples = 0;
	OutAtlas.Layers.SetNum(LayerCount);
	for (int32 LayerId = 0; LayerId < LayerCount; ++LayerId)
	{
		if (GetRemaining(Reader, Payload.Num()) < PayloadLayerTableSize)
		{
			return Fail(TEXT("Support payload ended inside the Layer table."));
		}
		FLNPSupportAtlasLayer& Layer = OutAtlas.Layers[LayerId];
		uint16 SourceIndex = 0;
		int32 RowCount = 0;
		FLNPSupportLayout& Layout = Layer.Layout;
		Reader << SourceIndex << Reserved << Layer.BaseRadius << Layout.Subdivisions << Layout.J0 << RowCount;
		Layer.SourceIndex = SourceIndex;
		Layer.RadiusStep = RadiusStep;
		const int32 N = Layout.Subdivisions;
		const int32 CrustN = LayerId == 0 ? N : OutAtlas.Layers[0].Layout.Subdivisions;
		if (N < 1 || N % CrustN != 0 || SourceIndex >= SourceCount || !(Layer.BaseRadius > 0.0) || !FMath::IsFinite(Layer.BaseRadius)
			|| Layout.J0 < 0 || RowCount < 0 || static_cast<int64>(Layout.J0) + RowCount > static_cast<int64>(N) + 1
			|| GetRemaining(Reader, Payload.Num()) < RowCount * PayloadRowSize)
		{
			return Fail(FString::Printf(
				TEXT("Support Layer %d has subdivisions %d (crust %d), source %u of %u, base radius %.2f, rows [%d, +%d)."),
				LayerId, N, CrustN, SourceIndex, SourceCount, Layer.BaseRadius, Layout.J0, RowCount));
		}

		Layout.Rows.SetNum(RowCount);
		for (int32 Row = 0; Row < RowCount; ++Row)
		{
			FLNPSupportRowSpan& Span = Layout.Rows[Row];
			Reader << Span.IStart << Span.Count;
			const int32 J = Layout.J0 + Row;
			if (Span.Count < 0 || (Span.Count > 0 && (Span.IStart < 0 || static_cast<int64>(Span.IStart) + Span.Count > N - J + 1)))
			{
				return Fail(FString::Printf(TEXT("Support Layer %d row %d span [%d, +%d) is outside the octant."),
					LayerId, J, Span.IStart, Span.Count));
			}
			TotalSamples += Span.Count;
		}
		Layout.BuildOffsets();
		if (LayerId == 0 && !Layout.IsFull())
		{
			return Fail(TEXT("Support Layer 0 is not a full crust layout."));
		}
	}

	if (GetRemaining(Reader, Payload.Num()) < TotalSamples * PayloadBytesPerSample)
	{
		return Fail(FString::Printf(TEXT("Support payload is too small for %lld samples."), TotalSamples));
	}
	for (FLNPSupportAtlasLayer& Layer : OutAtlas.Layers)
	{
		const int32 Count = Layer.Layout.Num();
		Layer.RadiusQ.SetNumUninitialized(Count);
		Layer.NormalQ.SetNumUninitialized(Count * 2);
		Layer.Flags.SetNumUninitialized(Count);
		Reader.Serialize(Layer.RadiusQ.GetData(), Layer.RadiusQ.Num() * sizeof(int16));
		Reader.Serialize(Layer.NormalQ.GetData(), Layer.NormalQ.Num() * sizeof(int16));
		Reader.Serialize(Layer.Flags.GetData(), Layer.Flags.Num());
	}

	OutAtlas.Sources.SetNum(SourceCount);
	for (int32 SourceIndex = 0; SourceIndex < SourceCount; ++SourceIndex)
	{
		FLNPSupportAtlasSource& Source = OutAtlas.Sources[SourceIndex];
		int32 KeyBytes = 0;
		Reader << KeyBytes;
		if (Reader.IsError() || KeyBytes < 1 || GetRemaining(Reader, Payload.Num()) < KeyBytes + 1)
		{
			return Fail(FString::Printf(TEXT("Support source %d has an invalid key length %d."), SourceIndex, KeyBytes));
		}
		const ANSICHAR* KeyData = reinterpret_cast<const ANSICHAR*>(Payload.GetData() + Reader.Tell());
		const FUTF8ToTCHAR Key(KeyData, KeyBytes);
		Source.Key = FString(Key.Length(), Key.Get());
		Reader.Seek(Reader.Tell() + KeyBytes);

		uint8 Kind = 0;
		Reader << Kind;
		if (Kind == 0)
		{
			Reader << Source.FaceMap.UniformLayer;
		}
		else if (Kind == 1)
		{
			int32 Count = 0;
			Reader << Count;
			if (Reader.IsError() || Count < 1 || GetRemaining(Reader, Payload.Num()) < static_cast<int64>(Count) * static_cast<int64>(sizeof(uint16)))
			{
				return Fail(FString::Printf(TEXT("Support source '%s' has an invalid face map size %d."), *Source.Key, Count));
			}
			Source.FaceMap.LayerByExternalFace.SetNumUninitialized(Count);
			Reader.Serialize(Source.FaceMap.LayerByExternalFace.GetData(), Count * sizeof(uint16));
		}
		else
		{
			return Fail(FString::Printf(TEXT("Support source '%s' has an unknown face map kind %u."), *Source.Key, Kind));
		}
		if (Reader.IsError() || !IsFaceMapValid(Source.FaceMap, LayerCount))
		{
			return Fail(FString::Printf(TEXT("Support source '%s' face map refers to a missing Layer."), *Source.Key));
		}
		if (SourceIndex > 0 && OutAtlas.Sources[SourceIndex - 1].Key.Compare(Source.Key, ESearchCase::CaseSensitive) >= 0)
		{
			return Fail(FString::Printf(TEXT("Support source keys are not strictly ascending at '%s'."), *Source.Key));
		}
	}

	if (Reader.IsError())
	{
		return Fail(TEXT("Support payload ended early."));
	}
	if (Reader.Tell() != Payload.Num())
	{
		return Fail(FString::Printf(TEXT("Support payload has %lld trailing bytes."), Payload.Num() - Reader.Tell()));
	}
	return true;
}

bool LNPSupportAtlas::QueryLayer(const FLNPSupportAtlasLayer& Layer, const FVector3d& LocalDirection, FLNPSupportLayerQuery& OutQuery)
{
	OutQuery = FLNPSupportLayerQuery();
	FIntPoint Corners[3];
	double Weights[3];
	if (!ComputeCell(Layer.Layout.Subdivisions, LocalDirection, Corners, Weights))
	{
		return false;
	}

	bool bInterpolates = true;
	double Radius = 0.0;
	FVector3f Normal = FVector3f::ZeroVector;
	OutQuery.MinCornerRadius = TNumericLimits<double>::Max();
	OutQuery.MaxCornerRadius = TNumericLimits<double>::Lowest();
	for (int32 Corner = 0; Corner < 3; ++Corner)
	{
		const int32 Index = Layer.Layout.Find(Corners[Corner].X, Corners[Corner].Y);
		const ELNPSupportSampleFlags Flags = Index == INDEX_NONE ? ELNPSupportSampleFlags::None : Layer.GetFlags(Index);
		if (!EnumHasAnyFlags(Flags, ELNPSupportSampleFlags::Valid))
		{
			bInterpolates = false;
			continue;
		}
		const double CornerRadius = Layer.GetRadius(Index);
		++OutQuery.ValidCorners;
		OutQuery.MinCornerRadius = FMath::Min(OutQuery.MinCornerRadius, CornerRadius);
		OutQuery.MaxCornerRadius = FMath::Max(OutQuery.MaxCornerRadius, CornerRadius);
		bInterpolates &= !EnumHasAnyFlags(Flags, ELNPSupportSampleFlags::NeedsExact);
		Radius += Weights[Corner] * CornerRadius;
		Normal += static_cast<float>(Weights[Corner]) * Layer.GetNormal(Index);
	}
	if (OutQuery.ValidCorners == 0)
	{
		OutQuery.MinCornerRadius = 0.0;
		OutQuery.MaxCornerRadius = 0.0;
	}
	if (bInterpolates)
	{
		OutQuery.bInterpolated = true;
		OutQuery.Radius = Radius;
		OutQuery.Normal = Normal.GetSafeNormal();
	}
	return OutQuery.bInterpolated;
}

ELNPSupportQueryResult LNPSupportAtlas::QueryLayers(
	const FLNPSupportAtlas& Atlas,
	const FVector3d& LocalDirection,
	double FeetRadius,
	double MaxStepUp,
	double MaxDrop,
	uint16 PreferredLayer,
	FLNPSupportLayerHit& OutHit)
{
	OutHit = FLNPSupportLayerHit();
	FIntPoint Corners[3];
	double Weights[3];
	if (Atlas.Layers.IsEmpty() || !ComputeCell(Atlas.Layers[0].Layout.Subdivisions, LocalDirection, Corners, Weights))
	{
		return ELNPSupportQueryResult::NeedsExact;
	}

	const double WindowTop = FeetRadius - MaxStepUp;
	const double WindowBottom = FeetRadius + MaxDrop;
	FLNPSupportLayerHit Best;
	bool bPreferredFound = false;
	for (int32 LayerId = 0; LayerId < Atlas.Layers.Num(); ++LayerId)
	{
		FLNPSupportLayerQuery Query;
		QueryLayer(Atlas.Layers[LayerId], LocalDirection, Query);
		if (Query.ValidCorners == 0)
		{
			continue;
		}
		if (!Query.bInterpolated)
		{
			if (Query.MaxCornerRadius >= WindowTop && Query.MinCornerRadius <= WindowBottom)
			{
				return ELNPSupportQueryResult::NeedsExact;
			}
			continue;
		}
		if (Query.Radius < WindowTop || Query.Radius > WindowBottom || bPreferredFound)
		{
			continue;
		}
		const bool bPreferred = LayerId == PreferredLayer;
		if (bPreferred || Best.Layer == LNPSupportLayers::NoLayer || Query.Radius < Best.Radius)
		{
			Best.Layer = static_cast<uint16>(LayerId);
			Best.Radius = Query.Radius;
			Best.Normal = Query.Normal;
			bPreferredFound = bPreferred;
		}
	}

	if (Best.Layer == LNPSupportLayers::NoLayer)
	{
		return ELNPSupportQueryResult::NoSupport;
	}
	OutHit = Best;
	return ELNPSupportQueryResult::Supported;
}
