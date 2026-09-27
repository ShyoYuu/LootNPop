// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AStaticMeshActor;
class UPackage;
class UStaticMesh;
class UWorld;

/**
 * 에디터가 코드로 만드는 fixture·동굴 키트 메시의 삼각형 목록.
 * complex-as-simple StaticMesh 에셋으로 저장해 exact query와 베이커가 같은 삼각형을 보게 한다.
 */
struct FLNPFixtureMesh
{
	TArray<FVector> Positions;
	TArray<FIntVector> Triangles;

	int32 AddVertex(const FVector& Position) { return Positions.Add(Position); }

	/** 엔진 삼각형 법선 규약((P1-P2)×(P0-P2))이 Up과 같은 쪽을 향하도록 winding을 맞춘다. */
	void AddTriangle(int32 A, int32 B, int32 C, const FVector& Up);

	/** 볼록 사각형(둘레 순서)을 두 삼각형으로 추가한다. bDoubleSided면 반대쪽 면도 추가한다. */
	void AddQuad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Up, bool bDoubleSided);

	/**
	 * 볼록 영역(모든 평면에서 PlaneDot < 0인 공간) 안쪽 부분을 잘라낸다. 영역 경계를 가로지르는 삼각형은
	 * 평면으로 쪼개 바깥 조각만 남긴다. 영역과 무관한 정점은 좌표를 바꾸지 않으므로 이음매 정점이 유지된다.
	 * 반환값은 잘라낸 원본 삼각형 수다.
	 */
	int32 RemoveInsideConvex(TConstArrayView<FPlane> Planes);
};

namespace LNPFixtureMesh
{
	/** 위도(z=0 적도에서 +Z 쪽)·방위(+X에서 +Y 쪽) 각도의 단위 방향. */
	inline FVector DirectionFromLatAz(double LatDeg, double AzDeg)
	{
		const double Lat = FMath::DegreesToRadians(LatDeg);
		const double Az = FMath::DegreesToRadians(AzDeg);
		return FVector(FMath::Cos(Lat) * FMath::Cos(Az), FMath::Cos(Lat) * FMath::Sin(Az), FMath::Sin(Lat));
	}

	/**
	 * 원점 중심 반지름 Radius의 (+X,+Y,+Z) 옥탄트 구면 패치. 꼭짓점 중심 삼각 격자(i, j, N-i-j)라서 이음매 정점은
	 * 이음매 평면 위에 정확히 있다. 법선은 중심 방향이다. KeepTriangle이 있으면 무게중심 방향으로 삼각형을 거른다.
	 */
	FLNPFixtureMesh BuildSphereOctant(double Radius, int32 Subdivisions,
		TFunctionRef<bool(const FVector& CentroidDirection)> KeepTriangle = [](const FVector&) { return true; });

	/** 상자 면 비트. 축 A의 음/양 면이 (1 << 2A) / (1 << 2A+1)이다. */
	namespace EBoxFace
	{
		constexpr uint8 NegX = 1 << 0, PosX = 1 << 1, NegY = 1 << 2, PosY = 1 << 3, NegZ = 1 << 4, PosZ = 1 << 5;
		constexpr uint8 All = 0x3F;
	}

	/** 원점 중심 100cm 정육면체의 FaceMask 면. 법선은 바깥쪽이다. */
	FLNPFixtureMesh BuildUnitBox(uint8 FaceMask = EBoxFace::All);

	bool SavePackage(UPackage& Package, UObject& Asset, const FString& Extension);

	/**
	 * PackageFolder/AssetName에 complex-as-simple 충돌을 가진 StaticMesh 에셋을 만들거나 제자리 갱신하고 저장한다.
	 * Sockets는 이름과 메시 로컬 transform이며, 기존 소켓은 모두 이것으로 바뀐다.
	 */
	UStaticMesh* WriteStaticMesh(const FString& PackageFolder, const FString& AssetName, const FLNPFixtureMesh& Mesh,
		TConstArrayView<TPair<FName, FTransform>> Sockets = {});

	AStaticMeshActor* SpawnMeshActor(
		UWorld& World,
		UStaticMesh& Mesh,
		const FString& Label,
		const FTransform& Transform,
		TArray<FName> Tags,
		FName Profile);

	/** PackageName에 저장할 빈 editor World를 만든다. 호출자가 액터를 채운 뒤 SaveWorld로 저장·파괴한다. */
	UWorld* CreateLevelWorld(const FString& PackageName);
	bool SaveWorld(UWorld& World);
}
