// Copyright (c) 2026 LootNPop. All rights reserved.

// Phase 4a 지각 fixture LVI를 결정론적으로 생성하는 에디터 명령.
//
// 지각은 노이즈 없는 완전 구면 옥탄트 패치다. 기대값이 "반지름 R, 법선은 중심 방향"으로 해석적으로 정해져
// Atlas·exact 오차와 이음매 일치를 판정하기 쉽다. 입구 구멍 하나를 뚫어 coverage hole 사례를 만든다.
// 지각 외 fixture(분리 sheet, 양면 판, 음수·비균일 scale 슬래브)는 삼각형 추출·transform·법선 검증용이다.
//
// 예: LNP.SurfaceNav.BuildCrustFixture
//     기존 LVI가 있으면 거부한다. 다시 만들려면 LVI를 지운 뒤 실행한다(메시는 제자리 갱신).

#include "Config/LNPSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "SurfaceNavigation/LNPFixtureMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPSurfaceFixture, Log, All);

namespace LNPSurfaceFixture
{
	const TCHAR* const FixtureFolder = TEXT("/Game/Maps/SurfaceNavigation/Fixtures");
	const TCHAR* const LevelName = TEXT("LVI_Octant_Fixture_Crust");
	const TCHAR* const SphereMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");

	/** 지각 격자 분할 수. Atlas 분할 수와 배수 관계가 아니어야 샘플이 우연히 정점에만 떨어지지 않는다. */
	constexpr int32 CrustSubdivisions = 96;
	/** 지각 입구 구멍의 각반지름. 30,000cm에서 약 785cm. */
	constexpr double HoleAngleDeg = 1.5;
	/** 비지각 fixture를 지각 안쪽(중심 쪽)으로 띄우는 거리. */
	constexpr double InnerOffset = 800.0;

	const FName SupportTag(TEXT("LNP.Surface.Support"));
	const FName BlockerTag(TEXT("LNP.Surface.Blocker"));
	const FName StaticTag(TEXT("LNP.Surface.Static"));
	const FName DecorationTag(TEXT("LNP.Surface.Decoration"));

	/** 원점 중심 반지름 Radius의 (+X,+Y,+Z) 옥탄트 패치. 법선은 중심 방향이고, HoleDir 주변 삼각형은 뺀다. */
	FLNPFixtureMesh BuildCrust(double Radius, const FVector& HoleDir)
	{
		const double HoleCos = FMath::Cos(FMath::DegreesToRadians(HoleAngleDeg));
		return LNPFixtureMesh::BuildSphereOctant(Radius, CrustSubdivisions,
			[&HoleDir, HoleCos](const FVector& Direction) { return FVector::DotProduct(Direction, HoleDir) <= HoleCos; });
	}

	/** Center 방향, 반지름 Radius의 구면 캡을 Mesh에 추가한다. 법선은 중심 방향이다. */
	void AppendCap(FLNPFixtureMesh& Mesh, const FVector& Center, double Radius, double AngleDeg, bool bDoubleSided)
	{
		constexpr int32 Rings = 6;
		constexpr int32 Segments = 24;
		FVector TangentA, TangentB;
		Center.FindBestAxisVectors(TangentA, TangentB);

		const int32 CenterIndex = Mesh.AddVertex(Center * Radius);
		TArray<int32> Previous;
		for (int32 Ring = 1; Ring <= Rings; ++Ring)
		{
			const double Angle = FMath::DegreesToRadians(AngleDeg) * Ring / Rings;
			TArray<int32> Current;
			for (int32 Segment = 0; Segment < Segments; ++Segment)
			{
				const double Phi = UE_TWO_PI * Segment / Segments;
				const FVector Direction = Center * FMath::Cos(Angle)
					+ (TangentA * FMath::Cos(Phi) + TangentB * FMath::Sin(Phi)) * FMath::Sin(Angle);
				Current.Add(Mesh.AddVertex(Direction * Radius));
			}

			for (int32 Segment = 0; Segment < Segments; ++Segment)
			{
				const int32 Next = (Segment + 1) % Segments;
				auto Add = [&Mesh, &Center, bDoubleSided](int32 A, int32 B, int32 C)
				{
					Mesh.AddTriangle(A, B, C, -Center);
					if (bDoubleSided)
					{
						Mesh.AddTriangle(A, B, C, Center);
					}
				};
				if (Ring == 1)
				{
					Add(CenterIndex, Current[Segment], Current[Next]);
				}
				else
				{
					Add(Previous[Segment], Current[Segment], Current[Next]);
					Add(Previous[Segment], Current[Next], Previous[Next]);
				}
			}
			Previous = MoveTemp(Current);
		}
	}

	void Run()
	{
		const double Radius = GetDefault<ULNPSettings>()->SphereRadius;
		const FString LevelPackageName = FString::Printf(TEXT("%s/%s"), FixtureFolder, LevelName);
		if (FPackageName::DoesPackageExist(LevelPackageName))
		{
			UE_LOG(LogLNPSurfaceFixture, Error,
				TEXT("[CrustFixture] %s already exists. Delete it first to rebuild."), *LevelPackageName);
			return;
		}

		// 모든 fixture는 옥탄트 내부(위도 25~40°)에 두어 이음매·꼭짓점과 int16 캡에서 떨어뜨린다.
		const FVector HoleDir = LNPFixtureMesh::DirectionFromLatAz(30.0, 45.0);
		const FVector SplitDirA = LNPFixtureMesh::DirectionFromLatAz(35.0, 20.0);
		const FVector SplitDirB = LNPFixtureMesh::DirectionFromLatAz(35.0, 26.0);
		const FVector DoubleSidedDir = LNPFixtureMesh::DirectionFromLatAz(25.0, 70.0);
		const FVector SlabDir = LNPFixtureMesh::DirectionFromLatAz(38.0, 60.0);

		FLNPFixtureMesh SplitSheet;
		AppendCap(SplitSheet, SplitDirA, Radius - InnerOffset, 1.0, false);
		AppendCap(SplitSheet, SplitDirB, Radius - InnerOffset, 1.0, false);
		FLNPFixtureMesh DoubleSided;
		AppendCap(DoubleSided, DoubleSidedDir, Radius - InnerOffset, 1.0, true);

		UStaticMesh* CrustMesh = LNPFixtureMesh::WriteStaticMesh(FixtureFolder, TEXT("SM_FixtureCrust_R30000"), BuildCrust(Radius, HoleDir));
		UStaticMesh* SplitMesh = LNPFixtureMesh::WriteStaticMesh(FixtureFolder, TEXT("SM_FixtureSplitSheet"), SplitSheet);
		UStaticMesh* DoubleSidedMesh = LNPFixtureMesh::WriteStaticMesh(FixtureFolder, TEXT("SM_FixtureDoubleSidedPlate"), DoubleSided);
		UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, SphereMeshPath);
		UStaticMesh* SlabMesh = LNPFixtureMesh::WriteStaticMesh(FixtureFolder, TEXT("SM_FixtureSlab"), LNPFixtureMesh::BuildUnitBox());
		if (!CrustMesh || !SplitMesh || !DoubleSidedMesh || !SphereMesh || !SlabMesh)
		{
			UE_LOG(LogLNPSurfaceFixture, Error, TEXT("[CrustFixture] Mesh creation failed"));
			return;
		}

		UWorld* World = LNPFixtureMesh::CreateLevelWorld(LevelPackageName);

		const TArray<FName> TerrainTags = {SupportTag, BlockerTag, StaticTag};
		LNPFixtureMesh::SpawnMeshActor(*World, *CrustMesh, TEXT("FX_Crust"), FTransform::Identity, TerrainTags, TEXT("LNPStaticTerrain"));
		LNPFixtureMesh::SpawnMeshActor(*World, *SplitMesh, TEXT("FX_SplitSheet"), FTransform::Identity, TerrainTags, TEXT("LNPStaticTerrain"));
		LNPFixtureMesh::SpawnMeshActor(*World, *DoubleSidedMesh, TEXT("FX_DoubleSidedPlate"), FTransform::Identity, TerrainTags,
			TEXT("LNPStaticTerrain"));

		// 100cm 정육면체를 음수·비균일 scale로 400×300×20cm 슬래브로 만들고 로컬 Z를 중심 쪽 Up에 맞춘다.
		const FTransform SlabTransform(
			FRotationMatrix::MakeFromZ(-SlabDir).ToQuat(),
			SlabDir * (Radius - InnerOffset),
			FVector(4.0, -3.0, 0.2));
		LNPFixtureMesh::SpawnMeshActor(*World, *SlabMesh, TEXT("FX_NegativeScaleSlab"), SlabTransform, TerrainTags, TEXT("LNPStaticTerrain"));

		// 이음매 변 중점·꼭짓점·구멍 oracle 위치 표시. 지각 100cm 안쪽에 둔다.
		const TArray<TPair<FString, FVector>> Probes = {
			{TEXT("Probe_SeamMid_XY"), FVector(1.0, 1.0, 0.0).GetSafeNormal()},
			{TEXT("Probe_SeamMid_YZ"), FVector(0.0, 1.0, 1.0).GetSafeNormal()},
			{TEXT("Probe_SeamMid_ZX"), FVector(1.0, 0.0, 1.0).GetSafeNormal()},
			{TEXT("Probe_Corner_X"), FVector::XAxisVector},
			{TEXT("Probe_Corner_Y"), FVector::YAxisVector},
			{TEXT("Probe_Corner_Z"), FVector::ZAxisVector},
			{TEXT("Probe_Hole"), HoleDir},
		};
		for (const TPair<FString, FVector>& Probe : Probes)
		{
			LNPFixtureMesh::SpawnMeshActor(*World, *SphereMesh, Probe.Key, FTransform(Probe.Value * (Radius - 100.0)),
				{DecorationTag}, TEXT("LNPDecoration"));
		}

		const bool bSaved = LNPFixtureMesh::SaveWorld(*World);

		UE_LOG(LogLNPSurfaceFixture, Display,
			TEXT("[CrustFixture] %s %s | Radius=%.0f Subdivisions=%d CrustTriangles=%d Hole=%.1fdeg"),
			*LevelPackageName, bSaved ? TEXT("saved") : TEXT("FAILED to save"),
			Radius, CrustSubdivisions, CrustMesh->GetNumTriangles(0), HoleAngleDeg);
	}

	static FAutoConsoleCommand Command(
		TEXT("LNP.SurfaceNav.BuildCrustFixture"),
		TEXT("Create the Phase 4a crust fixture LVI (perfect-sphere octant crust with an entrance hole, ")
		TEXT("split sheet, double-sided plate, negative-scale slab, seam probes) under /Game/Maps/SurfaceNavigation/Fixtures. ")
		TEXT("Refuses to overwrite an existing LVI."),
		FConsoleCommandDelegate::CreateStatic(&Run));
}
