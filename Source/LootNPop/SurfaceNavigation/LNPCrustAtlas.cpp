// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPCrustAtlas.h"

#include "SurfaceNavigation/LNPSurfaceBakeGeometry.h"

bool LNPCrustAtlas::Rasterize(
	const FLNPBakeTriangleMesh& Crust,
	const FLNPSupportRasterSettings& Settings,
	FLNPSupportLayerRaster& OutRaster,
	FString& OutError)
{
	OutRaster = FLNPSupportLayerRaster();
	const int32 N = Settings.Subdivisions;
	if (N < 1)
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
	if (!LNPSupportAtlas::Rasterize(Snapped, Settings, FLNPSupportLayout::MakeFull(N), OutRaster, OutError))
	{
		OutError = FString::Printf(TEXT("Crust: %s"), *OutError);
		return false;
	}
	return true;
}

void LNPCrustAtlas::ComputeSeamHashes(const FLNPSupportAtlasLayer& Crust, FIoHash (&OutHashes)[3])
{
	const int32 N = Crust.Layout.Subdivisions;
	for (int32 Edge = 0; Edge < 3; ++Edge)
	{
		TArray<uint8> Bytes;
		Bytes.Reserve((N + 1) * 3);
		for (int32 Step = 0; Step <= N; ++Step)
		{
			const FIntPoint Coord = GetSeamSampleCoord(N, static_cast<ELNPCrustSeamEdge>(Edge), Step);
			const int32 Index = LNPSupportAtlas::GetSampleIndex(N, Coord.X, Coord.Y);
			const uint16 Radius = static_cast<uint16>(Crust.RadiusQ[Index]);
			Bytes.Add(static_cast<uint8>(Radius & 0xff));
			Bytes.Add(static_cast<uint8>(Radius >> 8));
			Bytes.Add(Crust.Flags[Index] & static_cast<uint8>(ELNPSupportSampleFlags::Valid));
		}
		OutHashes[Edge] = FIoHash::HashBuffer(Bytes.GetData(), Bytes.Num());
	}
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
