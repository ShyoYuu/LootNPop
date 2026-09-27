// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPFixtureMesh.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogLNPFixtureMesh, Log, All);

void FLNPFixtureMesh::AddTriangle(int32 A, int32 B, int32 C, const FVector& Up)
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

void FLNPFixtureMesh::AddQuad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Up,
	bool bDoubleSided)
{
	const int32 IA = AddVertex(A);
	const int32 IB = AddVertex(B);
	const int32 IC = AddVertex(C);
	const int32 ID = AddVertex(D);
	AddTriangle(IA, IB, IC, Up);
	AddTriangle(IA, IC, ID, Up);
	if (bDoubleSided)
	{
		AddTriangle(IA, IB, IC, -Up);
		AddTriangle(IA, IC, ID, -Up);
	}
}

int32 FLNPFixtureMesh::RemoveInsideConvex(TConstArrayView<FPlane> Planes)
{
	// 새 정점은 모서리 양 끝 좌표를 정렬해 계산하므로 이웃 삼각형이 같은 모서리를 쪼개면 같은 점이 나온다. 공유한다.
	TMap<FVector, int32> SplitVertices;
	auto SplitEdge = [this, &SplitVertices](const FVector& P, const FVector& Q, const FPlane& Plane) -> int32
	{
		const bool bSwap = P.X > Q.X || (P.X == Q.X && (P.Y > Q.Y || (P.Y == Q.Y && P.Z > Q.Z)));
		const FVector& A = bSwap ? Q : P;
		const FVector& B = bSwap ? P : Q;
		const double DA = Plane.PlaneDot(A);
		const double DB = Plane.PlaneDot(B);
		const FVector Point = A + (B - A) * (DA / (DA - DB));
		if (const int32* Existing = SplitVertices.Find(Point))
		{
			return *Existing;
		}
		const int32 Index = AddVertex(Point);
		SplitVertices.Add(Point, Index);
		return Index;
	};

	TArray<FIntVector> Kept;
	Kept.Reserve(Triangles.Num());
	int32 RemovedCount = 0;
	for (const FIntVector& Triangle : Triangles)
	{
		// 어느 한 평면의 바깥(>= 0)에 세 정점이 모두 있으면 영역과 겹치지 않는다.
		bool bOutside = false;
		for (const FPlane& Plane : Planes)
		{
			if (Plane.PlaneDot(Positions[Triangle.X]) >= 0.0 && Plane.PlaneDot(Positions[Triangle.Y]) >= 0.0
				&& Plane.PlaneDot(Positions[Triangle.Z]) >= 0.0)
			{
				bOutside = true;
				break;
			}
		}
		if (bOutside)
		{
			Kept.Add(Triangle);
			continue;
		}

		// 평면마다 바깥 조각을 떼어 남기고 안쪽 조각으로 다음 평면을 계속 자른다. 마지막 안쪽 조각은 버린다.
		++RemovedCount;
		TArray<int32, TInlineAllocator<8>> Inside = {Triangle.X, Triangle.Y, Triangle.Z};
		for (const FPlane& Plane : Planes)
		{
			TArray<int32, TInlineAllocator<8>> NextInside;
			TArray<int32, TInlineAllocator<8>> Outside;
			for (int32 Corner = 0; Corner < Inside.Num(); ++Corner)
			{
				const int32 Current = Inside[Corner];
				const int32 Next = Inside[(Corner + 1) % Inside.Num()];
				const double DC = Plane.PlaneDot(Positions[Current]);
				const double DN = Plane.PlaneDot(Positions[Next]);
				(DC < 0.0 ? NextInside : Outside).Add(Current);
				if ((DC < 0.0) != (DN < 0.0) && DC != 0.0 && DN != 0.0)
				{
					const int32 Split = SplitEdge(Positions[Current], Positions[Next], Plane);
					NextInside.Add(Split);
					Outside.Add(Split);
				}
			}
			// 볼록 다각형이므로 첫 정점 기준 fan이 winding을 유지한다.
			for (int32 Corner = 1; Corner + 1 < Outside.Num(); ++Corner)
			{
				const FVector Cross = FVector::CrossProduct(
					Positions[Outside[Corner]] - Positions[Outside[0]], Positions[Outside[Corner + 1]] - Positions[Outside[0]]);
				if (Cross.SizeSquared() > 1e-8)
				{
					Kept.Emplace(Outside[0], Outside[Corner], Outside[Corner + 1]);
				}
			}
			Inside = MoveTemp(NextInside);
			if (Inside.Num() < 3)
			{
				break;
			}
		}
	}
	Triangles = MoveTemp(Kept);
	return RemovedCount;
}

namespace LNPFixtureMesh
{
	FLNPFixtureMesh BuildSphereOctant(double Radius, int32 Subdivisions,
		TFunctionRef<bool(const FVector& CentroidDirection)> KeepTriangle)
	{
		FLNPFixtureMesh Mesh;
		const int32 N = Subdivisions;
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

		auto AddIfKept = [&Mesh, &KeepTriangle](int32 A, int32 B, int32 C)
		{
			const FVector Centroid = (Mesh.Positions[A] + Mesh.Positions[B] + Mesh.Positions[C]) / 3.0;
			const FVector Direction = Centroid.GetSafeNormal();
			if (KeepTriangle(Direction))
			{
				Mesh.AddTriangle(A, B, C, -Direction);
			}
		};

		for (int32 J = 0; J < N; ++J)
		{
			for (int32 I = 0; I + J < N; ++I)
			{
				const int32 V00 = Index[J * (N + 1) + I];
				const int32 V10 = Index[J * (N + 1) + I + 1];
				const int32 V01 = Index[(J + 1) * (N + 1) + I];
				AddIfKept(V00, V10, V01);
				if (I + J < N - 1)
				{
					const int32 V11 = Index[(J + 1) * (N + 1) + I + 1];
					AddIfKept(V10, V11, V01);
				}
			}
		}
		return Mesh;
	}

	FLNPFixtureMesh BuildUnitBox(uint8 FaceMask)
	{
		FLNPFixtureMesh Mesh;
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
				if (!(FaceMask & (1 << (Axis * 2 + Side))))
				{
					continue;
				}
				const int32 Base = Side ? AxisBit : 0;
				FVector Up = FVector::ZeroVector;
				Up[Axis] = Side ? 1.0 : -1.0;
				Mesh.AddTriangle(Base, Base | BitA, Base | BitA | BitB, Up);
				Mesh.AddTriangle(Base, Base | BitA | BitB, Base | BitB, Up);
			}
		}
		return Mesh;
	}

	FMeshDescription ToMeshDescription(const FLNPFixtureMesh& Mesh)
	{
		FMeshDescription Description;
		FStaticMeshAttributes Attributes(Description);
		Attributes.Register();

		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Fixture");

		// 잘려 나간 삼각형만 참조하던 정점은 만들지 않는다.
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

	UStaticMesh* WriteStaticMesh(const FString& PackageFolder, const FString& AssetName, const FLNPFixtureMesh& Mesh,
		TConstArrayView<TPair<FName, FTransform>> Sockets)
	{
		const FString PackageName = FString::Printf(TEXT("%s/%s"), *PackageFolder, *AssetName);
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
		StaticMesh->Sockets.Reset();
		for (const TPair<FName, FTransform>& Entry : Sockets)
		{
			UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(StaticMesh);
			Socket->SocketName = Entry.Key;
			Socket->RelativeLocation = Entry.Value.GetLocation();
			Socket->RelativeRotation = Entry.Value.Rotator();
			StaticMesh->AddSocket(Socket);
		}

		StaticMesh->Build(false);
		StaticMesh->PostEditChange();

		if (!SavePackage(*Package, *StaticMesh, FPackageName::GetAssetPackageExtension()))
		{
			UE_LOG(LogLNPFixtureMesh, Error, TEXT("[FixtureMesh] Failed to save %s"), *PackageName);
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

	UWorld* CreateLevelWorld(const FString& PackageName)
	{
		UPackage* LevelPackage = CreatePackage(*PackageName);
		UWorld::InitializationValues Init;
		Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		UWorld* World = UWorld::CreateWorld(EWorldType::Inactive, false,
			FName(FPackageName::GetShortName(PackageName)), LevelPackage, true, ERHIFeatureLevel::Num, &Init);
		World->SetFlags(RF_Public | RF_Standalone);
		return World;
	}

	bool SaveWorld(UWorld& World)
	{
		FAssetRegistryModule::AssetCreated(&World);
		const bool bSaved = SavePackage(*World.GetOutermost(), World, FPackageName::GetMapPackageExtension());
		World.DestroyWorld(false);
		World.RemoveFromRoot();
		if (!bSaved)
		{
			UE_LOG(LogLNPFixtureMesh, Error, TEXT("[FixtureMesh] Failed to save %s"), *World.GetOutermost()->GetName());
		}
		return bSaved;
	}
}
