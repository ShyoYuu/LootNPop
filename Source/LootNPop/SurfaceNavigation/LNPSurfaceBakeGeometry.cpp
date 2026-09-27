// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPSurfaceBakeGeometry.h"

namespace
{
constexpr uint8 AllSeamPlanes = 0b111;
}

FVector3d FLNPBakeTriangleMesh::GetTriangleNormal(int32 TriangleIndex) const
{
	const FIntVector3& Triangle = Triangles[TriangleIndex];
	const FVector3d& A = Vertices[Triangle.X];
	const FVector3d& B = Vertices[Triangle.Y];
	const FVector3d& C = Vertices[Triangle.Z];
	return FVector3d::CrossProduct(B - A, C - A).GetSafeNormal();
}

bool LNPSurfaceBake::ValidateSupportSource(const FLNPBakeSupportSource& Source, FString& OutError)
{
	if (Source.Mesh.Triangles.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Support source '%s' has no collision triangles."), *Source.Name);
		return false;
	}

	for (const FIntVector3& Triangle : Source.Mesh.Triangles)
	{
		if (!Source.Mesh.Vertices.IsValidIndex(Triangle.X)
			|| !Source.Mesh.Vertices.IsValidIndex(Triangle.Y)
			|| !Source.Mesh.Vertices.IsValidIndex(Triangle.Z))
		{
			OutError = FString::Printf(TEXT("Support source '%s' has an out-of-range triangle index."), *Source.Name);
			return false;
		}
	}

	for (const FVector3d& Vertex : Source.Mesh.Vertices)
	{
		if (!FMath::IsFinite(Vertex.X) || !FMath::IsFinite(Vertex.Y) || !FMath::IsFinite(Vertex.Z))
		{
			OutError = FString::Printf(TEXT("Support source '%s' has a non-finite vertex."), *Source.Name);
			return false;
		}
		if (Vertex.GetMin() < -SeamPlaneTolerance)
		{
			OutError = FString::Printf(
				TEXT("Support source '%s' crosses the octant boundary at (%.1f, %.1f, %.1f)."),
				*Source.Name, Vertex.X, Vertex.Y, Vertex.Z);
			return false;
		}
		if (Vertex.GetAbsMax() > ReplicatedPositionCap)
		{
			OutError = FString::Printf(
				TEXT("Support source '%s' vertex (%.1f, %.1f, %.1f) exceeds the int16 replicated position cap %.0f."),
				*Source.Name, Vertex.X, Vertex.Y, Vertex.Z, ReplicatedPositionCap);
			return false;
		}
	}
	return true;
}

uint8 LNPSurfaceBake::GetTouchedSeamPlanes(const FLNPBakeTriangleMesh& Mesh)
{
	uint8 Touched = 0;
	for (const FVector3d& Vertex : Mesh.Vertices)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (FMath::Abs(Vertex[Axis]) <= SeamPlaneTolerance)
			{
				Touched |= 1 << Axis;
			}
		}
		if (Touched == AllSeamPlanes)
		{
			break;
		}
	}
	return Touched;
}

bool LNPSurfaceBake::IdentifyCrust(
	TConstArrayView<FLNPBakeSupportSource> Sources,
	int32& OutCrustIndex,
	FString& OutError)
{
	OutCrustIndex = INDEX_NONE;
	TArray<FString> Candidates;
	for (int32 Index = 0; Index < Sources.Num(); ++Index)
	{
		if (GetTouchedSeamPlanes(Sources[Index].Mesh) == AllSeamPlanes)
		{
			OutCrustIndex = Index;
			Candidates.Add(Sources[Index].Name);
		}
	}

	if (Candidates.Num() != 1)
	{
		OutCrustIndex = INDEX_NONE;
		OutError = Candidates.IsEmpty()
			? FString::Printf(
				TEXT("No Support source touches all three seam planes, so the octant has no crust (%d Support source(s))."),
				Sources.Num())
			: FString::Printf(
				TEXT("%d Support sources touch all three seam planes; exactly one crust is allowed:\n%s"),
				Candidates.Num(), *FString::Join(Candidates, TEXT("\n")));
		return false;
	}
	return true;
}
