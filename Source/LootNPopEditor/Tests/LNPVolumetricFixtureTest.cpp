// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "DataAsset/LNPMassSpawnConfig.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "MassEntityConfigAsset.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "HitDetection/LNPProjectileMotion.h"
#include "Misc/ScopeExit.h"
#include "StaticMeshCompiler.h"
#include "SurfaceNavigation/LNPCollisionChannels.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "SurfaceNavigation/LNPMassSpawnPlan.h"
#include "SurfaceNavigation/LNPNavPathfinding.h"
#include "SurfaceNavigation/LNPOctantTriangleExtractor.h"
#include "SurfaceNavigation/LNPVolumetricFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPVolumetricFixtureTest,
	"LootNPop.SurfaceNavigation.Bake.VolumetricFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPVolumetricFixtureTest::RunTest(const FString& Parameters)
{
	using namespace LNPVolumetricFixture;
	const FSoftObjectPath DataPath(TEXT("/Game/Maps/SurfaceNavigation/Fixtures/DA_OctantSurface_Fixture_Regression.DA_OctantSurface_Fixture_Regression"));
	ULNPOctantSurfaceData* Data = TSoftObjectPtr<ULNPOctantSurfaceData>(DataPath).LoadSynchronous();
	UWorld* SourceWorld = LoadObject<UWorld>(nullptr, LNPRegressionFixture::LevelPath);
	if (!TestNotNull(TEXT("Saved volumetric data loads"), Data) || !TestNotNull(TEXT("Saved fixture loads"), SourceWorld)) { return false; }
	TArray<FLNPOctantDefinition> Definitions;
	Definitions.SetNum(8);
	for (auto& Definition : Definitions)
	{
		Definition.LevelAsset = TSoftObjectPtr<UWorld>(FSoftObjectPath(LNPRegressionFixture::LevelPath));
		Definition.SurfaceData = TSoftObjectPtr<ULNPOctantSurfaceData>(DataPath);
		Definition.SeamSignature = TEXT("RegressionSeam");
	}
	TArray<ULNPOctantSurfaceData*> Assets;
	Assets.Init(Data, 8);
	FLNPSurfaceDataSnapshot Snapshot;
	FString Error;
	if (!LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, Assets, 4, Snapshot, Error)) { AddError(Error); return false; }
	TMap<uint32, int32> GroupSizes;
	for (int32 N = 0; N < Snapshot.Nav.Graph.SlotNodeBase[1]; ++N)
	{
		++GroupSizes.FindOrAdd(LNPNavGraph::GetGroup(Snapshot.Nav, N));
	}
	for (const auto& Pair : GroupSizes)
	{
		int32 Representative = INDEX_NONE;
		for (int32 N = 0; N < Snapshot.Nav.Graph.SlotNodeBase[1]; ++N)
		{
			if (LNPNavGraph::GetGroup(Snapshot.Nav, N) == Pair.Key) { Representative = N; break; }
		}
		const auto& Node = Snapshot.Nav.Graph.GetNode(0, Representative);
		const auto& Atlas = *Snapshot.Slots[0].Support;
		AddInfo(FString::Printf(TEXT("Volumetric group=%u nodes/slot=%d layer=%u source=%s local=%s"), Pair.Key, Pair.Value, Node.LayerOrdinal,
			*Atlas.Sources[Atlas.Layers[Node.LayerOrdinal].SourceIndex].Key,
			*Transform().InverseTransformPosition(FVector(Node.LocalPoint)).ToString()));
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("VolumetricFixtureTest"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	TMap<const UPrimitiveComponent*, FString> SourceKeys;
	ULNPHitIdentitySubsystem* Registry = World->GetSubsystem<ULNPHitIdentitySubsystem>();
	ULNPMassWorldCollisionSubsystem* Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
	FLNPHitIdentitySnapshot FloorSnapshot;
	FloorSnapshot.SurfaceDataGeneration = Snapshot.Generation;
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		const FTransform SlotTransform(FQuat(Snapshot.Slots[Slot].SlotRotation));
		for (AActor* Source : SourceWorld->PersistentLevel->Actors)
		{
			AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Source);
			if (!MeshActor) { continue; }
			const UStaticMeshComponent* Original = MeshActor->GetStaticMeshComponent();
			FStaticMeshCompilingManager::Get().FinishCompilation({Original->GetStaticMesh()});
			AActor* Owner = World->SpawnActor<AActor>();
			UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(Owner);
			Copy->SetStaticMesh(Original->GetStaticMesh());
			Copy->SetMobility(EComponentMobility::Static);
			Copy->SetCollisionProfileName(Original->GetCollisionProfileName());
			Copy->SetWorldTransform(FLNPOctantTriangleExtractor::GetSourceTransform(*Original) * SlotTransform);
			Owner->SetRootComponent(Copy);
			Copy->RegisterComponent();
			SourceKeys.Add(Copy, Source->GetName() + TEXT(".") + Original->GetName());
			const auto& SlotAtlas = Snapshot.Slots[Slot].Support;
			const int32 FloorSource = SlotAtlas->Sources.IndexOfByPredicate([&](const auto& S) { return S.Key == SourceKeys.FindChecked(Copy); });
			if (FloorSource != INDEX_NONE)
			{
				Registry->RegisterRuntimeSource(Copy);
				FLNPExactSourceEntry Entry;
				Entry.Lifetime = ELNPExactSourceLifetime::Static; Entry.Roles = ELNPExactSourceRole::Support;
				Entry.Slot = Slot; Entry.Support = SlotAtlas; Entry.SupportSourceIndex = FloorSource;
				FloorSnapshot.Sources.Add(Copy, Entry);
			}
			if (MeshActor->GetActorLabel() == TEXT("RF_07_Ground") || MeshActor->GetActorLabel() == TEXT("RF_07_Structure"))
			{
				const FString& Key = SourceKeys.FindChecked(Copy);
				const auto& Atlas = *Snapshot.Slots[Slot].Support;
				const int32 SourceIndex = Atlas.Sources.IndexOfByPredicate([&](const auto& S) { return S.Key == Key; });
				TestTrue(TEXT("Volumetric source is saved in the face table"), SourceIndex != INDEX_NONE);
				int32 Layers = 0;
				for (const auto& Layer : Atlas.Layers)
				{
					if (Layer.SourceIndex != SourceIndex) { continue; }
					++Layers;
					TestEqual(TEXT("Coarse floor and fine structure retain source resolution"), Layer.Layout.Subdivisions,
						Atlas.Layers[0].Layout.Subdivisions * (MeshActor->GetActorLabel() == TEXT("RF_07_Ground") ? 1 : 4));
				}
			if (MeshActor->GetActorLabel() == TEXT("RF_07_Structure")) { TestTrue(TEXT("Folded structure has multiple sheets"), Layers > 1); }
			}
		}
	}
	Registry->Tick(0.0f);
	TArray<FVector> ShaftAnchors;
	for (const TCHAR* Label : {TEXT("RF_07_ShaftAnchor_0"), TEXT("RF_07_ShaftAnchor_1")})
	{
		for (AActor* Actor : SourceWorld->PersistentLevel->Actors)
		{
			if (Cast<ALNPPlacementMarker>(Actor) && Actor->GetActorLabel() == Label)
			{
				ShaftAnchors.Add(Actor->GetActorLocation());
				break;
			}
		}
	}
	if (!TestEqual(TEXT("Saved shaft has both grapple markers"), ShaftAnchors.Num(), 2)) { return false; }

	// 정점·공유 모서리의 수치적 ray miss를 피하고 실제 삼각형 내부를 표본으로 쓴다.
	const FVector Probes[] = {FloorPoint(0.0, 1800.0), FloorPoint(0.0, -3000.0), FloorPoint(1500.0, 0.0, UpperHeight),
		RampPoint(RampLength * 0.5, DoorY), RampPoint(RampLength * 0.5, -DoorY), FloorPoint(37.0, ShaftY + 51.0),
		(SpiralPoint(20, 550.0) + SpiralPoint(20, 650.0) + SpiralPoint(21, 650.0)) / 3.0,
		(SpiralPoint(88, 550.0) + SpiralPoint(88, 650.0) + SpiralPoint(89, 650.0)) / 3.0};
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VolumetricFixture), true);
	Params.bReturnFaceIndex = true;
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		const auto& Rotation = Snapshot.Slots[Slot].SlotRotation;
		const auto& Atlas = *Snapshot.Slots[Slot].Support;
		auto Point = [&](const FVector& P) { return Rotation.RotateVector(FVector3d(Transform().TransformPosition(P))); };
		// 공동 2P에서 지형을 빠져나간 Ghost의 실제 구간을 모든 slot에서 회귀 검사한다.
		const FQuat ReproRotation = ULNPOctantSpawnSubsystem::OctantRotations[4].Quaternion();
		const FVector EscapeStart = Rotation.RotateVector(ReproRotation.UnrotateVector(FVector(-17858.007, 19901.777, -19939.118)));
		const FVector EscapeEnd = Rotation.RotateVector(ReproRotation.UnrotateVector(FVector(-17000.360, 15953.050, -25550.740)));
		FHitResult EscapeHit;
		TestTrue(FString::Printf(TEXT("Slot %d cavern projectile escape reproduction hits exact terrain"), Slot),
			World->LineTraceSingleByChannel(EscapeHit, EscapeStart, EscapeEnd, LNPCollisionChannels::WorldExact, Params));
		const FVector WallStart = Rotation.RotateVector(ReproRotation.UnrotateVector(FVector(-20221.028, 19244.136, -18302.941)));
		const FVector WallEnd = Rotation.RotateVector(ReproRotation.UnrotateVector(FVector(-11846.657, 24083.636, -21816.514)));
		FCollisionQueryParams SimpleParams = Params;
		SimpleParams.bTraceComplex = false;
		TestTrue(FString::Printf(TEXT("Slot %d cavern wall projectile reproduction hits runtime exact terrain"), Slot),
			World->LineTraceSingleByChannel(EscapeHit, WallStart, WallEnd, LNPCollisionChannels::WorldExact, SimpleParams));
		// 실제 반복 이탈: 총구가 공동 벽 밖에서 시작하면 비행 선분만으로 벽을 잡을 수 없다.
		const FVector RawMuzzle = Rotation.RotateVector(ReproRotation.UnrotateVector(FVector(-17698.360, 20840.067, -19097.790)));
		const FVector MuzzleEnd = Rotation.RotateVector(ReproRotation.UnrotateVector(FVector(-14300.452, 24074.104, -20305.148)));
		const FVector Body = RawMuzzle - Rotation.RotateVector(FVector3d(Transform().TransformVector(FVector(0.0, 200.0, 0.0))));
		TestTrue(TEXT("Body to escaped muzzle crosses cavern wall"),
			World->LineTraceSingleByChannel(EscapeHit, Body, RawMuzzle, LNPCollisionChannels::WorldExact, SimpleParams));
		const FVector ProtectedMuzzle = LNPProjectileMotion::ClipMuzzleToWorld(World->GetSubsystem<ULNPMassWorldCollisionSubsystem>(), Body, RawMuzzle);
		TestFalse(TEXT("Unprotected muzzle projectile misses terrain"),
			World->LineTraceSingleByChannel(EscapeHit, RawMuzzle, MuzzleEnd, LNPCollisionChannels::WorldExact, SimpleParams));
		const FVector ClearMuzzle = Body - Rotation.RotateVector(FVector3d(Transform().TransformVector(FVector(0.0, 50.0, 0.0))));
		TestEqual(TEXT("Unblocked muzzle stays unchanged"),
			LNPProjectileMotion::ClipMuzzleToWorld(World->GetSubsystem<ULNPMassWorldCollisionSubsystem>(), Body, ClearMuzzle), ClearMuzzle);
		TestTrue(FString::Printf(TEXT("Slot %d protected muzzle projectile hits cavern wall"), Slot),
			World->LineTraceSingleByChannel(EscapeHit, ProtectedMuzzle, MuzzleEnd, LNPCollisionChannels::WorldExact, SimpleParams));
		// 실제 저장된 앵커의 150cm 전방 정지 지점에서 입구 외벽 밖까지 통과할 수 있어야 한다.
		const FVector3d LowerAnchor = Rotation.RotateVector(FVector3d(ShaftAnchors[0]));
		const FVector3d UpperAnchor = Rotation.RotateVector(FVector3d(ShaftAnchors[1]));
		const FVector3d LowerArrival = LowerAnchor + LowerAnchor.GetSafeNormal() * 150.0;
		const FVector3d UpperArrival = UpperAnchor - (UpperAnchor - LowerArrival).GetSafeNormal() * 150.0;
		TestTrue(TEXT("Upper shaft grapple remains within 25m reach"), FVector3d::Distance(LowerArrival, UpperAnchor) <= 2500.0);
		const FVector3d Outside = UpperArrival + Rotation.RotateVector(FVector3d(Frame().Bitangent)) * 800.0;
		for (const auto& Segment : {TPair<FVector3d, FVector3d>(LowerArrival, UpperArrival), {UpperArrival, Outside}})
		{
			const FVector3d Up = -((Segment.Key + Segment.Value) * 0.5).GetSafeNormal();
			FHitResult Hit;
			TestFalse(FString::Printf(TEXT("Slot %d shaft grapple and rim exit are unobstructed"), Slot),
				World->SweepSingleByChannel(Hit, Segment.Key, Segment.Value, FQuat::FindBetweenNormals(FVector::UpVector, Up),
					ECC_Pawn, FCollisionShape::MakeCapsule(42.0f, 96.0f), Params));
		}
		// 바닥 아래에서 올라오는 충돌과 나선 끝의 실제 캐릭터 통과 공간을 검사한다.
		const FVector3d Balcony = Point(FloorPoint(1500.0, 0.0, UpperHeight));
		const FVector3d BalconyUp = -Balcony.GetSafeNormal();
		FHitResult Underside;
		TestTrue(TEXT("Upper floor blocks Pawn approaching from below"), World->LineTraceSingleByChannel(
			Underside, Balcony - BalconyUp * 200.0, Balcony + BalconyUp * 100.0, ECC_Pawn, Params));
		TestTrue(TEXT("Upper floor underside faces downward"), FVector::DotProduct(Underside.ImpactNormal, BalconyUp) < -0.9);
		TArray<FVector> ExitPath;
		for (int32 S = SpiralSegments - 16; S <= SpiralSegments; ++S) { ExitPath.Add(SpiralPoint(S, 650.0)); }
		const FVector End = ExitPath.Last();
		const double EndAngle = UE_TWO_PI * SpiralTurns;
		const FVector Exit = FloorPoint(End.X + 500.0 * FMath::Cos(EndAngle), End.Y + 500.0 * FMath::Sin(EndAngle), UpperHeight);
		for (int32 Step = 1; Step <= 5; ++Step)
		{
			const FVector P = FMath::Lerp(End, Exit, Step / 5.0);
			ExitPath.Add(FloorPoint(P.X, P.Y, UpperHeight));
		}
		for (int32 Step = 1; Step <= 24; ++Step)
		{
			ExitPath.Add(FloorPoint(FMath::Lerp(Exit.X, 1500.0, Step / 24.0), Exit.Y, UpperHeight));
		}
		for (int32 I = 1; I < ExitPath.Num(); ++I)
		{
			const FVector3d A = Point(ExitPath[I - 1]), B = Point(ExitPath[I]);
			const FVector3d Up = -((A + B) * 0.5).GetSafeNormal();
			FHitResult Hit;
			TestFalse(FString::Printf(TEXT("Slot %d spiral exit capsule segment %d is unobstructed"), Slot, I),
				World->SweepSingleByChannel(Hit, A + Up * 105.0, B + Up * 105.0,
					FQuat::FindBetweenNormals(FVector::UpVector, Up), ECC_Pawn, FCollisionShape::MakeCapsule(35.0f, 90.0f), Params));
		}
		for (int32 I = 0; I < UE_ARRAY_COUNT(Probes); ++I)
		{
			const FVector3d Feet = Point(Probes[I]);
			const FVector3d Up = -Feet.GetSafeNormal();
			FHitResult Hit;
			if (!TestTrue(FString::Printf(TEXT("Slot %d probe %d exact floor exists"), Slot, I),
				World->LineTraceSingleByChannel(Hit, Feet + Up * 30.0, Feet - Up * 60.0, LNPCollisionChannels::SurfaceSupport, Params))) { continue; }
			const FString* Key = SourceKeys.Find(Hit.GetComponent());
			const int32 SourceIndex = Key ? Atlas.Sources.IndexOfByPredicate([&](const auto& S) { return S.Key == *Key; }) : INDEX_NONE;
			if (!TestTrue(TEXT("Exact floor resolves a saved source"), SourceIndex != INDEX_NONE)) { continue; }
			const uint16 LayerId = Atlas.Sources[SourceIndex].FaceMap.Resolve(Hit.FaceIndex);
			if (!TestTrue(TEXT("Exact face resolves a Support Layer"), Atlas.Layers.IsValidIndex(LayerId))) { continue; }
			// 실제 Mover처럼 face를 요청하지 않은 hit도 같은 접점의 층을 복구해야 한다.
			FHitResult MoverHit = Hit;
			MoverHit.FaceIndex = INDEX_NONE;
			const uint64 Before = Collision->GetQueryCount(ELNPWorldQueryClass::GroundRiskFallback);
			const auto FloorIdentity = Collision->ResolveFloorHit(FloorSnapshot, MoverHit);
			const bool bFolded = !Atlas.Sources[SourceIndex].FaceMap.IsUniform();
			TestEqual(TEXT("Only a folded floor without face performs an exact fallback"),
				Collision->GetQueryCount(ELNPWorldQueryClass::GroundRiskFallback) - Before, bFolded ? uint64(1) : uint64(0));
			TestEqual(TEXT("Mover floor without face resolves the exact Layer"), FloorIdentity.LocalLayerId, LayerId);
			TestEqual(TEXT("Mover floor preserves the slot"), FloorIdentity.Slot, static_cast<int8>(Slot));
			TestEqual(TEXT("Mover floor preserves the SurfaceData generation"), FloorIdentity.SurfaceDataGeneration, Snapshot.Generation);
			LNPEnemyExactMovement::FParams ActivationParams;
			ActivationParams.CapsuleRadius = 42.0f; ActivationParams.CapsuleHalfHeight = 96.0f;
			FLNPSurfaceHandle ActivationSurface;
			TestTrue(TEXT("Activation floor identity resolves"), LNPEnemySurfaceMovement::MakeSurfaceHandle(FloorIdentity, ActivationSurface));
			const FVector BuriedCenter = Hit.ImpactPoint + FVector(Up) * 44.5;
			FVector ActivationCenter;
			if (TestTrue(TEXT("Buried Actor activation projects to its current Layer"), LNPEnemySurfaceMovement::ProjectWanderTarget(
				&Snapshot, *Collision, ActivationParams, BuriedCenter, BuriedCenter.GetSafeNormal(),
				ActivationParams.CapsuleHalfHeight, ActivationSurface, ActivationCenter)))
			{
				TestTrue(TEXT("Actor activation restores the full capsule height"),
					FVector::DotProduct(ActivationCenter - Hit.ImpactPoint, FVector(Up)) > 94.0);
			}
			if (bFolded)
			{
				MoverHit.Item = 999;
				TestEqual(TEXT("Floor fallback rejects another instance"), Collision->ResolveFloorHit(FloorSnapshot, MoverHit).LocalLayerId, LNPSupportLayers::NoLayer);
				if (I == 2)
				{
					MoverHit.Item = Hit.Item;
					MoverHit.ImpactPoint = Point(FloorPoint(1500.0, 0.0));
					TestEqual(TEXT("Floor fallback rejects a different component below the balcony"),
						Collision->ResolveFloorHit(FloorSnapshot, MoverHit).LocalLayerId, LNPSupportLayers::NoLayer);
				}
			}
			const FVector3d Local = Rotation.UnrotateVector(FVector3d(Hit.ImpactPoint));
			FLNPSupportLayerHit Support;
			const auto Status = LNPSupportAtlas::QueryLayers(Atlas, Local.GetSafeNormal(), Local.Length() - 1.0, 45.0, 60.0, LayerId, Support);
			TestTrue(TEXT("Exact floor is represented or requires exact at a boundary"), Status != ELNPSupportQueryResult::NoSupport);
			if (Status == ELNPSupportQueryResult::Supported)
			{
				TestEqual(TEXT("Face identity and multi-layer query agree"), Support.Layer, LayerId);
				TestTrue(TEXT("Coarse/fine support stays within the crust radius error gate"), FMath::Abs(Support.Radius - Local.Length()) < 2.0);
			}
		}

		auto Nearest = [&](const FVector& P)
		{
			const FVector3d Target = Point(P);
			int32 Best = INDEX_NONE;
			double Distance = 250.0 * 250.0;
			for (int32 N = Snapshot.Nav.Graph.SlotNodeBase[Slot]; N < Snapshot.Nav.Graph.SlotNodeBase[Slot + 1]; ++N)
			{
				const double D = FVector3d::DistSquared(Target, Snapshot.Nav.Graph.GetWorldPoint(Slot, N));
				if (D < Distance) { Distance = D; Best = N; }
			}
			TestTrue(TEXT("Probe has a nearby walkable Nav node"), Best != INDEX_NONE);
			return Best;
		};
		auto Path = [&](const FVector& A, const FVector& B, bool bReachable, double EntranceSign = 0.0)
		{
			FLNPNavSearchScratch Scratch;
			FLNPNavSearch Search;
			LNPNavPathfinding::BeginSearch(Snapshot.Nav, Nearest(A), Nearest(B), {}, Scratch, Search);
			const double StartSeconds = FPlatformTime::Seconds();
			while (Search.Status == ELNPNavSearchStatus::Running) { LNPNavPathfinding::StepSearch(Snapshot.Nav, Scratch, Search, 4000); }
			AddInfo(FString::Printf(TEXT("Volumetric path slot=%d from=%s to=%s status=%d expansions=%d elapsedUs=%.1f cost=%.1f"),
				Slot, *A.ToString(), *B.ToString(), static_cast<int32>(Search.Status), Search.Expansions,
				(FPlatformTime::Seconds() - StartSeconds) * 1e6, Search.PathCost));
			TestTrue(FString::Printf(TEXT("Slot %d expected Walk reachability=%d"), Slot, bReachable), bReachable
				? Search.Status == ELNPNavSearchStatus::Found : Search.Status == ELNPNavSearchStatus::Unreachable);
			if (bReachable && EntranceSign != 0.0 && Search.Status == ELNPNavSearchStatus::Found)
			{
				TArray<int32> Nodes;
				LNPNavPathfinding::ExtractNodePath(Scratch, Search, Nodes);
				int32 RampNodes = 0;
				for (int32 N : Nodes)
				{
					const FVector P = Transform().InverseTransformPosition(FVector(Rotation.UnrotateVector(Snapshot.Nav.Graph.GetWorldPoint(Slot, N))));
					if (P.X > RoomHalf + 500.0 && P.X < RoomHalf + RampLength - 500.0)
					{
						++RampNodes;
						TestTrue(TEXT("A* selects the nearby entrance"), P.Y * EntranceSign > 700.0);
					}
				}
				TestTrue(TEXT("Path walks along the long entrance ramp"), RampNodes > 10);
			}
		};
		Path(RampPoint(RampLength - 1000.0, DoorY), FloorPoint(1700.0, DoorY), true, 1.0);
		Path(RampPoint(RampLength - 1000.0, -DoorY), FloorPoint(1700.0, -DoorY), true, -1.0);
		Path(FloorPoint(0.0, 1800.0), FloorPoint(1500.0, 0.0, UpperHeight), true);
		Path(SpiralPoint(12, 650.0), SpiralPoint(96, 650.0), true);
		Path(FloorPoint(0.0, 1800.0), FloorPoint(0.0, -3000.0), true);
		Path(FloorPoint(0.0, 1800.0), FloorPoint(0.0, ShaftY), false);
		const FVector Mouth = Transform().TransformPosition(FVector(0.0, ShaftY, 4000.0)).GetSafeNormal() * LNPRegressionFixture::CrustRadius;
		Path(Transform().InverseTransformPosition(Mouth + Frame().Tangent * 1200.0), FloorPoint(0.0, ShaftY), false);
		FLNPSupportLayerQuery Crust;
		TestFalse(TEXT("Vertical entrance has no ghost crust floor"), LNPSupportAtlas::QueryLayer(Atlas.Layers[0], Mouth.GetSafeNormal(), Crust));
	}

	// 공동의 특정 세트 anchor가 실제 적 후보를 제공해 PIE에서 편성을 확인할 수 있어야 한다.
	FLNPMassSpawnSetPlanInput CaveSets[2];
	for (int32 I = 0; I < 2; ++I)
	{
		CaveSets[I].SpawnSetId = I == 0 ? TEXT("Bench01") : TEXT("Bench01_2");
		CaveSets[I].RequestedPods = 1;
		CaveSets[I].PodAssetIndex = 0;
		CaveSets[I].RequiredHeadroom = I == 0 ? 0.0f : 2638.0f;
		FLNPMassSpawnEnemyPlanInput& Enemy = CaveSets[I].Enemies.AddDefaulted_GetRef();
		Enemy.AssetIndex = I + 1; Enemy.RequestedCount = 3; Enemy.RequiredHeadroom = CaveSets[I].RequiredHeadroom;
	}
	FLNPMassSpawnPlan CavePlan;
	if (LNPMassSpawnPlanning::BuildPlan(Snapshot, CaveSets, 123, 2000.0f, 800.0f, CavePlan, Error))
	{
		TestEqual(TEXT("Both cave sets get authored Pods"), CavePlan.Pods.Num(), 2);
		for (const auto& Pod : CavePlan.Pods)
		{
			TestEqual(TEXT("Cave set uses its own anchor"), Pod.PlacementSource, ELNPPodPlacementSource::AuthoredForSet);
			TestTrue(TEXT("Cave set has actual nearby enemy candidates"), Pod.Enemies.Num() == 1 && Pod.Enemies[0].Transforms.Num() == 3);
		}
	}
	else { AddError(Error); }
	ULNPMassSpawnConfig* Config = LoadObject<ULNPMassSpawnConfig>(nullptr,
		TEXT("/Game/SpawnData/DA_MassSpawnConfig.DA_MassSpawnConfig"));
	if (TestNotNull(TEXT("Production mixed spawn config loads"), Config))
	{
		TArray<FLNPMassSpawnSetPlanInput> Sets;
		int32 AssetIndex = 0, CaveFlyerIndex = INDEX_NONE, CaveFlyerRequestedCount = 0;
		for (const auto& Entry : Config->LootPodSpawnSets)
		{
			auto& Set = Sets.AddDefaulted_GetRef();
			Set.SpawnSetId = Entry.SpawnSetId; Set.RequestedPods = Entry.PodSetCount; Set.PodAssetIndex = AssetIndex++;
			for (const auto& Enemy : Entry.AssociatedEnemies)
			{
				const ULNPEnemyTrait* Trait = nullptr;
				for (const UMassEntityConfigAsset* Asset = Enemy.EnemyEntityConfig; Asset && !Trait; Asset = Asset->GetConfig().GetParent())
				{
					Trait = Cast<ULNPEnemyTrait>(Asset->FindTrait(ULNPEnemyTrait::StaticClass()));
				}
				const float Required = Trait && Trait->EnemyConfig && Enemy.Count > 0 ? Trait->EnemyConfig->GetSpawnRequiredHeadroom() : 0.0f;
				Set.Enemies.Add({Enemy.Count, AssetIndex, Required});
				Set.RequiredHeadroom = FMath::Max(Set.RequiredHeadroom, Required);
				if (Set.SpawnSetId == TEXT("Bench01_2") && Required > 0.0f) { CaveFlyerIndex = AssetIndex; CaveFlyerRequestedCount = Enemy.Count; }
				++AssetIndex;
			}
		}
		TestTrue(TEXT("Production cave set requests flying enemies"), CaveFlyerRequestedCount > 0);
		FLNPMassSpawnPlan Mixed;
		if (LNPMassSpawnPlanning::BuildPlan(Snapshot, Sets, 123, Config->MinDistanceBetweenPods,
			Config->EnemySpawnRadiusAroundPod, Mixed, Error))
		{
			int32 CaveFlyerPods = 0;
			for (const auto& Pod : Mixed.Pods)
			{
				if (Sets[Pod.SetInputIndex].SpawnSetId != TEXT("Bench01_2") || Pod.PlacementSource != ELNPPodPlacementSource::AuthoredForSet) { continue; }
				++CaveFlyerPods;
				int32 Flyers = 0;
				for (const auto& Enemy : Pod.Enemies) { if (Enemy.AssetIndex == CaveFlyerIndex) { Flyers += Enemy.Transforms.Num(); } }
				TestEqual(TEXT("Actual mixed cave set retains all configured flying enemies"), Flyers, CaveFlyerRequestedCount);
			}
			TestEqual(TEXT("Each slot gets its mixed flying cave Pod"), CaveFlyerPods, 8);
		}
		else { AddError(Error); }
	}

	// 저장된 실제 sweep 값을 지역별로 제한해 비행 편성의 런타임 필터까지 검사한다.
	for (const bool bTall : {false, true})
	{
		FLNPSurfaceDataSnapshot Filtered = Snapshot;
		int32 CandidateCount = 0;
		for (auto& Slot : Filtered.Slots)
		{
			auto Spawn = MakeShared<FLNPSpawnData, ESPMode::ThreadSafe>();
			for (const auto& Candidate : Slot.Spawn->RandomCandidates)
			{
				const FVector P = Transform().InverseTransformPosition(FVector(Candidate.LocalPosition));
				const bool bRegion = bTall ? P.X > -2050.0 && P.X < -1550.0 && FMath::Abs(P.Y) < 700.0
					: FMath::Abs(P.X) < 300.0 && P.Y > -3400.0 && P.Y < -2800.0;
				if (bRegion && P.Z < 250.0)
				{
					TestTrue(TEXT("Actual bake headroom distinguishes tall and low spaces"), bTall ? Candidate.Headroom >= 2638.0f : Candidate.Headroom < 2638.0f);
					Spawn->RandomCandidates.Add(Candidate);
					++CandidateCount;
				}
			}
			Slot.Spawn = Spawn;
		}
		TestTrue(TEXT("Region has real baked spawn candidates"), CandidateCount > 0);
		FLNPMassSpawnSetPlanInput Set;
		Set.SpawnSetId = TEXT("VolumetricFlyers"); Set.RequestedPods = 1; Set.PodAssetIndex = 0; Set.RequiredHeadroom = 2638.0f;
		FLNPMassSpawnPlan Plan;
		if (!LNPMassSpawnPlanning::BuildPlan(Filtered, MakeArrayView(&Set, 1), 123, 200.0f, 500.0f, Plan, Error)) { AddError(Error); continue; }
		TestEqual(TEXT("Flying Pod can only be assigned in the tall region"), Plan.Pods.Num(), bTall ? 1 : 0);
	}
	return !HasAnyErrors();
}

#endif
