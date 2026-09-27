// Copyright (c) 2026 LootNPop. All rights reserved.

// Phase 4a 지각 fixture LVI를 결정론적으로 생성하는 에디터 명령.
//
// 지각은 노이즈 없는 완전 구면 옥탄트 패치다. 기대값이 "반지름 R, 법선은 중심 방향"으로 해석적으로 정해져
// Atlas·exact 오차와 이음매 일치를 판정하기 쉽다. 입구 구멍 하나를 뚫어 coverage hole 사례를 만든다.
// 지각 외 fixture(분리 sheet, 양면 판, 음수·비균일 scale 슬래브)는 삼각형 추출·transform·법선 검증용이다.
//
// 예: LNP.SurfaceNav.BuildCrustFixture
//     기존 LVI가 있으면 거부한다. 다시 만들려면 LVI를 지운 뒤 실행한다(메시는 제자리 갱신).

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Config/LNPSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

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

	FVector DirectionFromLatAz(double LatDeg, double AzDeg)
	{
		const double Lat = FMath::DegreesToRadians(LatDeg);
		const double Az = FMath::DegreesToRadians(AzDeg);
		return FVector(FMath::Cos(Lat) * FMath::Cos(Az), FMath::Cos(Lat) * FMath::Sin(Az), FMath::Sin(Lat));
	}

	struct FFixtureMesh
	{
		TArray<FVector> Positions;
		TArray<FIntVector> Triangles;

		int32 AddVertex(const FVector& Position)
		{
			return Positions.Add(Position);
		}

		/** 엔진 삼각형 법선 규약((P1-P2)×(P0-P2))이 Up과 같은 쪽을 향하도록 winding을 맞춘다. */
		void AddTriangle(int32 A, int32 B, int32 C, const FVector& Up)
		{
			const FVector Normal = FVector::CrossProduct(Positions[B] - Positions[C], Positions[A] - Positions[C]);
			if (FVector::DotProduct(Normal, Up) >= 0.0)
			{
				Triangles.Emplace(A, B, C);
			}
			else
			{
				Triangles.Emplace(A, C, B);
			}
		}
	};

	/** 원점 중심 반지름 Radius의 (+X,+Y,+Z) 옥탄트 패치. 법선은 중심 방향이고, HoleDir 주변 삼각형은 뺀다. */
	FFixtureMesh BuildCrust(double Radius, const FVector& HoleDir)
	{
		FFixtureMesh Mesh;
		const int32 N = CrustSubdivisions;
		TArray<int32> Index;
		Index.SetNumUninitialized((N + 1) * (N + 1));
		for (int32 J = 0; J <= N; ++J)
		{
			for (int32 I = 0; I + J <= N; ++I)
			{
				const FVector Direction = FVector(I, J, N - I - J).GetSafeNormal();
				Index[J * (N + 1) + I] = Mesh.AddVertex(Direction * Radius);
			}
		}

		const double HoleCos = FMath::Cos(FMath::DegreesToRadians(HoleAngleDeg));
		auto AddIfOutsideHole = [&Mesh, &HoleDir, HoleCos](int32 A, int32 B, int32 C)
		{
			const FVector Centroid = (Mesh.Positions[A] + Mesh.Positions[B] + Mesh.Positions[C]) / 3.0;
			const FVector Direction = Centroid.GetSafeNormal();
			if (FVector::DotProduct(Direction, HoleDir) > HoleCos)
			{
				return;
			}
			Mesh.AddTriangle(A, B, C, -Direction);
		};

		for (int32 J = 0; J < N; ++J)
		{
			for (int32 I = 0; I + J < N; ++I)
			{
				const int32 V00 = Index[J * (N + 1) + I];
				const int32 V10 = Index[J * (N + 1) + I + 1];
				const int32 V01 = Index[(J + 1) * (N + 1) + I];
				AddIfOutsideHole(V00, V10, V01);
				if (I + J < N - 1)
				{
					const int32 V11 = Index[(J + 1) * (N + 1) + I + 1];
					AddIfOutsideHole(V10, V11, V01);
				}
			}
		}
		return Mesh;
	}

	/** Center 방향, 반지름 Radius의 구면 캡을 Mesh에 추가한다. 법선은 중심 방향이다. */
	void AppendCap(FFixtureMesh& Mesh, const FVector& Center, double Radius, double AngleDeg, bool bDoubleSided)
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

	/** 원점 중심 100cm 정육면체. 법선은 바깥쪽이다. 엔진 Cube는 단순 box 충돌이라 exact가 trimesh를 맞히지 않는다. */
	FFixtureMesh BuildUnitBox()
	{
		FFixtureMesh Mesh;
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			Mesh.AddVertex(FVector(Corner & 1 ? 50.0 : -50.0, Corner & 2 ? 50.0 : -50.0, Corner & 4 ? 50.0 : -50.0));
		}
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const int32 AxisBit = 1 << Axis;
			const int32 BitA = 1 << ((Axis + 1) % 3);
			const int32 BitB = 1 << ((Axis + 2) % 3);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const int32 Base = Side ? AxisBit : 0;
				FVector Up = FVector::ZeroVector;
				Up[Axis] = Side ? 1.0 : -1.0;
				Mesh.AddTriangle(Base, Base | BitA, Base | BitA | BitB, Up);
				Mesh.AddTriangle(Base, Base | BitA | BitB, Base | BitB, Up);
			}
		}
		return Mesh;
	}

	FMeshDescription ToMeshDescription(const FFixtureMesh& Mesh)
	{
		FMeshDescription Description;
		FStaticMeshAttributes Attributes(Description);
		Attributes.Register();

		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Fixture");

		// 구멍으로 빠진 삼각형만 참조하던 정점은 만들지 않는다.
		TArray<FVertexID> VertexIds;
		VertexIds.Init(INDEX_NONE, Mesh.Positions.Num());
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		for (const FIntVector& Triangle : Mesh.Triangles)
		{
			TArray<FVertexInstanceID, TFixedAllocator<3>> Instances;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 Source = Triangle[Corner];
				if (VertexIds[Source] == INDEX_NONE)
				{
					VertexIds[Source] = Description.CreateVertex();
					Positions[VertexIds[Source]] = FVector3f(Mesh.Positions[Source]);
				}
				Instances.Add(Description.CreateVertexInstance(VertexIds[Source]));
			}
			Description.CreateTriangle(Group, Instances);
		}

		FStaticMeshOperations::ComputeTriangleTangentsAndNormals(Description);
		FStaticMeshOperations::ComputeTangentsAndNormals(Description,
			EComputeNTBsFlags::Normals | EComputeNTBsFlags::Tangents | EComputeNTBsFlags::WeightedNTBs);
		return Description;
	}

	bool SavePackage(UPackage& Package, UObject& Asset, const FString& Extension)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(Package.GetName(), Extension);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(&Package, &Asset, *Filename, Args);
	}

	/** complex-as-simple 충돌을 가진 StaticMesh 에셋을 만들거나 제자리 갱신하고 저장한다. */
	UStaticMesh* WriteStaticMesh(const FString& AssetName, const FFixtureMesh& Mesh)
	{
		const FString PackageName = FString::Printf(TEXT("%s/%s"), FixtureFolder, *AssetName);
		UPackage* Package = CreatePackage(*PackageName);
		Package->FullyLoad();

		UStaticMesh* StaticMesh = FindObject<UStaticMesh>(Package, *AssetName);
		if (!StaticMesh)
		{
			StaticMesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(StaticMesh);
		}
		StaticMesh->Modify();
		if (StaticMesh->GetNumSourceModels() == 0)
		{
			StaticMesh->AddSourceModel();
		}
		FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(0);
		SourceModel.BuildSettings.bRecomputeNormals = false;
		SourceModel.BuildSettings.bRecomputeTangents = false;
		SourceModel.BuildSettings.bGenerateLightmapUVs = false;

		StaticMesh->GetStaticMaterials().Reset();
		StaticMesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface), TEXT("Fixture")));
		StaticMesh->CreateMeshDescription(0, ToMeshDescription(Mesh));
		StaticMesh->CommitMeshDescription(0);

		StaticMesh->CreateBodySetup();
		StaticMesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
		StaticMesh->Build(false);
		StaticMesh->PostEditChange();

		if (!SavePackage(*Package, *StaticMesh, FPackageName::GetAssetPackageExtension()))
		{
			UE_LOG(LogLNPSurfaceFixture, Error, TEXT("[CrustFixture] Failed to save %s"), *PackageName);
			return nullptr;
		}
		return StaticMesh;
	}

	AStaticMeshActor* SpawnMeshActor(
		UWorld& World,
		UStaticMesh& Mesh,
		const FString& Label,
		const FTransform& Transform,
		TArray<FName> Tags,
		FName Profile)
	{
		AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(
			Transform.GetLocation(), Transform.Rotator());
		if (!Actor)
		{
			return nullptr;
		}
		Actor->SetActorScale3D(Transform.GetScale3D());
		Actor->SetActorLabel(Label);
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		Component->SetStaticMesh(&Mesh);
		Component->ComponentTags = MoveTemp(Tags);
		Component->SetCollisionProfileName(Profile);
		Component->SetCanEverAffectNavigation(false);
		return Actor;
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
		const FVector HoleDir = DirectionFromLatAz(30.0, 45.0);
		const FVector SplitDirA = DirectionFromLatAz(35.0, 20.0);
		const FVector SplitDirB = DirectionFromLatAz(35.0, 26.0);
		const FVector DoubleSidedDir = DirectionFromLatAz(25.0, 70.0);
		const FVector SlabDir = DirectionFromLatAz(38.0, 60.0);

		FFixtureMesh SplitSheet;
		AppendCap(SplitSheet, SplitDirA, Radius - InnerOffset, 1.0, false);
		AppendCap(SplitSheet, SplitDirB, Radius - InnerOffset, 1.0, false);
		FFixtureMesh DoubleSided;
		AppendCap(DoubleSided, DoubleSidedDir, Radius - InnerOffset, 1.0, true);

		UStaticMesh* CrustMesh = WriteStaticMesh(TEXT("SM_FixtureCrust_R30000"), BuildCrust(Radius, HoleDir));
		UStaticMesh* SplitMesh = WriteStaticMesh(TEXT("SM_FixtureSplitSheet"), SplitSheet);
		UStaticMesh* DoubleSidedMesh = WriteStaticMesh(TEXT("SM_FixtureDoubleSidedPlate"), DoubleSided);
		UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, SphereMeshPath);
		UStaticMesh* SlabMesh = WriteStaticMesh(TEXT("SM_FixtureSlab"), BuildUnitBox());
		if (!CrustMesh || !SplitMesh || !DoubleSidedMesh || !SphereMesh || !SlabMesh)
		{
			UE_LOG(LogLNPSurfaceFixture, Error, TEXT("[CrustFixture] Mesh creation failed"));
			return;
		}

		UPackage* LevelPackage = CreatePackage(*LevelPackageName);
		UWorld::InitializationValues Init;
		Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		UWorld* World = UWorld::CreateWorld(EWorldType::Inactive, false, FName(LevelName), LevelPackage, true,
			ERHIFeatureLevel::Num, &Init);
		World->SetFlags(RF_Public | RF_Standalone);

		const TArray<FName> TerrainTags = {SupportTag, BlockerTag, StaticTag};
		SpawnMeshActor(*World, *CrustMesh, TEXT("FX_Crust"), FTransform::Identity, TerrainTags, TEXT("LNPStaticTerrain"));
		SpawnMeshActor(*World, *SplitMesh, TEXT("FX_SplitSheet"), FTransform::Identity, TerrainTags, TEXT("LNPStaticTerrain"));
		SpawnMeshActor(*World, *DoubleSidedMesh, TEXT("FX_DoubleSidedPlate"), FTransform::Identity, TerrainTags,
			TEXT("LNPStaticTerrain"));

		// 100cm 정육면체를 음수·비균일 scale로 400×300×20cm 슬래브로 만들고 로컬 Z를 중심 쪽 Up에 맞춘다.
		const FTransform SlabTransform(
			FRotationMatrix::MakeFromZ(-SlabDir).ToQuat(),
			SlabDir * (Radius - InnerOffset),
			FVector(4.0, -3.0, 0.2));
		SpawnMeshActor(*World, *SlabMesh, TEXT("FX_NegativeScaleSlab"), SlabTransform, TerrainTags, TEXT("LNPStaticTerrain"));

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
			SpawnMeshActor(*World, *SphereMesh, Probe.Key, FTransform(Probe.Value * (Radius - 100.0)),
				{DecorationTag}, TEXT("LNPDecoration"));
		}

		FAssetRegistryModule::AssetCreated(World);
		const bool bSaved = SavePackage(*LevelPackage, *World, FPackageName::GetMapPackageExtension());
		World->DestroyWorld(false);
		World->RemoveFromRoot();

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
