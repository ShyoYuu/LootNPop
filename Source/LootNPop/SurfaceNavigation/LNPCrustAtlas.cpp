// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPCrustAtlas.h"

#include "Async/ParallelFor.h"
#include "IndexTypes.h"
#include "Intersection/IntrRay3Triangle3.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
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

/** codec v1 header 크기(바이트)와 샘플당 body 크기. */
constexpr int64 PayloadHeaderSize = 2 + 1 + 1 + 4 + 8 + 8 + 4 + 3 * sizeof(FIoHash::ByteArray);
constexpr int64 PayloadBytesPerSample = 2 + 4 + 1;

float SignNotZero(float Value)
{
	return Value >= 0.0f ? 1.0f : -1.0f;
}

void ComputeSeamHashes(const FLNPCrustAtlas& Atlas, FIoHash (&OutHashes)[3])
{
	const int32 N = Atlas.Subdivisions;
	for (int32 Edge = 0; Edge < 3; ++Edge)
	{
		TArray<uint8> Bytes;
		Bytes.Reserve((N + 1) * 3);
		for (int32 Step = 0; Step <= N; ++Step)
		{
			const FIntPoint Coord = LNPCrustAtlas::GetSeamSampleCoord(N, static_cast<ELNPCrustSeamEdge>(Edge), Step);
			const int32 Index = LNPCrustAtlas::GetSampleIndex(N, Coord.X, Coord.Y);
			const uint16 Radius = static_cast<uint16>(Atlas.RadiusQ[Index]);
			Bytes.Add(static_cast<uint8>(Radius & 0xff));
			Bytes.Add(static_cast<uint8>(Radius >> 8));
			Bytes.Add(Atlas.Flags[Index] & static_cast<uint8>(ELNPSupportSampleFlags::Valid));
		}
		OutHashes[Edge] = FIoHash::HashBuffer(Bytes.GetData(), Bytes.Num());
	}
}
}

FVector3f FLNPCrustAtlas::GetNormal(int32 Index) const
{
	return LNPCrustAtlas::DecodeNormal(NormalQ[Index * 2], NormalQ[Index * 2 + 1]);
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

	FLNPBakeTriangleMesh Snapped = Crust;
	for (FVector3d& Vertex : Snapped.Vertices)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (FMath::Abs(Vertex[Axis]) <= Settings.SeamSnapDistance)
			{
				Vertex[Axis] = 0.0;
			}
		}
	}

	TArray<FVector3d> TriangleNormals;
	TriangleNormals.SetNumUninitialized(Snapped.Triangles.Num());
	for (int32 TriangleIndex = 0; TriangleIndex < Snapped.Triangles.Num(); ++TriangleIndex)
	{
		TriangleNormals[TriangleIndex] = Snapped.GetTriangleNormal(TriangleIndex);
	}

	const FBakeMeshAdapter Adapter{&Snapped};
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

FIntPoint LNPCrustAtlas::GetSeamSampleCoord(int32 Subdivisions, ELNPCrustSeamEdge Edge, int32 Step)
{
	switch (Edge)
	{
	case ELNPCrustSeamEdge::X0:
		return FIntPoint(0, Subdivisions - Step);
	case ELNPCrustSeamEdge::Y0:
		return FIntPoint(Subdivisions - Step, 0);
	default:
		return FIntPoint(Subdivisions - Step, Step);
	}
}

bool LNPCrustAtlas::ComputeSeamPairs(
	TConstArrayView<FRotator> SlotRotations,
	TArray<FLNPCrustSeamPair>& OutPairs,
	FString& OutError)
{
	OutPairs.Reset();
	// 변 규약 순서의 시작·끝 꼭짓점 축(GetSeamSampleCoord와 같다).
	constexpr int32 EdgeAxes[3][2] = {{1, 2}, {0, 2}, {0, 1}};

	struct FInstance
	{
		FLNPCrustSeamEdgeRef Ref;
		FIntVector Start;
	};
	TMap<TPair<FIntVector, FIntVector>, TArray<FInstance>> WorldEdges;
	for (int32 Slot = 0; Slot < SlotRotations.Num(); ++Slot)
	{
		FIntVector Corners[3];
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			FVector3d Local = FVector3d::ZeroVector;
			Local[Axis] = 1.0;
			const FVector3d World = SlotRotations[Slot].RotateVector(Local);
			Corners[Axis] = FIntVector(FMath::RoundToInt32(World.X), FMath::RoundToInt32(World.Y), FMath::RoundToInt32(World.Z));
			if (!World.Equals(FVector3d(Corners[Axis]), UE_KINDA_SMALL_NUMBER))
			{
				OutError = FString::Printf(TEXT("Slot %d rotation %s does not map axis %d onto a world axis."),
					Slot, *SlotRotations[Slot].ToString(), Axis);
				return false;
			}
		}
		for (int32 Edge = 0; Edge < 3; ++Edge)
		{
			const FIntVector& Start = Corners[EdgeAxes[Edge][0]];
			const FIntVector& End = Corners[EdgeAxes[Edge][1]];
			// 월드 변 키는 방향과 무관해야 한다. 성분 사전순으로 두 끝을 정렬한다.
			const bool bStartFirst = Start.X != End.X ? Start.X < End.X : Start.Y != End.Y ? Start.Y < End.Y : Start.Z < End.Z;
			const TPair<FIntVector, FIntVector> Key = bStartFirst ? MakeTuple(Start, End) : MakeTuple(End, Start);
			WorldEdges.FindOrAdd(Key).Add({{Slot, static_cast<ELNPCrustSeamEdge>(Edge)}, Start});
		}
	}

	for (const TPair<TPair<FIntVector, FIntVector>, TArray<FInstance>>& WorldEdge : WorldEdges)
	{
		const TArray<FInstance>& Instances = WorldEdge.Value;
		if (Instances.Num() != 2)
		{
			OutError = FString::Printf(TEXT("World seam %s-%s has %d edge instances; expected 2."),
				*WorldEdge.Key.Key.ToString(), *WorldEdge.Key.Value.ToString(), Instances.Num());
			OutPairs.Reset();
			return false;
		}
		// 인스턴스는 (slot, 변) 오름차순으로 추가됐다.
		OutPairs.Add({Instances[0].Ref, Instances[1].Ref, Instances[0].Start != Instances[1].Start});
	}
	OutPairs.Sort([](const FLNPCrustSeamPair& L, const FLNPCrustSeamPair& R)
	{
		return L.A.Slot != R.A.Slot ? L.A.Slot < R.A.Slot : L.A.Edge < R.A.Edge;
	});
	return true;
}

void LNPCrustAtlas::EncodeNormal(const FVector3f& Normal, int16& OutX, int16& OutY)
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

FVector3f LNPCrustAtlas::DecodeNormal(int16 X, int16 Y)
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

bool LNPCrustAtlas::Encode(
	const FLNPCrustAtlasRaster& Raster,
	const FLNPCrustCodecSettings& Settings,
	TArray<uint8>& OutPayload,
	FString& OutError)
{
	OutPayload.Reset();
	const int32 N = Raster.Subdivisions;
	if (N < 1 || Raster.Samples.Num() != GetSampleCount(N))
	{
		OutError = FString::Printf(TEXT("Crust raster has %d samples for subdivisions %d."), Raster.Samples.Num(), N);
		return false;
	}
	if (!(Settings.BaseRadius > 0.0) || !(Settings.RadiusStep > 0.0) || !FMath::IsFinite(Settings.RadiusStep))
	{
		OutError = FString::Printf(TEXT("Invalid crust codec settings: base radius %.3f, radius step %.4f."),
			Settings.BaseRadius, Settings.RadiusStep);
		return false;
	}

	FLNPCrustAtlas Atlas;
	Atlas.Subdivisions = N;
	Atlas.BaseRadius = Settings.BaseRadius;
	Atlas.RadiusStep = Settings.RadiusStep;
	Atlas.RadiusQ.SetNumZeroed(Raster.Samples.Num());
	Atlas.NormalQ.SetNumZeroed(Raster.Samples.Num() * 2);
	Atlas.Flags.SetNumZeroed(Raster.Samples.Num());
	for (int32 Index = 0; Index < Raster.Samples.Num(); ++Index)
	{
		const FLNPCrustSample& Sample = Raster.Samples[Index];
		Atlas.Flags[Index] = static_cast<uint8>(Sample.Flags);
		if (!EnumHasAnyFlags(Sample.Flags, ELNPSupportSampleFlags::Valid))
		{
			continue;
		}

		if (!FMath::IsFinite(Sample.Radius) || Sample.Normal.ContainsNaN())
		{
			OutError = FString::Printf(TEXT("Crust sample %d has a non-finite radius or normal."), Index);
			return false;
		}
		const double Offset = FMath::RoundToDouble((Sample.Radius - Settings.BaseRadius) / Settings.RadiusStep);
		if (FMath::Abs(Offset) > 32767.0)
		{
			OutError = FString::Printf(
				TEXT("Crust sample %d radius %.2f is outside the quantization range %.2f +/- %.2f (step %.4f)."),
				Index, Sample.Radius, Settings.BaseRadius, 32767.0 * Settings.RadiusStep, Settings.RadiusStep);
			return false;
		}
		Atlas.RadiusQ[Index] = static_cast<int16>(Offset);
		EncodeNormal(Sample.Normal, Atlas.NormalQ[Index * 2], Atlas.NormalQ[Index * 2 + 1]);
	}
	ComputeSeamHashes(Atlas, Atlas.SeamHashes);

	FMemoryWriter Writer(OutPayload);
	uint16 Version = CodecVersion;
	uint8 LayerCount = 1;
	uint8 Reserved = 0;
	int32 Subdivisions = N;
	double BaseRadius = Atlas.BaseRadius;
	double RadiusStep = Atlas.RadiusStep;
	uint32 SampleCount = static_cast<uint32>(Atlas.Num());
	Writer << Version << LayerCount << Reserved << Subdivisions << BaseRadius << RadiusStep << SampleCount;
	for (FIoHash& Hash : Atlas.SeamHashes)
	{
		Writer << Hash;
	}
	Writer.Serialize(Atlas.RadiusQ.GetData(), Atlas.RadiusQ.Num() * sizeof(int16));
	Writer.Serialize(Atlas.NormalQ.GetData(), Atlas.NormalQ.Num() * sizeof(int16));
	Writer.Serialize(Atlas.Flags.GetData(), Atlas.Flags.Num());
	check(OutPayload.Num() == PayloadHeaderSize + PayloadBytesPerSample * Atlas.Num());
	return true;
}

bool LNPCrustAtlas::Decode(TConstArrayView<uint8> Payload, FLNPCrustAtlas& OutAtlas, FString& OutError)
{
	OutAtlas = FLNPCrustAtlas();
	if (Payload.Num() < PayloadHeaderSize)
	{
		OutError = FString::Printf(TEXT("Crust payload is %d bytes, smaller than its header."), Payload.Num());
		return false;
	}

	FMemoryReaderView Reader(MakeArrayView(Payload.GetData(), Payload.Num()));
	uint16 Version = 0;
	uint8 LayerCount = 0;
	uint8 Reserved = 0;
	int32 Subdivisions = 0;
	uint32 SampleCount = 0;
	Reader << Version << LayerCount << Reserved << Subdivisions << OutAtlas.BaseRadius << OutAtlas.RadiusStep << SampleCount;
	for (FIoHash& Hash : OutAtlas.SeamHashes)
	{
		Reader << Hash;
	}
	if (Version != CodecVersion || LayerCount != 1)
	{
		OutError = FString::Printf(TEXT("Unsupported crust payload codec %u with %u layer(s)."), Version, LayerCount);
		return false;
	}
	if (Subdivisions < 1 || SampleCount != static_cast<uint32>(GetSampleCount(Subdivisions))
		|| Payload.Num() != PayloadHeaderSize + PayloadBytesPerSample * SampleCount)
	{
		OutError = FString::Printf(TEXT("Crust payload size %d does not match subdivisions %d and %u samples."),
			Payload.Num(), Subdivisions, SampleCount);
		return false;
	}
	if (!(OutAtlas.BaseRadius > 0.0) || !(OutAtlas.RadiusStep > 0.0))
	{
		OutError = TEXT("Crust payload has a non-positive base radius or radius step.");
		return false;
	}

	OutAtlas.Subdivisions = Subdivisions;
	OutAtlas.RadiusQ.SetNumUninitialized(SampleCount);
	OutAtlas.NormalQ.SetNumUninitialized(SampleCount * 2);
	OutAtlas.Flags.SetNumUninitialized(SampleCount);
	Reader.Serialize(OutAtlas.RadiusQ.GetData(), OutAtlas.RadiusQ.Num() * sizeof(int16));
	Reader.Serialize(OutAtlas.NormalQ.GetData(), OutAtlas.NormalQ.Num() * sizeof(int16));
	Reader.Serialize(OutAtlas.Flags.GetData(), OutAtlas.Flags.Num());
	if (Reader.IsError())
	{
		OutError = TEXT("Crust payload ended early.");
		OutAtlas = FLNPCrustAtlas();
		return false;
	}
	return true;
}

bool LNPCrustAtlas::QuerySupport(const FLNPCrustAtlas& Atlas, const FVector3d& LocalDirection, FLNPCrustSupportHit& OutHit)
{
	OutHit = FLNPCrustSupportHit();
	const int32 N = Atlas.Subdivisions;
	// 이음매 위 방향은 회전 오차로 성분이 아주 작은 음수일 수 있다. 그보다 크게 벗어나면 이 옥탄트가 아니다.
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

	FIntPoint Corners[3];
	double Weights[3];
	if (FU + FV <= 1.0 || I0 + J0 + 2 > N)
	{
		Corners[0] = FIntPoint(I0, J0);
		Corners[1] = FIntPoint(I0 + 1, J0);
		Corners[2] = FIntPoint(I0, J0 + 1);
		Weights[0] = 1.0 - FU - FV;
		Weights[1] = FU;
		Weights[2] = FV;
	}
	else
	{
		Corners[0] = FIntPoint(I0 + 1, J0 + 1);
		Corners[1] = FIntPoint(I0 + 1, J0);
		Corners[2] = FIntPoint(I0, J0 + 1);
		Weights[0] = FU + FV - 1.0;
		Weights[1] = 1.0 - FV;
		Weights[2] = 1.0 - FU;
	}

	double WeightSum = 0.0;
	for (double& Weight : Weights)
	{
		Weight = FMath::Clamp(Weight, 0.0, 1.0);
		WeightSum += Weight;
	}

	FVector3f Normal = FVector3f::ZeroVector;
	for (int32 Corner = 0; Corner < 3; ++Corner)
	{
		const int32 Index = GetSampleIndex(N, Corners[Corner].X, Corners[Corner].Y);
		const ELNPSupportSampleFlags Flags = Atlas.GetFlags(Index);
		if (!EnumHasAnyFlags(Flags, ELNPSupportSampleFlags::Valid) || EnumHasAnyFlags(Flags, ELNPSupportSampleFlags::NeedsExact))
		{
			return false;
		}
		const double Weight = Weights[Corner] / WeightSum;
		OutHit.Radius += Weight * Atlas.GetRadius(Index);
		Normal += static_cast<float>(Weight) * Atlas.GetNormal(Index);
	}
	OutHit.Normal = Normal.GetSafeNormal();
	return true;
}
