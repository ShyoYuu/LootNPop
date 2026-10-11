// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Find.h"
#include "DataAsset/LNPMassSpawnConfig.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "DynamicTerrain/LNPMovingPanel.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshCompiler.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "MassEntityConfigAsset.h"
#include "SurfaceNavigation/LNPMassSpawnPlan.h"
#include "SurfaceNavigation/LNPNavPathfinding.h"
#include "SurfaceNavigation/LNPNavQuery.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPHollowContentTest,
	"LootNPop.SurfaceNavigation.Bake.HollowContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPHollowContentTest::RunTest(const FString& Parameters)
{
	ULNPOctantSurfaceData* Data = LoadObject<ULNPOctantSurfaceData>(nullptr,
		TEXT("/Game/Maps/Hollow_00/DA_OctantSurface_Hollow_00.DA_OctantSurface_Hollow_00"));
	ULNPMassSpawnConfig* Config = LoadObject<ULNPMassSpawnConfig>(nullptr,
		TEXT("/Game/SpawnData/DA_MassSpawnConfig.DA_MassSpawnConfig"));
	if (!TestNotNull(TEXT("Hollow bake loads"), Data) || !TestNotNull(TEXT("Spawn config loads"), Config)) return false;

	// 같은 콘텐츠를 모든 slot 회전에서 검사한다. 에셋이나 실행 중 월드는 수정하지 않는다.
	FLNPOctantDefinition Definition;
	Definition.LevelAsset = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Hollow_00/LVI_Octant_Hollow_00.LVI_Octant_Hollow_00")));
	Definition.SurfaceData = Data;
	Definition.SeamSignature = TEXT("HollowContentTest");
	TArray<FLNPOctantDefinition> Definitions;
	Definitions.Init(Definition, 8);
	TArray<ULNPOctantSurfaceData*> Loaded;
	Loaded.Init(Data, 8);
	FLNPSurfaceDataSnapshot Snapshot;
	FString Error;
	if (!LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, Loaded, 1, Snapshot, Error))
	{
		AddError(Error);
		return false;
	}

	TArray<FLNPMassSpawnSetPlanInput> Sets;
	int32 AssetIndex = 0;
	for (const auto& Entry : Config->LootPodSpawnSets)
	{
		auto& Set = Sets.AddDefaulted_GetRef();
		Set.SpawnSetId = Entry.SpawnSetId;
		Set.RequestedPods = Entry.PodSetCount;
		Set.PodAssetIndex = AssetIndex++;
		for (const auto& Enemy : Entry.AssociatedEnemies)
		{
			const ULNPEnemyTrait* Trait = nullptr;
			for (const UMassEntityConfigAsset* Asset = Enemy.EnemyEntityConfig; Asset && !Trait; Asset = Asset->GetConfig().GetParent())
			{
				Trait = Cast<ULNPEnemyTrait>(Asset->FindTrait(ULNPEnemyTrait::StaticClass()));
			}
			const float Required = Trait && Trait->EnemyConfig ? Trait->EnemyConfig->GetSpawnRequiredHeadroom() : 0.0f;
			Set.Enemies.Add({Enemy.Count, AssetIndex++, Required, FName(*Enemy.EnemyEntityConfig->GetPathName())});
			Set.RequiredHeadroom = FMath::Max(Set.RequiredHeadroom, Required);
		}
	}

	const auto& Anchors = Snapshot.Slots[0].Spawn->AuthoredAnchors;
	if (!TestEqual(TEXT("Hollow retains eight authored Pods"), Anchors.Num(), 8)) return false;
	for (const auto& Anchor : Anchors)
	{
		TestEqual(TEXT("Each Pod retains two authored enemy types"), Anchor.EnemyFormations.Num(), 2);
		for (const auto& Formation : Anchor.EnemyFormations)
		{
			const FString Path = Formation.EnemyConfigPath.ToString();
			const int32 Expected = Path.Contains(TEXT("Flyer01")) ? 4 : Path.Contains(TEXT("Crawler01")) ? 6 : 0;
			TestTrue(TEXT("Hollow composition uses Flyer or Crawler"), Expected > 0);
			TestEqual(TEXT("Hollow retains four Flyers and six Crawlers"), Formation.Count, Expected);
		}
	}
	FLNPNavSearchScratch Scratch;
	// 실패한 뒷길 접합을 위치별로 식별할 수 있게 실제 Nav 투영 결과를 남긴다.
	const FVector SecretPoints[] = {{-4500.0, 4500.0, -12000.0}, {-6400.0, 4500.0, -12000.0},
		{-13500.0, 3500.0, -9500.0}, {-13500.0, -8500.0, -4500.0}, {-3500.0, -8500.0, -2000.0},
		{-3500.0, -11500.0, -2000.0}, {-12500.0, -11500.0, 0.0}, {-12500.0, -12500.0, 0.0}};
	const FVector Radial = FVector(1.0, 1.0, 1.0).GetSafeNormal();
	const FVector Bitangent(-1.0 / FMath::Sqrt(6.0), -1.0 / FMath::Sqrt(6.0), FMath::Sqrt(2.0 / 3.0));
	const FVector Tangent(-1.0 / FMath::Sqrt(2.0), 1.0 / FMath::Sqrt(2.0), 0.0);
	const auto WalkHeight = [](double T) { return T < 1.0/6.0 ? 3.6*T*T : T > 5.0/6.0 ? 1.0-3.6*FMath::Square(1.0-T) : 1.2*(T-1.0/12.0); };
	// 중심선 검사는 바깥 코너의 추락 틈을 놓쳤다. 두 회전 공간의 대각선까지 보행 바닥을 검사한다.
	for (const FVector& Bend : {SecretPoints[2], SecretPoints[3]})
	{
		for (int32 X = -2; X <= 2; ++X)
		{
			for (int32 Y = -2; Y <= 2; ++Y)
			{
				const double Scale = 30000.0 / (30000.0 - Bend.Z);
				const FVector P = Bend + FVector(X * 200.0 * Scale, Y * 200.0 * Scale, 0.0);
				const FVector World = (Radial * 30000.0 + Bitangent * P.X + Tangent * P.Y).GetSafeNormal() * (30000.0 - P.Z);
				bool bFound = false;
				for (const auto& Layer : Snapshot.Slots[0].Navigation->Layers)
				{
					FLNPNavProjection Projection;
					if (LNPNavQuery::ProjectToNode(Snapshot, World, {0, Layer.LocalSupportLayerId, Snapshot.Generation}, 100.0, Projection))
						{ bFound = true; break; }
				}
				TestTrue(FString::Printf(TEXT("Secret corner %.0f probe %d,%d has floor within 100cm"), Bend.Z, X, Y), bFound);
			}
		}
	}
	// 외곽 다리와 층계참은 실제 보행으로 잔도에 진입할 수 있어야 한다.
	const FVector OuterPoints[] = {{-12500.0, -12500.0, 0.0}, {-9000.0, -13500.0, 0.0},
		{-13500.0, 13500.0, 0.0}, {-6000.0, 6000.0, 4500.0},
		{-9000.0, -13500.0, 0.0}, {-2500.0, -5400.0, 6000.0},
		{-13500.0, 13500.0, 0.0}, {4000.0, 11000.0, 2000.0}};
	TArray<int32> OuterNodes;
	for (const FVector& P : OuterPoints)
	{
		const FVector World = (Radial * 30000.0 + Bitangent * P.X + Tangent * P.Y).GetSafeNormal() * (30000.0 - P.Z);
		FLNPNavProjection Best;
		Best.Distance = MAX_dbl;
		for (const auto& Layer : Snapshot.Slots[0].Navigation->Layers)
		{
			FLNPNavProjection Projection;
			if (LNPNavQuery::ProjectToNode(Snapshot, World, {0, Layer.LocalSupportLayerId, Snapshot.Generation}, 300.0, Projection)
				&& Projection.Distance < Best.Distance) Best = Projection;
		}
		if (!TestTrue(TEXT("Outer route endpoint projects to Nav"), Best.Node.IsValid())) return false;
		OuterNodes.Add(LNPNavGraph::ToGraphNode(Snapshot.Nav, Best.Node));
		AddInfo(FString::Printf(TEXT("Outer endpoint=%d layer=%u distance=%.1f group=%u"), OuterNodes.Num() - 1,
			Best.Node.RuntimeNavLayerId, Best.Distance, LNPNavGraph::GetGroup(Snapshot.Nav, OuterNodes.Last())));
	}
	for (int32 Route = 0; Route < UE_ARRAY_COUNT(OuterPoints) / 2; ++Route)
	{
		for (bool Reverse : {false, true})
		{
			FLNPNavSearchParams Params;
			Params.MaxExpansions = Snapshot.Nav.Graph.GetNodeCount();
			FLNPNavSearch Search;
			LNPNavPathfinding::BeginSearch(Snapshot.Nav, OuterNodes[Route * 2 + (Reverse ? 1 : 0)],
				OuterNodes[Route * 2 + (Reverse ? 0 : 1)], Params, Scratch, Search);
			if (Search.Status == ELNPNavSearchStatus::Running)
				LNPNavPathfinding::StepSearch(Snapshot.Nav, Scratch, Search, Params.MaxExpansions);
			AddInfo(FString::Printf(TEXT("Outer route=%d reverse=%d status=%d expansions=%d"), Route, Reverse,
				static_cast<int32>(Search.Status), Search.Expansions));
			TestTrue(TEXT("Outer ridge, saddle and mountain summit routes connect in both directions"), Search.Status == ELNPNavSearchStatus::Found);
		}
	}
	for (int32 Segment = 0; Segment < UE_ARRAY_COUNT(SecretPoints) - 1; ++Segment)
	{
		for (int32 Step = 0; Step <= 8; ++Step)
		{
			const double T = Step / 8.0;
			FVector P = FMath::Lerp(SecretPoints[Segment], SecretPoints[Segment + 1], T);
			P.Z = FMath::Lerp(SecretPoints[Segment].Z, SecretPoints[Segment + 1].Z, WalkHeight(T));
			const FVector World = (Radial * 30000.0 + Bitangent * P.X + Tangent * P.Y).GetSafeNormal() * (30000.0 - P.Z);
			FLNPNavProjection Best;
			Best.Distance = MAX_dbl;
			uint16 LayerId = MAX_uint16;
			for (const auto& Layer : Snapshot.Slots[0].Navigation->Layers)
			{
				FLNPNavProjection Projection;
				if (LNPNavQuery::ProjectToNode(Snapshot, World, {0, Layer.LocalSupportLayerId, Snapshot.Generation}, 300.0, Projection)
					&& Projection.Distance < Best.Distance) { Best = Projection; LayerId = Layer.LocalSupportLayerId; }
			}
			AddInfo(FString::Printf(TEXT("Secret segment=%d t=%.3f layer=%u distance=%.1f group=%u"), Segment, Step / 8.0,
				LayerId, Best.Distance, Best.Node.IsValid() ? LNPNavGraph::GetGroup(Snapshot.Nav, LNPNavGraph::ToGraphNode(Snapshot.Nav, Best.Node)) : MAX_uint32));
			TestTrue(TEXT("Every secret route sample projects within 100cm"), Best.Node.IsValid() && Best.Distance <= 100.0);
		}
	}
	// 저장된 조립을 격리 월드에 복사한다. 실제 Chaos 충돌을 검사하고 에디터/PIE 월드는 수정하지 않는다.
	UWorld* SourceWorld = LoadObject<UWorld>(nullptr, TEXT("/Game/Maps/Hollow_00/LVI_Octant_Hollow_00.LVI_Octant_Hollow_00"));
	if (!TestNotNull(TEXT("Hollow production LVI loads"), SourceWorld)) return false;
	UWorld* CollisionWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HollowCollisionTest"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(CollisionWorld);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(CollisionWorld); CollisionWorld->DestroyWorld(false); };
	TSet<UStaticMesh*> UsedMeshes;
	int32 AuthoredMeshes = 0;
	for (AActor* Actor : SourceWorld->PersistentLevel->Actors)
	{
		AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
		if (!MeshActor) continue;
		UStaticMeshComponent* Original = MeshActor->GetStaticMeshComponent();
		if (!Original->ComponentHasTag(TEXT("LNP.Surface.Static"))) continue;
		UStaticMesh* Mesh = Original->GetStaticMesh();
		if (!TestNotNull(TEXT("Terrain has mesh"), Mesh)) return false;
		if (Mesh->GetPathName().StartsWith(TEXT("/Game/Maps/Hollow_00/")))
			TestFalse(TEXT("Terrain mesh has one scene owner"), UsedMeshes.Contains(Mesh));
		UsedMeshes.Add(Mesh);
		if (Mesh->GetPathName().Contains(TEXT("/Authored/")))
		{
			++AuthoredMeshes;
			TestTrue(TEXT("Only approved whole-terrain version is placed"), Mesh->GetName().StartsWith(TEXT("SM_Hollow_00_LowPolyV017_")));
		}
		if (Mesh->GetPathName().StartsWith(TEXT("/Game/Maps/Hollow_00/")))
		{
			TestTrue(TEXT("Terrain uses LOD0 complex collision without a substitute"), Mesh->LODForCollision == 0 && Mesh->ComplexCollisionMesh == nullptr);
			TestTrue(TEXT("Terrain collision is single-sided complex-as-simple"), Mesh->GetBodySetup()
				&& Mesh->GetBodySetup()->CollisionTraceFlag == CTF_UseComplexAsSimple && !Mesh->GetBodySetup()->bDoubleSidedGeometry);
		}
		FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
		AActor* Owner = CollisionWorld->SpawnActor<AActor>();
		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(Owner);
		Copy->SetStaticMesh(Mesh);
		Copy->SetMobility(EComponentMobility::Static);
		Copy->SetCollisionProfileName(Original->GetCollisionProfileName());
		Copy->SetWorldTransform(Original->GetComponentTransform());
		Owner->SetRootComponent(Copy);
		Copy->RegisterComponent();
	}
	TestEqual(TEXT("Exactly eight whole-terrain role meshes are placed"), AuthoredMeshes, 8);
	auto Point = [&](FVector P) { return (Radial*30000.0+Bitangent*P.X+Tangent*P.Y).GetSafeNormal()*(30000.0-P.Z); };
	FCollisionQueryParams CollisionParams(SCENE_QUERY_STAT(HollowContent), false);
	const TArray<TArray<FVector>> Routes = {
		{SecretPoints[0], SecretPoints[1], SecretPoints[2], SecretPoints[3], SecretPoints[4], SecretPoints[5], SecretPoints[6], SecretPoints[7]},
		{{-9000.0,-13500.0,0.0}, {5000.0,-13500.0,2000.0}, {5000.0,-11000.0,2000.0},
			{-9000.0,-11000.0,4000.0}, {-9000.0,-8500.0,4000.0}, {4000.0,-8500.0,6000.0},
			{4000.0,-6000.0,6000.0}, {2000.0,-6000.0,6000.0}},
		{{-13500.0,13500.0,0.0}, {4000.0,11000.0,2000.0}, {4000.0,8500.0,2000.0}, {-6000.0,8500.0,4500.0}, {-6000.0,6000.0,4500.0}}
	};
	int32 FloorProbes = 0, CapsuleSweeps = 0;
	for (int32 Route = 0; Route < Routes.Num(); ++Route)
	{
		TArray<FVector2D> Sides, Offsets;
		for (int32 I = 1; I < Routes[Route].Num(); ++I)
		{
			const FVector2D Along = FVector2D(Routes[Route][I]-Routes[Route][I-1]).GetSafeNormal();
			Sides.Add(FVector2D(-Along.Y,Along.X));
		}
		Offsets.Add(Sides[0]);
		for (int32 I = 1; I < Sides.Num(); ++I)
		{
			const FVector2D Miter = (Sides[I-1]+Sides[I]).GetSafeNormal();
			Offsets.Add(Miter/FVector2D::DotProduct(Miter,Sides[I-1]));
		}
		Offsets.Add(Sides.Last());
	for (int32 Segment = 1; Segment < Routes[Route].Num(); ++Segment)
	{
		const FVector A = Routes[Route][Segment-1], B = Routes[Route][Segment];
		const int32 Steps = FMath::CeilToInt(FVector2D::Distance(FVector2D(A), FVector2D(B))/100.0);
		for (double Lateral : {-700.0, -350.0, 0.0, 350.0, 700.0})
		{
			FVector Previous = FVector::ZeroVector;
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const double T = double(Step)/Steps;
				FVector P = FMath::Lerp(A,B,T) + FVector(FMath::Lerp(Offsets[Segment-1],Offsets[Segment],T)*Lateral,0.0);
				P.Z = FMath::Lerp(A.Z,B.Z,WalkHeight(T));
				const FVector Feet = Point(P), Up = -Feet.GetSafeNormal();
				FHitResult Floor;
				++FloorProbes;
				if (!CollisionWorld->LineTraceSingleByChannel(Floor, Feet+Up*100.0, Feet-Up*100.0, ECC_Pawn, CollisionParams))
				{
					AddError(FString::Printf(TEXT("Missing route floor route=%d segment=%d step=%d side=%.0f"), Route, Segment, Step, Lateral));
					break;
				}
				const FVector Center = Floor.ImpactPoint+Up*110.0;
				if (Step > 0)
				for (bool Reverse : {false,true})
				{
					FHitResult Hit;
					++CapsuleSweeps;
					if (CollisionWorld->SweepSingleByChannel(Hit, Reverse ? Center : Previous, Reverse ? Previous : Center,
						FQuat::FindBetweenNormals(FVector::UpVector, Up), ECC_Pawn, FCollisionShape::MakeCapsule(42.0f,96.0f), CollisionParams))
						AddError(FString::Printf(TEXT("Blocked route capsule route=%d segment=%d step=%d side=%.0f reverse=%d mesh=%s"),
							Route, Segment, Step, Lateral, Reverse, *GetNameSafe(Hit.GetComponent()->GetOwner())));
				}
				Previous = Center;
			}
		}
	}
	}
	AddInfo(FString::Printf(TEXT("Hollow Chaos route floor probes=%d bidirectional five-track capsule sweeps=%d"), FloorProbes, CapsuleSweeps));
	// 내부 소유 경계 전체를 가로질러 지각과 Blender 외피 사이의 발밑·캡슐 접합을 검사한다.
	const FVector2D Boundary[] = {{-15500.0,-15500.0},{6000.0,-15500.0},{15500.0,-8500.0},
		{15500.0,8500.0},{6000.0,15500.0},{-15500.0,15500.0}};
	int32 JointProbes = 0;
	for (int32 Edge = 0; Edge < UE_ARRAY_COUNT(Boundary); ++Edge)
	{
		const FVector2D A = Boundary[Edge]*0.98, B = Boundary[(Edge+1)%UE_ARRAY_COUNT(Boundary)]*0.98;
		const FVector2D Along = (B-A).GetSafeNormal(), Inward(-Along.Y,Along.X);
		const int32 Steps = FMath::CeilToInt(FVector2D::Distance(A,B)/400.0);
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			FVector Previous = FVector::ZeroVector;
			for (int32 Cross = -4; Cross <= 4; ++Cross)
			{
				const FVector Feet = Point(FVector(FMath::Lerp(A,B,double(Step)/Steps)+Inward*Cross*50.0,0.0));
				const FVector Up = -Feet.GetSafeNormal();
				FHitResult Floor;
				++JointProbes;
				if (!CollisionWorld->LineTraceSingleByChannel(Floor,Feet+Up*100.0,Feet-Up*100.0,ECC_Pawn,CollisionParams))
				{
					AddError(FString::Printf(TEXT("Crust joint missing floor edge=%d step=%d cross=%d xy=%s"),
						Edge,Step,Cross,*(FMath::Lerp(A,B,double(Step)/Steps)+Inward*Cross*50.0).ToString()));
					break;
				}
				const FVector Center = Floor.ImpactPoint+Up*110.0;
				if (Cross > -4)
				for (bool Reverse : {false,true})
				{
					FHitResult Hit;
					if (CollisionWorld->SweepSingleByChannel(
						Hit,Reverse ? Center : Previous,Reverse ? Previous : Center,FQuat::FindBetweenNormals(FVector::UpVector,Up),
						ECC_Pawn,FCollisionShape::MakeCapsule(42.0f,96.0f),CollisionParams))
						AddError(FString::Printf(TEXT("Crust joint blocked edge=%d step=%d cross=%d reverse=%d mesh=%s xy=%s"),
							Edge,Step,Cross,Reverse,*GetNameSafe(Hit.GetComponent()),
							*(FMath::Lerp(A,B,double(Step)/Steps)+Inward*Cross*50.0).ToString()));
				}
				Previous = Center;
			}
		}
	}
	AddInfo(FString::Printf(TEXT("Hollow crust joint floor probes=%d"),JointProbes));
	// 수직 탈출 축과 상부 출구는 실제 캡슐을 끌어올릴 수 있는 빈 공간이어야 한다.
	const TPair<FVector,FVector> Shafts[] = {{FVector(0.0,0.0,-11890.0),FVector(0.0,0.0,-4500.0)},
		{FVector(2500.0,9800.0,-11890.0),FVector(2500.0,9800.0,2200.0)}};
	for (const auto& Shaft : Shafts)
	{
		const FVector A = Point(Shaft.Key), B = Point(Shaft.Value), Up = -((A+B)*0.5).GetSafeNormal();
		FHitResult Hit;
		if (CollisionWorld->SweepSingleByChannel(Hit,A,B,
			FQuat::FindBetweenNormals(FVector::UpVector,Up), ECC_Pawn, FCollisionShape::MakeCapsule(42.0f,96.0f), CollisionParams))
			AddError(FString::Printf(TEXT("Cavern escape shaft blocked start=%s mesh=%s hit=%s"),
				*Shaft.Key.ToString(),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString()));
	}
	// 대표 착지부에서 다음 훅의 지형 가림·거리, 패널 전 구간의 탑승 공간을 확인한다.
	struct FHookCheck { const TCHAR* Id; FVector Approach; FVector Landing; bool bCheckInertia = false; bool bCheckAllDirections = false; };
	// 수직 축 세 훅은 통로를 끌어올리는 역할이다. 그 외 모든 훅은 바로 아래 착지면을 가진다.
	const FHookCheck HookChecks[] = {
		{TEXT("WestLow"),{-2500,0,-7500},{-4500,-3000,-5000},true,true},
		{TEXT("WestPocketLip"),{-4700,-3000,-5000},{-3400,-2800,-2500},true,true},
		{TEXT("WestMid"),{-4200,-2800,-2500},{-2500,-2850,1500},true,true},
		// Mid 착지 후 약4m 오른쪽에서 당겨 High 잔도 밑면을 피한다.
		{TEXT("WestHigh"),{-2150,-2650,1500},{-3500,-2700,3500},true,true},
		{TEXT("WestTop"),{-3500,-2700,3500},{-5000,-4700,6000},true,true},
		{TEXT("WestPanel"),{-2500,-2850,1500},{2800,-2800,2000},true,true},
		{TEXT("EastLow"),{2500,0,-7500},{5500,2800,-5000},true,true},
		{TEXT("EastPanel"),{5500,2800,-5000},{3750,2500,2000},true,true},
		{TEXT("EastMid"),{3750,2500,2000},{0,2850,4500},true,true},
		{TEXT("EastHigh"),{0,2850,4500},{4050,2950,7500},true,true},
		{TEXT("EastTop"),{4050,2950,7500},{3250,5000,10000},true,true},
		{TEXT("BasinExit"),{0,0,-12000},FVector::ZeroVector},
		{TEXT("WestTopNorth"),{4662,-3884,6000},{4650,-4700,6000},true,true},
		{TEXT("EastTopSouth"),{-8206,4184,10000},{-8500,5000,10000},true,true},
		{TEXT("EastOuterRecovery"),{-4645,7295,0},{-5000,6800,4500},true},
		{TEXT("WestOuterRecovery"),{2750,-7429,0},{3500,-8250,5998},true},
		{TEXT("MountainWestRecovery"),{-7750,-9789,0},{-8500,-11000,3991},true},
		{TEXT("MountainEastRecovery"),{3800,-12205,0},{4500,-11000,2009},true},
		{TEXT("Ascent"),{-2500,-5400,6000},{-5500,-2000,14500}},
		{TEXT("Arena"),{-5500,0,14500},{3000,0,18000}},
		{TEXT("ShuttleIsland"),{-8700,-6600,6000},{-8000,-14000,6000}},
		{TEXT("BackPocketLip"),{6000,-5300,1500},{6000,-6700,1500}},
		{TEXT("NorthShaftLower"),{2500,9800,-12000},FVector::ZeroVector},
		{TEXT("NorthShaftUpper"),{3150,9800,-5500},FVector::ZeroVector},
		{TEXT("Rim"),{1800,9800,0},{4000,11000,2000}}
	};
	int32 HookApproaches = 0, HookLandings = 0, HookFalls = 0, PanelSweeps = 0, Panels = 0, MesaReviews = 0;
	TMap<FString, ALNPPlacementMarker*> Hooks;
	for (AActor* Actor : SourceWorld->PersistentLevel->Actors)
	{
		if (!Actor) continue;
		if (Actor->Tags.Contains(TEXT("HollowCaveMesaEntry")))
		{
			const FVector Feet = Actor->GetActorLocation(), Up = -Feet.GetSafeNormal();
			FHitResult Floor;
			TestTrue(TEXT("Cave Mesa review point has ground within one meter"), CollisionWorld->LineTraceSingleByChannel(
				Floor, Feet+Up*100.0, Feet-Up*100.0, ECC_Pawn, CollisionParams));
			TestFalse(TEXT("Cave Mesa teleport capsule starts clear of the ramp"), CollisionWorld->OverlapBlockingTestByChannel(
				Feet+Up*150.0, FQuat::FindBetweenNormals(FVector::UpVector,Up), ECC_Pawn,
				FCollisionShape::MakeCapsule(42.0f,96.0f), CollisionParams));
			++MesaReviews;
		}
		ALNPPlacementMarker* Marker = Cast<ALNPPlacementMarker>(Actor);
		if (!Marker) continue;
		if (Marker->GetActorLabel().StartsWith(TEXT("Anchor_")))
		{
			const FString Id = Marker->GetActorLabel().RightChop(7);
			const FHookCheck* Check = Algo::FindByPredicate(HookChecks,[&](const FHookCheck& Value) { return Id == Value.Id; });
			if (!TestNotNull(TEXT("Hook has a single declared purpose"),Check)) continue;
			TestFalse(TEXT("No duplicated hook role"),Hooks.Contains(Id));
			TestTrue(TEXT("Hollow uses authored hooks without extra seed hooks"),Marker->ActorHasTag(TEXT("LNP.Grapple.AuthoredOnly")));
			Hooks.Add(Id, Marker);
			const FVector Feet = Point(Check->Approach), Eye = Feet-Feet.GetSafeNormal()*180.0;
			FHitResult Hit;
			if (CollisionWorld->LineTraceSingleByChannel(Hit,Eye,Marker->GetActorLocation(),ECC_Visibility,CollisionParams))
				AddError(FString::Printf(TEXT("Hook approach blocked id=%s hit=%s"),*Id,*Hit.ImpactPoint.ToString()));
			TestTrue(FString::Printf(TEXT("Hook %s is within its marker reach"),*Id),
				FVector::Distance(Eye,Marker->GetActorLocation()) <= Marker->GrappleMaxDistance);
			++HookApproaches;
			if (!Check->Landing.IsNearlyZero())
			{
				const FVector Anchor = Marker->GetActorLocation(), Up = -Anchor.GetSafeNormal(), Landing = Point(Check->Landing);
				const double Fall = FVector::Distance(Anchor,Landing);
				FHitResult Floor;
				if (TestTrue(FString::Printf(TEXT("Hook %s ray away from world center hits its landing"),*Id),
					CollisionWorld->LineTraceSingleByChannel(Floor,Anchor,Anchor-Up*(Fall+100.0),ECC_Pawn,CollisionParams)))
				{
					TestTrue(FString::Printf(TEXT("Hook %s first floor is its intended ledge"),*Id),Floor.ImpactPoint.Equals(Landing,10.0));
					TestFalse(FString::Printf(TEXT("Hook %s capsule can fall to its ledge"),*Id),CollisionWorld->SweepSingleByChannel(
						Hit,Anchor,Floor.ImpactPoint+Up*110.0,FQuat::FindBetweenNormals(FVector::UpVector,Up),ECC_Pawn,
						FCollisionShape::MakeCapsule(42.0f,96.0f),CollisionParams));
					const double Margin = Check->bCheckAllDirections ? 400.0 : (Check->bCheckInertia ? 100.0 : 60.0);
					for (int32 Side = 0; Side < 16; ++Side)
					{
						const double Angle = Side*UE_TWO_PI/16.0;
						const FVector Offset = (Marker->GetActorForwardVector()*FMath::Cos(Angle)
							+Marker->GetActorRightVector()*FMath::Sin(Angle))*Margin;
						TestTrue(TEXT("Hook landing footprint has floor beyond capsule radius"),CollisionWorld->LineTraceSingleByChannel(
							Hit,Landing+Offset+Up*50.0,Landing+Offset-Up*50.0,ECC_Pawn,CollisionParams));
					}
				}
				if (Check->bCheckInertia)
				{
					const FVector PullStart = Feet-Feet.GetSafeNormal()*110.0;
					const FVector PullDirection = (Anchor-PullStart).GetSafeNormal();
					TestFalse(FString::Printf(TEXT("Hook %s approach fits the player capsule"),*Id),CollisionWorld->SweepSingleByChannel(
						Hit,PullStart,Anchor-PullDirection*150.0,FQuat::FindBetweenNormals(FVector::UpVector,Up),ECC_Pawn,
						FCollisionShape::MakeCapsule(42.0f,96.0f),CollisionParams));
					// BP의 종료 거리150/속도600, 중력2000, 무입력 감속200/종단2000을 이용한 배치 예측 검사다.
					// 실제 Mover·입력 보정·착지 후 보행은 마지막 사용자 PIE에서 확인한다.
					const auto VerifyFall = [&](const FVector& Direction, bool bRequireLandingMargin)
					{
						FVector Position = Anchor-Direction*150.0, Velocity = Direction*600.0;
						++HookFalls;
						for (int32 Step = 0; Step < 900; ++Step)
						{
							const FVector FallUp = -Position.GetSafeNormal();
							FVector Lateral = FVector::VectorPlaneProject(Velocity,FallUp);
							const double Speed = Lateral.Length();
							Lateral *= FMath::Max(0.0,1.0-200.0/60.0/FMath::Max(Speed,0.001));
							Velocity = Lateral+FallUp*FMath::Max(-2000.0,FVector::DotProduct(Velocity,FallUp)-2000.0/60.0);
							const FVector Next = Position+Velocity/60.0;
							FHitResult Arrival;
							if (CollisionWorld->SweepSingleByChannel(Arrival,Position,Next,
								FQuat::FindBetweenNormals(FVector::UpVector,FallUp),ECC_Pawn,
								FCollisionShape::MakeCapsule(42.0f,96.0f),CollisionParams))
							{
								const FVector FloorUp = -Arrival.ImpactPoint.GetSafeNormal();
								if (Arrival.bStartPenetrating || FVector::DotProduct(Arrival.ImpactNormal,FloorUp)<0.7
									|| FMath::Abs(30000.0-Arrival.ImpactPoint.Length()-Check->Landing.Z)>50.0)
									AddError(FString::Printf(TEXT("Hook inertia arrival missed id=%s direction=%s height=%.1f normal=%.2f penetrating=%d"),
										*Id,*Direction.ToString(),30000.0-Arrival.ImpactPoint.Length(),
										FVector::DotProduct(Arrival.ImpactNormal,FloorUp),Arrival.bStartPenetrating));
								if (bRequireLandingMargin)
								for (FVector Offset : {Marker->GetActorForwardVector()*100.0,-Marker->GetActorForwardVector()*100.0,
									Marker->GetActorRightVector()*100.0,-Marker->GetActorRightVector()*100.0})
									TestTrue(FString::Printf(TEXT("Hook %s inertia arrival has landing margin"),*Id),CollisionWorld->LineTraceSingleByChannel(
										Hit,Arrival.ImpactPoint+Offset+FloorUp*50.0,Arrival.ImpactPoint+Offset-FloorUp*50.0,ECC_Pawn,CollisionParams));
								return;
							}
							Position = Next;
						}
						AddError(FString::Printf(TEXT("Hook inertia arrival has no floor id=%s direction=%s"),*Id,*Direction.ToString()));
					};
					VerifyFall(PullDirection,true);
					for (int32 Side = 0; Side < 8; ++Side)
					{
						const double Angle = Side*UE_TWO_PI/8.0;
						const FVector Start = PullStart+(Marker->GetActorForwardVector()*FMath::Cos(Angle)
							+Marker->GetActorRightVector()*FMath::Sin(Angle))*200.0;
						if (!CollisionWorld->LineTraceSingleByChannel(Hit,Start,Anchor,ECC_Visibility,CollisionParams))
							VerifyFall((Anchor-Start).GetSafeNormal(),true);
					}
					if (Id == TEXT("EastOuterRecovery"))
					{
						const FVector Other = Point(FVector(-5052,7156,0)), OtherEye = Other-Other.GetSafeNormal()*180.0;
						TestFalse(TEXT("Shared east recovery reaches the second reported hollow"),CollisionWorld->LineTraceSingleByChannel(
							Hit,OtherEye,Anchor,ECC_Visibility,CollisionParams));
						TestTrue(TEXT("Second east recovery point is within reach"),FVector::Distance(OtherEye,Anchor)<=Marker->GrappleMaxDistance);
						const FVector OtherLaunch = Point(FVector(-4900,7400,0));
						const FVector OtherStart = OtherLaunch-OtherLaunch.GetSafeNormal()*110.0;
						const FVector OtherDirection = (Anchor-OtherStart).GetSafeNormal();
						TestFalse(TEXT("Second east recovery launch fits the player capsule"),CollisionWorld->SweepSingleByChannel(
							Hit,OtherStart,Anchor-OtherDirection*150.0,FQuat::FindBetweenNormals(FVector::UpVector,Up),ECC_Pawn,
							FCollisionShape::MakeCapsule(42.0f,96.0f),CollisionParams));
						VerifyFall(OtherDirection,true);
					}
					if (Check->bCheckAllDirections)
					{
						for (double Vertical : {0.0,0.5,0.9})
						for (int32 Side = 0; Side < 16; ++Side)
						{
							const double Angle = Side*UE_TWO_PI/16.0;
							const FVector Direction = Up*Vertical+(Marker->GetActorForwardVector()*FMath::Cos(Angle)
								+Marker->GetActorRightVector()*FMath::Sin(Angle))*FMath::Sqrt(1.0-Vertical*Vertical);
							if (!CollisionWorld->LineTraceSingleByChannel(Hit,Anchor-Direction*4000.0,Anchor,ECC_Visibility,CollisionParams)
								&& !CollisionWorld->SweepSingleByChannel(Hit,Anchor-Direction*4000.0,Anchor-Direction*150.0,
									FQuat::FindBetweenNormals(FVector::UpVector,Up),ECC_Pawn,FCollisionShape::MakeCapsule(42.0f,96.0f),CollisionParams))
								// 합성 공중 진입은 몸체 착지를 검사하고,100cm 여유는 실제 출발점과 주변에서 검사한다.
								VerifyFall(Direction,false);
						}
					}
				}
				++HookLandings;
			}
		}
		if (!Marker->ElementClass || !Marker->ElementClass->IsChildOf(ALNPMovingPanel::StaticClass())) continue;
		++Panels;
		const ALNPMovingPanel* Defaults = Marker->ElementClass->GetDefaultObject<ALNPMovingPanel>();
		const FVector Size = Defaults->GetPanelMesh()->GetStaticMesh()->GetBoundingBox().GetSize()
			* Defaults->GetPanelMesh()->GetRelativeScale3D();
		TestTrue(TEXT("Hollow panels provide a 1200 x 1200cm combat surface"), Size.Equals(FVector(1200.0,1200.0,30.0),0.1));
		FLNPPanelPath Path;
		Path.OriginLocation = Marker->GetActorLocation();
		Path.OriginRotation = Marker->GetActorQuat();
		Path.Curve = Marker->Path->GetSplinePointsPosition();
		if (Marker->GetActorLabel() == TEXT("Panel_00"))
			TestTrue(TEXT("Shuttle transport crosses the gap to the west cliff"), Marker->Path->GetSplineLength() >= 2500.0);
		for (int32 End : {0,1})
		{
			const FQuat Rotation = Path.EvaluateRotation(End);
			const FVector Up = Rotation.GetAxisZ();
			const FVector Origin = Marker->Path->GetLocationAtDistanceAlongSpline(
				Marker->Path->GetSplineLength()*End,ESplineCoordinateSpace::World)+Up*15.0;
			int32 DockSamples = 0;
			for (double Forward : {-540.0,-270.0,0.0,270.0,540.0})
			for (double Side : {-540.0,-270.0,0.0,270.0,540.0})
			{
				const FVector Top = Origin+Rotation.GetAxisX()*Forward+Rotation.GetAxisY()*Side;
				FHitResult Floor;
				if (CollisionWorld->LineTraceSingleByChannel(Floor,Top+Up*300.0,Top-Up*300.0,ECC_Pawn,CollisionParams)
					&& FMath::Abs(FVector::DotProduct(Top-Floor.ImpactPoint,Up)) <= 140.0) ++DockSamples;
			}
			TestTrue(FString::Printf(TEXT("Panel %s endpoint %d has a usable docking overlap within jump height"),
				*Marker->GetActorLabel(),End),DockSamples >= 2);
		}
		// 넓어진 판의 앞/뒤·좌/우 가장자리까지 포함해 아홉 탑승 트랙을 검사한다.
		for (double Forward : {-540.0,0.0,540.0})
		for (double Side : {-540.0,0.0,540.0})
		{
			FVector Previous = FVector::ZeroVector;
			const int32 Steps = FMath::CeilToInt(Marker->Path->GetSplineLength()/100.0);
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const FQuat Rotation = Path.EvaluateRotation(double(Step)/Steps);
				const FVector Up = Rotation.GetAxisZ();
				const FVector Center = Marker->Path->GetLocationAtDistanceAlongSpline(
					Marker->Path->GetSplineLength()*Step/Steps,ESplineCoordinateSpace::World)+Up*125.0
					+Rotation.GetAxisX()*Forward+Rotation.GetAxisY()*Side;
				if (Step > 0)
				for (bool Reverse : {false,true})
				{
					FHitResult Hit;
					if (CollisionWorld->SweepSingleByChannel(Hit,Reverse ? Center : Previous,Reverse ? Previous : Center,
						FQuat::FindBetweenNormals(FVector::UpVector,Up),ECC_Pawn,FCollisionShape::MakeCapsule(42.0f,96.0f),CollisionParams))
						AddError(FString::Printf(TEXT("Panel rider blocked panel=%s step=%d forward=%.0f side=%.0f reverse=%d"),
							*Marker->GetActorLabel(),Step,Forward,Side,Reverse));
					++PanelSweeps;
				}
				Previous = Center;
			}
		}
	}
	TestEqual(TEXT("Twenty-five hooks have distinct declared roles"),HookApproaches,25);
	TestEqual(TEXT("All twenty-two landing hooks have a real floor directly below"),HookLandings,22);
	TestEqual(TEXT("Both Hollow panels retain their transport paths"),Panels,2);
	TestEqual(TEXT("Cave Mesa review point is checked"),MesaReviews,1);
	// 새 Rim 훅은 축 출구와 경사로 바깥 양쪽에서 보여야 한다.
	if (ALNPPlacementMarker* const* Rim = Hooks.Find(TEXT("Rim")))
	for (FVector Local : {FVector(2500,9800,600),FVector(6000,11000,0),FVector(4000,13000,0)})
	{
		const FVector Feet = Point(Local), Eye = Feet-Feet.GetSafeNormal()*180.0;
		FHitResult Hit;
		TestFalse(TEXT("Rim recovery hook is visible from shaft exit and ramp exterior"), CollisionWorld->LineTraceSingleByChannel(
			Hit, Eye, (*Rim)->GetActorLocation(), ECC_Visibility, CollisionParams));
		TestTrue(TEXT("Rim recovery hook reaches shaft exit and ramp exterior"),
			FVector::Distance(Eye,(*Rim)->GetActorLocation()) <= (*Rim)->GrappleMaxDistance);
	}
	if (ALNPPlacementMarker* const* WestTop = Hooks.Find(TEXT("WestTop")))
	{
		const FVector Feet = Point(FVector(-9000,-14500,6000)), Eye = Feet-Feet.GetSafeNormal()*180.0;
		FHitResult Hit;
		TestFalse(TEXT("West cliff top hook also serves Shuttle return"),CollisionWorld->LineTraceSingleByChannel(
			Hit,Eye,(*WestTop)->GetActorLocation(),ECC_Visibility,CollisionParams));
		TestTrue(TEXT("Shuttle can reach shared west cliff top hook"),FVector::Distance(Eye,(*WestTop)->GetActorLocation()) <= (*WestTop)->GrappleMaxDistance);
	}
	AddInfo(FString::Printf(TEXT("Hollow hook approaches=%d landings=%d inertia falls=%d rotating panel rider capsule sweeps=%d"),HookApproaches,HookLandings,HookFalls,PanelSweeps));
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		TArray<int32> CaveNodes, GroundNodes;
		int32 ShuttleNode = INDEX_NONE;
		int32 WestNode = INDEX_NONE;
		for (const auto& Anchor : Anchors)
		{
			const FVector3d Local(Anchor.LocalTransform.GetLocation());
			FLNPNavProjection Projection;
			const FLNPSurfaceHandle Surface{static_cast<uint16>(Slot), Anchor.LocalLayerId, Snapshot.Generation};
			if (!TestTrue(TEXT("Authored Pod projects to its baked Nav layer"), LNPNavQuery::ProjectToNode(Snapshot,
				Snapshot.Slots[Slot].SlotRotation.RotateVector(Local), Surface, LNPNavQuery::DefaultProjectionRadius, Projection))) return false;
			const int32 Node = LNPNavGraph::ToGraphNode(Snapshot.Nav, Projection.Node);
			// 지각 아래 절벽 굴의 Pod도 있으므로 고도가 아니라 공동 Pod의 안정 ID로 분류한다.
			(Anchor.SpawnPointId.D == 3 || Anchor.SpawnPointId.D == 4 ? CaveNodes : GroundNodes).Add(Node);
			if (Anchor.SpawnPointId.D == 5) ShuttleNode = Node;
			if (Anchor.SpawnPointId.D == 1) WestNode = Node;
			TestTrue(TEXT("Pod retains 250cm edge clearance"), Anchor.EdgeClearance >= 250.0f);
			for (const auto& Formation : Anchor.EnemyFormations)
			{
				for (const auto& Set : Sets)
				{
					for (const auto& Enemy : Set.Enemies)
					{
						if (Enemy.ConfigPath == Formation.EnemyConfigPath)
							TestTrue(TEXT("Pod headroom meets its enemy config"), Anchor.Headroom >= Enemy.RequiredHeadroom);
					}
				}
			}
			if (Slot == 0) AddInfo(FString::Printf(TEXT("Hollow Pod=%u layer=%u edge=%.0f headroom=%.0f projection=%.1f group=%u"),
				Anchor.SpawnPointId.D, Anchor.LocalLayerId, Anchor.EdgeClearance, Anchor.Headroom, Projection.Distance,
				LNPNavGraph::GetGroup(Snapshot.Nav, Node)));
		}
		if (!TestEqual(TEXT("Two Pods remain in the main cavern"), CaveNodes.Num(), 2)) return false;
		if (!TestTrue(TEXT("Shuttle Pod projects to Nav"), ShuttleNode != INDEX_NONE)) return false;
		for (const int32 Ground : GroundNodes)
		{
			if (Ground == ShuttleNode) continue;
			for (const bool Reverse : {false, true})
			{
				FLNPNavSearch Search;
				LNPNavPathfinding::BeginSearch(Snapshot.Nav, Reverse ? Ground : ShuttleNode,
					Reverse ? ShuttleNode : Ground, {}, Scratch, Search);
				TestTrue(TEXT("Shuttle island has no static walking path to either cliff"),
					Search.Status == ELNPNavSearchStatus::Unreachable);
			}
		}
		FLNPNavSearch CaveSearch;
		LNPNavPathfinding::BeginSearch(Snapshot.Nav, CaveNodes[0], CaveNodes[1], {}, Scratch, CaveSearch);
		if (CaveSearch.Status == ELNPNavSearchStatus::Running)
			LNPNavPathfinding::StepSearch(Snapshot.Nav, Scratch, CaveSearch, 30000);
		TestTrue(TEXT("Cave branches have a real walking path"), CaveSearch.Status == ELNPNavSearchStatus::Found);
		if (!TestTrue(TEXT("West Pod projects to Nav"), WestNode != INDEX_NONE)) return false;
		// 새 뒷길은 실제 보행 연결이다. 수직 입구만 있었던 이전 NoPath 기대값을 대체한다.
		for (const int32 Cave : CaveNodes)
		{
			for (const bool Reverse : {false, true})
			{
				FLNPNavSearch Search;
				// 콘텐츠 연결 검사는 긴 외곽 우회도 끝까지 탐색한다. 런타임 요청의 기본 상한은 바꾸지 않는다.
				FLNPNavSearchParams Params;
				Params.MaxExpansions = Snapshot.Nav.Graph.GetNodeCount();
				LNPNavPathfinding::BeginSearch(Snapshot.Nav, Reverse ? WestNode : Cave, Reverse ? Cave : WestNode, Params, Scratch, Search);
				if (Search.Status == ELNPNavSearchStatus::Running)
					LNPNavPathfinding::StepSearch(Snapshot.Nav, Scratch, Search, Params.MaxExpansions);
				if (Slot == 0) AddInfo(FString::Printf(TEXT("Secret path reverse=%d status=%d expansions=%d"),
					Reverse, static_cast<int32>(Search.Status), Search.Expansions));
				TestTrue(TEXT("Secret ramp connects cave and west approach in both directions"), Search.Status == ELNPNavSearchStatus::Found);
			}
		}
	}

	// 실제 스폰 계획에서 후보 선점·거리·천장 제한을 통과한 개별 편성을 검사한다.
	for (int32 Seed = 0; Seed < 16; ++Seed)
	{
		FLNPMassSpawnPlan Plan;
		if (!LNPMassSpawnPlanning::BuildPlan(Snapshot, Sets, Seed, Config->MinDistanceBetweenPods,
			Config->EnemySpawnRadiusAroundPod, Plan, Error)) { AddError(Error); return false; }
		int32 Encounters = 0;
		for (const auto& Pod : Plan.Pods)
		{
			if (Pod.AuthoredEnemies.IsEmpty()) continue;
			++Encounters;
			for (const auto& Enemy : Pod.AuthoredEnemies)
			{
				const auto* Group = Pod.Enemies.FindByPredicate([&](const auto& Value) { return Value.AssetIndex == Enemy.AssetIndex; });
				TestEqual(FString::Printf(TEXT("Seed %d slot %u Pod %s retains %s"), Seed, Pod.Surface.OctantSlot,
					*Pod.Transform.GetLocation().ToString(), *Enemy.ConfigPath.ToString()), Group ? Group->Transforms.Num() : 0, Enemy.RequestedCount);
				if (!Group) continue;
				for (const FTransform& Transform : Group->Transforms)
				{
					TestTrue(TEXT("Spawn stays inside its authored approach region"), FVector::Distance(Transform.GetLocation(),
						Pod.Transform.TransformPosition(FVector(Enemy.Offset.X, Enemy.Offset.Y, 0.0))) <= Enemy.FormationRadius);
					TestTrue(TEXT("Spawn leaves four meters around Pod"), FVector::Distance(Transform.GetLocation(), Pod.Transform.GetLocation()) >= 400.0);
				}
			}
		}
		TestEqual(TEXT("All rotated authored Pods retain their encounters"), Encounters, 64);
	}
	return !HasAnyErrors();
}

#endif
