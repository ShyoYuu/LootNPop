// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPSupportLayers.h"

#include "SurfaceNavigation/LNPSurfaceBakeGeometry.h"

namespace
{
/** 거리 Tolerance 안의 정점을 먼저 나온 정점으로 합친다. 입력 순서가 같으면 결과도 같다. */
TArray<int32> WeldVertices(TConstArrayView<FVector3d> Vertices, double Tolerance)
{
	TArray<int32> Representative;
	Representative.SetNumUninitialized(Vertices.Num());
	TMap<FIntVector, TArray<int32>> Cells;
	const double ToleranceSquared = Tolerance * Tolerance;
	for (int32 Index = 0; Index < Vertices.Num(); ++Index)
	{
		const FVector3d& Vertex = Vertices[Index];
		const FIntVector Cell(
			FMath::FloorToInt32(Vertex.X / Tolerance),
			FMath::FloorToInt32(Vertex.Y / Tolerance),
			FMath::FloorToInt32(Vertex.Z / Tolerance));

		int32 Found = INDEX_NONE;
		for (int32 DX = -1; DX <= 1 && Found == INDEX_NONE; ++DX)
		{
			for (int32 DY = -1; DY <= 1 && Found == INDEX_NONE; ++DY)
			{
				for (int32 DZ = -1; DZ <= 1 && Found == INDEX_NONE; ++DZ)
				{
					if (const TArray<int32>* Candidates = Cells.Find(Cell + FIntVector(DX, DY, DZ)))
					{
						for (const int32 Candidate : *Candidates)
						{
							if (FVector3d::DistSquared(Vertices[Candidate], Vertex) <= ToleranceSquared)
							{
								Found = Candidate;
								break;
							}
						}
					}
				}
			}
		}

		if (Found == INDEX_NONE)
		{
			Cells.FindOrAdd(Cell).Add(Index);
			Found = Index;
		}
		Representative[Index] = Found;
	}
	return Representative;
}

int32 FindRoot(TArray<int32>& Parent, int32 Index)
{
	while (Parent[Index] != Index)
	{
		Parent[Index] = Parent[Parent[Index]];
		Index = Parent[Index];
	}
	return Index;
}

void Union(TArray<int32>& Parent, int32 A, int32 B)
{
	A = FindRoot(Parent, A);
	B = FindRoot(Parent, B);
	if (A != B)
	{
		// 작은 인덱스를 root로 둬서 결과가 입력 순서만으로 정해지게 한다.
		Parent[FMath::Max(A, B)] = FMath::Min(A, B);
	}
}

bool IsWalkable(const FLNPBakeTriangleMesh& Mesh, int32 TriangleIndex, double WalkableMinDot)
{
	const FIntVector3& Triangle = Mesh.Triangles[TriangleIndex];
	const FVector3d Centroid = (Mesh.Vertices[Triangle.X] + Mesh.Vertices[Triangle.Y] + Mesh.Vertices[Triangle.Z]) / 3.0;
	const FVector3d Up = -Centroid.GetSafeNormal();
	return FVector3d::DotProduct(Mesh.GetTriangleNormal(TriangleIndex), Up) >= WalkableMinDot;
}

/** walkable 삼각형의 연결 성분. 각 성분은 추출 순서 인덱스 오름차순이다. */
TArray<TArray<int32>> FindWalkableSheets(const FLNPBakeTriangleMesh& Mesh, const FLNPSupportLayerSettings& Settings)
{
	const TArray<int32> Welded = WeldVertices(Mesh.Vertices, Settings.WeldDistance);
	const int32 TriangleCount = Mesh.Triangles.Num();

	TArray<bool> Walkable;
	Walkable.SetNumUninitialized(TriangleCount);
	TArray<int32> Parent;
	Parent.SetNumUninitialized(TriangleCount);
	TMap<uint64, int32> FirstTriangleByEdge;
	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		Parent[TriangleIndex] = TriangleIndex;
		Walkable[TriangleIndex] = IsWalkable(Mesh, TriangleIndex, Settings.WalkableMinDot);
		if (!Walkable[TriangleIndex])
		{
			continue;
		}

		const FIntVector3& Triangle = Mesh.Triangles[TriangleIndex];
		const int32 Corners[3] = {Welded[Triangle.X], Welded[Triangle.Y], Welded[Triangle.Z]};
		for (int32 Edge = 0; Edge < 3; ++Edge)
		{
			const uint32 A = static_cast<uint32>(Corners[Edge]);
			const uint32 B = static_cast<uint32>(Corners[(Edge + 1) % 3]);
			if (A == B)
			{
				continue;
			}
			const uint64 Key = (static_cast<uint64>(FMath::Min(A, B)) << 32) | FMath::Max(A, B);
			if (const int32* Other = FirstTriangleByEdge.Find(Key))
			{
				Union(Parent, *Other, TriangleIndex);
			}
			else
			{
				FirstTriangleByEdge.Add(Key, TriangleIndex);
			}
		}
	}

	TArray<TArray<int32>> Sheets;
	TMap<int32, int32> SheetByRoot;
	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		if (Walkable[TriangleIndex])
		{
			const int32 Root = FindRoot(Parent, TriangleIndex);
			int32& Sheet = SheetByRoot.FindOrAdd(Root, Sheets.Num());
			if (Sheet == Sheets.Num())
			{
				Sheets.AddDefaulted();
			}
			Sheets[Sheet].Add(TriangleIndex);
		}
	}
	return Sheets;
}

int32 GetMinExternalFace(const FLNPBakeTriangleMesh& Mesh, TConstArrayView<int32> Triangles)
{
	int32 Min = MAX_int32;
	for (const int32 TriangleIndex : Triangles)
	{
		Min = FMath::Min(Min, Mesh.ExternalFaceIndices[TriangleIndex]);
	}
	return Min;
}

bool ValidateFaceIndices(const FLNPBakeSupportSource& Source, FString& OutError)
{
	const FLNPBakeTriangleMesh& Mesh = Source.Mesh;
	if (Mesh.Triangles.IsEmpty() || Mesh.ExternalFaceIndices.Num() != Mesh.Triangles.Num())
	{
		OutError = FString::Printf(TEXT("Support source '%s' has %d external face indices for %d triangles."),
			*Source.Name, Mesh.ExternalFaceIndices.Num(), Mesh.Triangles.Num());
		return false;
	}
	TSet<int32> Seen;
	for (const int32 External : Mesh.ExternalFaceIndices)
	{
		bool bDuplicate = false;
		Seen.Add(External, &bDuplicate);
		if (External < 0 || bDuplicate)
		{
			OutError = FString::Printf(TEXT("Support source '%s' has an invalid or duplicate external face index %d."),
				*Source.Name, External);
			return false;
		}
	}
	return true;
}
}

uint16 FLNPSupportFaceMap::Resolve(int32 ExternalFace) const
{
	if (IsUniform())
	{
		return UniformLayer;
	}
	return LayerByExternalFace.IsValidIndex(ExternalFace) ? LayerByExternalFace[ExternalFace] : LNPSupportLayers::NoLayer;
}

bool LNPSupportLayers::BuildLayers(
	TConstArrayView<FLNPBakeSupportSource> Sources,
	int32 CrustIndex,
	const FLNPSupportLayerSettings& Settings,
	FLNPSupportLayerSet& OutSet,
	FString& OutError)
{
	OutSet = FLNPSupportLayerSet();
	if (!Sources.IsValidIndex(CrustIndex))
	{
		OutError = TEXT("Support Layers need an identified crust source.");
		return false;
	}
	for (const FLNPBakeSupportSource& Source : Sources)
	{
		if (!ValidateFaceIndices(Source, OutError))
		{
			return false;
		}
	}

	// 지각은 Layer 0 하나다. 절벽 같은 non-walkable face도 지각이다.
	FLNPSupportLayer& Crust = OutSet.Layers.AddDefaulted_GetRef();
	Crust.SourceIndex = CrustIndex;
	Crust.Triangles.SetNumUninitialized(Sources[CrustIndex].Mesh.Triangles.Num());
	for (int32 TriangleIndex = 0; TriangleIndex < Crust.Triangles.Num(); ++TriangleIndex)
	{
		Crust.Triangles[TriangleIndex] = TriangleIndex;
	}
	Crust.MinExternalFace = GetMinExternalFace(Sources[CrustIndex].Mesh, Crust.Triangles);

	TArray<int32> Order;
	for (int32 Index = 0; Index < Sources.Num(); ++Index)
	{
		if (Index != CrustIndex)
		{
			Order.Add(Index);
		}
	}
	Order.Sort([&Sources](int32 A, int32 B)
	{
		return Sources[A].Key.Compare(Sources[B].Key, ESearchCase::CaseSensitive) < 0;
	});
	for (int32 Position = 1; Position < Order.Num(); ++Position)
	{
		if (Sources[Order[Position]].Key.Equals(Sources[Order[Position - 1]].Key, ESearchCase::CaseSensitive))
		{
			OutError = FString::Printf(TEXT("Support sources share the key '%s'."), *Sources[Order[Position]].Key);
			return false;
		}
	}

	for (const int32 SourceIndex : Order)
	{
		const FLNPBakeTriangleMesh& Mesh = Sources[SourceIndex].Mesh;
		TArray<TArray<int32>> Sheets = FindWalkableSheets(Mesh, Settings);
		TArray<int32> MinExternal;
		for (const TArray<int32>& Sheet : Sheets)
		{
			MinExternal.Add(GetMinExternalFace(Mesh, Sheet));
		}
		TArray<int32> SheetOrder;
		for (int32 Sheet = 0; Sheet < Sheets.Num(); ++Sheet)
		{
			SheetOrder.Add(Sheet);
		}
		SheetOrder.Sort([&MinExternal](int32 A, int32 B) { return MinExternal[A] < MinExternal[B]; });

		for (const int32 Sheet : SheetOrder)
		{
			FLNPSupportLayer& Layer = OutSet.Layers.AddDefaulted_GetRef();
			Layer.SourceIndex = SourceIndex;
			Layer.Triangles = MoveTemp(Sheets[Sheet]);
			Layer.MinExternalFace = MinExternal[Sheet];
		}
	}
	if (OutSet.Layers.Num() >= NoLayer)
	{
		OutError = FString::Printf(TEXT("%d Support Layers exceed the uint16 Layer ID range."), OutSet.Layers.Num());
		return false;
	}

	OutSet.FaceMaps.SetNum(Sources.Num());
	for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
	{
		const FLNPBakeTriangleMesh& Mesh = Sources[SourceIndex].Mesh;
		TArray<uint16> LayerByTriangle;
		LayerByTriangle.Init(NoLayer, Mesh.Triangles.Num());
		for (int32 LayerId = 0; LayerId < OutSet.Layers.Num(); ++LayerId)
		{
			if (OutSet.Layers[LayerId].SourceIndex == SourceIndex)
			{
				for (const int32 TriangleIndex : OutSet.Layers[LayerId].Triangles)
				{
					LayerByTriangle[TriangleIndex] = static_cast<uint16>(LayerId);
				}
			}
		}

		FLNPSupportFaceMap& FaceMap = OutSet.FaceMaps[SourceIndex];
		const bool bUniform = !LayerByTriangle.ContainsByPredicate(
			[First = LayerByTriangle[0]](uint16 Layer) { return Layer != First; });
		if (bUniform)
		{
			FaceMap.UniformLayer = LayerByTriangle[0];
			continue;
		}
		int32 MaxExternal = 0;
		for (const int32 External : Mesh.ExternalFaceIndices)
		{
			MaxExternal = FMath::Max(MaxExternal, External);
		}
		FaceMap.LayerByExternalFace.Init(NoLayer, MaxExternal + 1);
		for (int32 TriangleIndex = 0; TriangleIndex < Mesh.Triangles.Num(); ++TriangleIndex)
		{
			FaceMap.LayerByExternalFace[Mesh.ExternalFaceIndices[TriangleIndex]] = LayerByTriangle[TriangleIndex];
		}
	}
	return true;
}

FLNPBakeTriangleMesh LNPSupportLayers::MakeLayerMesh(const FLNPBakeTriangleMesh& SourceMesh, const FLNPSupportLayer& Layer)
{
	FLNPBakeTriangleMesh Mesh;
	Mesh.Vertices = SourceMesh.Vertices;
	Mesh.Triangles.Reserve(Layer.Triangles.Num());
	for (const int32 TriangleIndex : Layer.Triangles)
	{
		Mesh.Triangles.Add(SourceMesh.Triangles[TriangleIndex]);
		if (SourceMesh.ExternalFaceIndices.IsValidIndex(TriangleIndex))
		{
			Mesh.ExternalFaceIndices.Add(SourceMesh.ExternalFaceIndices[TriangleIndex]);
		}
	}
	return Mesh;
}
