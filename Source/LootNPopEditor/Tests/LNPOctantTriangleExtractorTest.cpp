// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "MeshDescription.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshOperations.h"
#include "StaticMeshResources.h"
#include "SurfaceNavigation/LNPOctantSourceCollector.h"
#include "SurfaceNavigation/LNPOctantTriangleExtractor.h"
#include "UObject/StrongObjectPtr.h"

namespace LNPOctantTriangleExtractorTest
{
	constexpr TCHAR FixtureLevelPath[] =
		TEXT("/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Crust.LVI_Octant_Fixture_Crust");
	constexpr TCHAR MeadowLevelPath[] = TEXT("/Game/Maps/Meadow_00/LVI_Octant_Meadow_00.LVI_Octant_Meadow_00");
	constexpr TCHAR FixtureCrustMeshName[] = TEXT("SM_FixtureCrust_R30000");
	const FName SupportTag(TEXT("LNP.Surface.Support"));

	/** exact 위치 허용값(cm). 30,000cm 좌표의 float 정밀도(약 0.004cm)보다 충분히 크다. */
	constexpr double PositionTolerance = 0.1;
	constexpr double NormalDotTolerance = 0.9999;
	/** 삼각형 앞면에서 되돌아오는 trace 길이. 슬래브 반두께(10cm)보다 짧아야 반대 면을 맞히지 않는다. */
	constexpr double TraceHalfLength = 5.0;
	constexpr int32 MaxSamplesPerComponent = 256;

	/** 수집기와 같은 방식으로 external object까지 불러온 source World. */
	UWorld* LoadSourceWorld(const TCHAR* LevelPath)
	{
		IAssetRegistry& AssetRegistry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		AssetRegistry.WaitForCompletion();
		const FAssetData Asset = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(LevelPath));
		TSet<FName> LoadTags;
		LoadTags.Add(ULevel::LoadAllExternalObjectsTag);
		return Asset.IsValid() ? Cast<UWorld>(Asset.GetAsset(MoveTemp(LoadTags))) : nullptr;
	}

	TArray<UStaticMeshComponent*> GetSupportComponents(const UWorld& World)
	{
		TArray<UStaticMeshComponent*> Result;
		for (AActor* Actor : World.PersistentLevel->Actors)
		{
			if (!IsValid(Actor) || Actor->HasAnyFlags(RF_Transient))
			{
				continue;
			}
			TArray<UStaticMeshComponent*> Components;
			Actor->GetComponents(Components);
			for (UStaticMeshComponent* Component : Components)
			{
				if (Component->ComponentTags.Contains(SupportTag))
				{
					Result.Add(Component);
				}
			}
		}
		return Result;
	}

	double PointTriangleDistance(const FLNPBakeTriangleMesh& Mesh, int32 TriangleIndex, const FVector3d& Point)
	{
		const FIntVector3& Triangle = Mesh.Triangles[TriangleIndex];
		const FVector3d Closest = FMath::ClosestPointOnTriangleToPoint(
			Point, Mesh.Vertices[Triangle.X], Mesh.Vertices[Triangle.Y], Mesh.Vertices[Triangle.Z]);
		return FVector3d::Distance(Closest, Point);
	}

	FVector3d GetCentroid(const FLNPBakeTriangleMesh& Mesh, int32 TriangleIndex)
	{
		const FIntVector3& Triangle = Mesh.Triangles[TriangleIndex];
		return (Mesh.Vertices[Triangle.X] + Mesh.Vertices[Triangle.Y] + Mesh.Vertices[Triangle.Z]) / 3.0;
	}

	struct FPhysicsTestWorld
	{
		TStrongObjectPtr<UWorld> World;

		FPhysicsTestWorld()
		{
			World.Reset(NewObject<UWorld>(GetTransientPackage()));
			World->WorldType = EWorldType::EditorPreview;
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(World->WorldType);
			WorldContext.SetCurrentWorld(World.Get());
			World->InitializeNewWorld(UWorld::InitializationValues()
				.AllowAudioPlayback(false)
				.CreatePhysicsScene(true)
				.RequiresHitProxies(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false));
		}

		~FPhysicsTestWorld()
		{
			GEngine->DestroyWorldContext(World.Get());
			World->DestroyWorld(true);
		}
	};

	/** 노이즈 없는 구면 패치(삼각형 N²개). Nanite fallback이 삼각형을 줄여야 원본 판별이 가능하다. */
	FMeshDescription MakeSpherePatchDescription(double Radius, int32 N)
	{
		FMeshDescription Description;
		FStaticMeshAttributes Attributes(Description);
		Attributes.Register();
		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();

		TArray<FVertexID> Index;
		Index.SetNum((N + 1) * (N + 1));
		for (int32 J = 0; J <= N; ++J)
		{
			for (int32 I = 0; I + J <= N; ++I)
			{
				const FVertexID Vertex = Description.CreateVertex();
				Positions[Vertex] = FVector3f(FVector3d(I, J, N - I - J).GetSafeNormal() * Radius);
				Index[J * (N + 1) + I] = Vertex;
			}
		}
		auto AddTriangle = [&Description, Group](FVertexID A, FVertexID B, FVertexID C)
		{
			TArray<FVertexInstanceID, TFixedAllocator<3>> Instances = {
				Description.CreateVertexInstance(A),
				Description.CreateVertexInstance(B),
				Description.CreateVertexInstance(C)};
			Description.CreateTriangle(Group, Instances);
		};
		for (int32 J = 0; J < N; ++J)
		{
			for (int32 I = 0; I + J < N; ++I)
			{
				AddTriangle(Index[J * (N + 1) + I], Index[(J + 1) * (N + 1) + I], Index[J * (N + 1) + I + 1]);
				if (I + J < N - 1)
				{
					AddTriangle(Index[J * (N + 1) + I + 1], Index[(J + 1) * (N + 1) + I], Index[(J + 1) * (N + 1) + I + 1]);
				}
			}
		}
		FStaticMeshOperations::ComputeTriangleTangentsAndNormals(Description);
		FStaticMeshOperations::ComputeTangentsAndNormals(Description, EComputeNTBsFlags::Normals | EComputeNTBsFlags::Tangents);
		return Description;
	}

	int32 CountChaosTriangles(const UBodySetup& BodySetup)
	{
		int32 Count = 0;
		for (const Chaos::FTriangleMeshImplicitObjectPtr& TriMesh : BodySetup.TriMeshGeometries)
		{
			Count += TriMesh.IsValid() ? TriMesh->Elements().GetNumTriangles() : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPFixtureTriangleExtractionMatchesExactTest,
	"LootNPop.SurfaceNavigation.Bake.FixtureTriangleExtractionMatchesExact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPFixtureTriangleExtractionMatchesExactTest::RunTest(const FString& Parameters)
{
	using namespace LNPOctantTriangleExtractorTest;
	UWorld* SourceWorld = LoadSourceWorld(FixtureLevelPath);
	if (!TestNotNull(TEXT("Crust fixture LVI is loadable"), SourceWorld))
	{
		return false;
	}

	const TArray<UStaticMeshComponent*> Components = GetSupportComponents(*SourceWorld);
	TestEqual(TEXT("Fixture has crust, split sheet, double-sided plate and slab Support components"), Components.Num(), 4);

	FPhysicsTestWorld PhysicsWorld;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LNPTriangleExtractionTest), /*bTraceComplex=*/false);
	QueryParams.bReturnFaceIndex = true;
	for (const UStaticMeshComponent* Source : Components)
	{
		const FString MeshName = Source->GetStaticMesh()->GetName();
		const FTransform SourceTransform = FLNPOctantTriangleExtractor::GetSourceTransform(*Source);
		// 수집기 semantic hash는 GetComponentTransform()을 쓴다. 로드만 한 World에서도 같아야 hash가 transform을 반영한다.
		TestTrue(FString::Printf(TEXT("%s: loaded component transform equals its composed relative transform"), *MeshName),
			Source->GetComponentTransform().Equals(SourceTransform, 1e-4));

		FLNPBakeTriangleMesh Mesh;
		FString Error;
		if (!FLNPOctantTriangleExtractor::ExtractComponent(*Source, Mesh, Error))
		{
			AddError(Error);
			continue;
		}

		// winding: 저작 법선은 지각·캡·판이 구 중심 방향, 슬래브는 슬래브 중심의 바깥 방향이다.
		const bool bSlab = MeshName == TEXT("SM_FixtureSlab");
		const bool bDoubleSided = MeshName == TEXT("SM_FixtureDoubleSidedPlate");
		int32 AuthoredFacingCount = 0;
		for (int32 TriangleIndex = 0; TriangleIndex < Mesh.Triangles.Num(); ++TriangleIndex)
		{
			const FVector3d Centroid = GetCentroid(Mesh, TriangleIndex);
			const FVector3d Expected = bSlab ? Centroid - SourceTransform.GetLocation() : -Centroid;
			AuthoredFacingCount += FVector3d::DotProduct(Mesh.GetTriangleNormal(TriangleIndex), Expected) > 0.0 ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("%s: extracted winding matches the authored facing"), *MeshName),
			AuthoredFacingCount, bDoubleSided ? Mesh.Triangles.Num() / 2 : Mesh.Triangles.Num());

		// face→Layer 표(D-037)의 키: exact hit FaceIndex는 추출 순서가 아니라 external 번호다.
		TMap<int32, int32> InternalByExternal;
		bool bExternalUnique = Mesh.ExternalFaceIndices.Num() == Mesh.Triangles.Num();
		for (int32 Internal = 0; Internal < Mesh.ExternalFaceIndices.Num() && bExternalUnique; ++Internal)
		{
			bExternalUnique = !InternalByExternal.Contains(Mesh.ExternalFaceIndices[Internal]);
			InternalByExternal.Add(Mesh.ExternalFaceIndices[Internal], Internal);
		}
		TestTrue(FString::Printf(TEXT("%s: every triangle has a unique external face index"), *MeshName), bExternalUnique);
		int32 ReorderedCount = 0;
		for (int32 Internal = 0; Internal < Mesh.ExternalFaceIndices.Num(); ++Internal)
		{
			ReorderedCount += Mesh.ExternalFaceIndices[Internal] != Internal ? 1 : 0;
		}

		UStaticMeshComponent* Probe = NewObject<UStaticMeshComponent>(PhysicsWorld.World.Get());
		Probe->SetStaticMesh(Source->GetStaticMesh());
		Probe->SetWorldTransform(SourceTransform);
		Probe->SetCollisionProfileName(Source->GetCollisionProfileName());
		Probe->RegisterComponentWithWorld(PhysicsWorld.World.Get());

		const int32 Stride = FMath::Max(1, Mesh.Triangles.Num() / MaxSamplesPerComponent);
		int32 SampleCount = 0;
		int32 MissCount = 0;
		int32 PositionMismatchCount = 0;
		int32 NormalMismatchCount = 0;
		int32 FaceIndexMismatchCount = 0;
		double MaxPositionError = 0.0;
		for (int32 TriangleIndex = 0; TriangleIndex < Mesh.Triangles.Num(); TriangleIndex += Stride)
		{
			const FVector3d Normal = Mesh.GetTriangleNormal(TriangleIndex);
			if (Normal.IsNearlyZero())
			{
				continue;
			}
			++SampleCount;
			const FVector3d Centroid = GetCentroid(Mesh, TriangleIndex);
			FHitResult Hit;
			if (!Probe->LineTraceComponent(
				Hit, Centroid + Normal * TraceHalfLength, Centroid - Normal * TraceHalfLength, QueryParams))
			{
				++MissCount;
				continue;
			}

			const FVector3d HitPoint(Hit.ImpactPoint);
			const double PositionError = FVector3d::Distance(HitPoint, Centroid);
			MaxPositionError = FMath::Max(MaxPositionError, PositionError);
			PositionMismatchCount += PositionError > PositionTolerance ? 1 : 0;

			// 양면 판은 같은 자리에 반대 winding 삼각형이 겹친다. hit 지점을 포함하는 삼각형 중 하나와 맞으면 된다.
			bool bNormalMatches = false;
			for (int32 Other = 0; Other < Mesh.Triangles.Num() && !bNormalMatches; ++Other)
			{
				bNormalMatches = PointTriangleDistance(Mesh, Other, HitPoint) <= PositionTolerance
					&& FVector3d::DotProduct(Mesh.GetTriangleNormal(Other), FVector3d(Hit.ImpactNormal)) >= NormalDotTolerance;
			}
			NormalMismatchCount += bNormalMatches ? 0 : 1;

			// hit FaceIndex가 가리키는 추출 삼각형이 hit 지점을 담고 법선이 같아야 한다. 양면 판도 hit한 winding 하나로 정해진다.
			const int32* HitInternal = InternalByExternal.Find(Hit.FaceIndex);
			const bool bFaceMatches = HitInternal
				&& PointTriangleDistance(Mesh, *HitInternal, HitPoint) <= PositionTolerance
				&& FVector3d::DotProduct(Mesh.GetTriangleNormal(*HitInternal), FVector3d(Hit.ImpactNormal)) >= NormalDotTolerance;
			FaceIndexMismatchCount += bFaceMatches ? 0 : 1;
		}

		AddInfo(FString::Printf(
			TEXT("%s: triangles=%d samples=%d misses=%d positionMismatch=%d normalMismatch=%d faceIndexMismatch=%d ")
			TEXT("externalReordered=%d maxPositionError=%.4fcm det=%.2f"),
			*MeshName, Mesh.Triangles.Num(), SampleCount, MissCount, PositionMismatchCount, NormalMismatchCount,
			FaceIndexMismatchCount, ReorderedCount, MaxPositionError, SourceTransform.GetDeterminant()));
		TestTrue(FString::Printf(TEXT("%s: sampled triangles"), *MeshName), SampleCount > 0);
		TestEqual(FString::Printf(TEXT("%s: every front-face exact trace hits"), *MeshName), MissCount, 0);
		TestEqual(FString::Printf(TEXT("%s: exact hit lies on the extracted triangle"), *MeshName), PositionMismatchCount, 0);
		TestEqual(FString::Printf(TEXT("%s: exact hit normal equals the extracted normal"), *MeshName), NormalMismatchCount, 0);
		TestEqual(FString::Printf(TEXT("%s: exact hit FaceIndex names the extracted triangle"), *MeshName), FaceIndexMismatchCount, 0);

		// 뒷면 접근은 Atlas 광선(중심→바깥, 지각 앞면)과 반대 경우다. 동작을 기록만 한다.
		if (MeshName == FixtureCrustMeshName)
		{
			const FVector3d Normal = Mesh.GetTriangleNormal(0);
			const FVector3d Centroid = GetCentroid(Mesh, 0);
			FHitResult Hit;
			const bool bBackHit = Probe->LineTraceComponent(
				Hit, Centroid - Normal * TraceHalfLength, Centroid + Normal * TraceHalfLength, QueryParams);
			AddInfo(FString::Printf(TEXT("%s: back-face trace hit=%d normalDotFront=%.4f"),
				*MeshName, bBackHit ? 1 : 0, bBackHit ? FVector3d::DotProduct(FVector3d(Hit.ImpactNormal), Normal) : 0.0));
		}
		Probe->UnregisterComponent();
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPOctantCrustIdentificationTest,
	"LootNPop.SurfaceNavigation.Bake.OctantCrustIdentification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPOctantCrustIdentificationTest::RunTest(const FString& Parameters)
{
	using namespace LNPOctantTriangleExtractorTest;
	for (const TCHAR* LevelPath : {MeadowLevelPath, FixtureLevelPath})
	{
		FLNPOctantSourceCollection Collection;
		FString Error;
		if (!FLNPOctantSourceCollector::CollectFromLevel(FSoftObjectPath(LevelPath), 1, {}, Collection, Error))
		{
			AddError(FString::Printf(TEXT("%s: %s"), LevelPath, *Error));
			continue;
		}

		UWorld* SourceWorld = LoadSourceWorld(LevelPath);
		if (!TestNotNull(FString::Printf(TEXT("%s is loadable"), LevelPath), SourceWorld))
		{
			continue;
		}

		TArray<FLNPBakeSupportSource> Sources;
		if (!FLNPOctantTriangleExtractor::ExtractSupportSources(*SourceWorld, Sources, Error))
		{
			AddError(FString::Printf(TEXT("%s: %s"), LevelPath, *Error));
			continue;
		}

		double MaxAbsComponent = 0.0;
		for (const FLNPBakeSupportSource& Source : Sources)
		{
			if (!LNPSurfaceBake::ValidateSupportSource(Source, Error))
			{
				AddError(FString::Printf(TEXT("%s: %s"), LevelPath, *Error));
			}
			for (const FVector3d& Vertex : Source.Mesh.Vertices)
			{
				MaxAbsComponent = FMath::Max(MaxAbsComponent, Vertex.GetAbsMax());
			}
		}

		int32 CrustIndex = INDEX_NONE;
		if (!LNPSurfaceBake::IdentifyCrust(Sources, CrustIndex, Error))
		{
			AddError(FString::Printf(TEXT("%s: %s"), LevelPath, *Error));
			continue;
		}

		const FLNPBakeSupportSource& Crust = Sources[CrustIndex];
		const UStaticMeshComponent* CrustComponent = FindObject<UStaticMeshComponent>(nullptr, *Crust.Name);
		const FString CrustMeshName = CrustComponent ? CrustComponent->GetStaticMesh()->GetName() : TEXT("<missing>");
		AddInfo(FString::Printf(
			TEXT("%s: SupportSources=%d Crust=%s (%s) CrustTriangles=%d MaxAbsComponent=%.1fcm"),
			LevelPath, Sources.Num(), *Crust.Name, *CrustMeshName, Crust.Mesh.Triangles.Num(), MaxAbsComponent));
		if (LevelPath == FixtureLevelPath)
		{
			TestEqual(TEXT("Fixture crust is the perfect-sphere crust mesh"), CrustMeshName, FString(FixtureCrustMeshName));
		}
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPNaniteComplexCollisionSourceTest,
	"LootNPop.SurfaceNavigation.Bake.NaniteComplexCollisionSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPNaniteComplexCollisionSourceTest::RunTest(const FString& Parameters)
{
	using namespace LNPOctantTriangleExtractorTest;
	constexpr int32 Subdivisions = 48;
	constexpr int32 SourceTriangleCount = Subdivisions * Subdivisions;

	TStrongObjectPtr<UStaticMesh> StaticMesh(NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient));
	StaticMesh->AddSourceModel();
	FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(0);
	SourceModel.BuildSettings.bRecomputeNormals = false;
	SourceModel.BuildSettings.bRecomputeTangents = false;
	SourceModel.BuildSettings.bGenerateLightmapUVs = false;
	StaticMesh->GetStaticMaterials().Add(FStaticMaterial());
	StaticMesh->CreateMeshDescription(0, MakeSpherePatchDescription(3000.0, Subdivisions));
	StaticMesh->CommitMeshDescription(0);

	FMeshNaniteSettings NaniteSettings = StaticMesh->GetNaniteSettings();
	NaniteSettings.bEnabled = true;
	NaniteSettings.GenerateFallback = ENaniteGenerateFallback::Enabled;
	NaniteSettings.FallbackTarget = ENaniteFallbackTarget::PercentTriangles;
	NaniteSettings.FallbackPercentTriangles = 0.1f;
	StaticMesh->SetNaniteSettings(NaniteSettings);

	StaticMesh->CreateBodySetup();
	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
	StaticMesh->Build(/*bInSilent=*/true);
	FStaticMeshCompilingManager::Get().FinishCompilation({StaticMesh.Get()});
	BodySetup = StaticMesh->GetBodySetup();
	BodySetup->InvalidatePhysicsData();
	BodySetup->CreatePhysicsMeshes();

	const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
	if (!TestTrue(TEXT("Nanite mesh has render LOD0"), RenderData && RenderData->LODResources.Num() > 0))
	{
		return false;
	}
	const int32 FallbackTriangleCount = RenderData->LODResources[0].GetNumTriangles();
	const int32 CollisionTriangleCount = CountChaosTriangles(*BodySetup);
	const TCHAR* Origin = CollisionTriangleCount == FallbackTriangleCount ? TEXT("Nanite fallback (render LOD0)")
		: CollisionTriangleCount == SourceTriangleCount ? TEXT("source mesh")
		: TEXT("unknown");
	AddInfo(FString::Printf(
		TEXT("Nanite enabled=%d: source=%d fallback LOD0=%d Chaos trimesh=%d -> complex collision comes from %s"),
		StaticMesh->IsNaniteEnabled() ? 1 : 0, SourceTriangleCount, FallbackTriangleCount, CollisionTriangleCount, Origin));

	TestTrue(TEXT("Nanite is enabled on the probe mesh"), StaticMesh->IsNaniteEnabled());
	TestTrue(TEXT("Fallback reduced triangles, so the two origins are distinguishable"),
		FallbackTriangleCount < SourceTriangleCount);
	TestNotEqual(TEXT("Chaos trimesh matches one of the two origins"), FString(Origin), FString(TEXT("unknown")));
	return !HasAnyErrors();
}

#endif
