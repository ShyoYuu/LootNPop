// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SplineComponent.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "DynamicTerrain/LNPMovingPanel.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "SurfaceNavigation/LNPConditionalPatch.h"
#include "SurfaceNavigation/LNPOctantSourceCollector.h"
#include "SurfaceNavigation/LNPOctantSurfaceBaker.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPConditionalPatchBakeTest,
	"LootNPop.SurfaceNavigation.ConditionalPatch.Bake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPConditionalPatchBakeTest::RunTest(const FString& Parameters)
{
	UWorld* World = LoadObject<UWorld>(nullptr, LNPRegressionFixture::LevelPath);
	UStaticMesh* Top = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Maps/SurfaceNavigation/Fixtures/SM_FixtureBoxTop.SM_FixtureBoxTop"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Source World loads"), World) || !TestNotNull(TEXT("Support mesh loads"), Top)
		|| !TestNotNull(TEXT("Blocker mesh loads"), Cube)) { return false; }
	const bool bWasDirty = World->GetOutermost()->IsDirty();
	// 저장하지 않는 source marker로 실제 cooked mesh·physics 베이크를 검증한다.
	ALNPPlacementMarker* Marker = NewObject<ALNPPlacementMarker>(World->PersistentLevel);
	World->PersistentLevel->Actors.Add(Marker);
	ON_SCOPE_EXIT
	{
		World->PersistentLevel->Actors.Remove(Marker);
		World->GetOutermost()->SetDirtyFlag(bWasDirty);
	};
	Marker->MarkerId = FGuid(8, 2, 1, 1);
	Marker->ElementClass = ALNPMovingPanel::StaticClass();
	const LNPRegressionFixture::FCaseFrame Frame = LNPRegressionFixture::BasicCrust();
	Marker->SetActorTransform(FTransform(Frame.UpRotation(), Frame.At(28150.0)));
	FLNPConditionalPatchSource& Support = Marker->PatchSources.AddDefaulted_GetRef();
	Support.Mesh = Top;
	Support.Transform.SetScale3D(FVector(10.0, 10.0, 3.0));
	FLNPConditionalPatchSource& Blocker = Marker->PatchSources.AddDefaulted_GetRef();
	Blocker.Mesh = Cube;
	Blocker.bSupport = false;
	Blocker.Transform = FTransform(FQuat::Identity, FVector(0.0, 0.0, -1850.0), FVector(8.0, 8.0, 4.0));
	FString Error;
	TArray<uint8> Original, Changed;
	if (!FLNPOctantSourceCollector::BuildPlacementMarkerSemantic(*Marker, Original, Error)) { AddError(Error); return false; }
	auto CheckChanged = [&](const TCHAR* Label)
	{
		TestTrue(Label, FLNPOctantSourceCollector::BuildPlacementMarkerSemantic(*Marker, Changed, Error) && Changed != Original);
	};
	Marker->PatchCondition = ELNPConditionalPatchCondition::Bridge;
	CheckChanged(TEXT("Condition changes freshness"));
	Marker->PatchCondition = ELNPConditionalPatchCondition::Intact;
	Marker->PatchSources[0].Transform.AddToTranslation(FVector(1.0, 0.0, 0.0));
	CheckChanged(TEXT("Stable pose changes freshness"));
	Marker->PatchSources[0].Transform.AddToTranslation(FVector(-1.0, 0.0, 0.0));
	Marker->PatchSources[0].bSupport = false;
	CheckChanged(TEXT("Support participation changes freshness"));
	Marker->PatchSources[0].bSupport = true;
	Marker->Path->SetLocationAtSplinePoint(0, FVector(1.0, 0.0, 0.0), ESplineCoordinateSpace::Local);
	CheckChanged(TEXT("Path changes freshness"));
	Marker->Path->SetLocationAtSplinePoint(0, FVector::ZeroVector, ESplineCoordinateSpace::Local);
	Marker->PathSpeed += 1.0f;
	CheckChanged(TEXT("Path speed changes freshness"));
	Marker->PathSpeed -= 1.0f;
	FLNPOctantBakeOptions Options;
	Options.CrustSpacing = 200.0;
	TStrongObjectPtr<ULNPOctantSurfaceData> Data(NewObject<ULNPOctantSurfaceData>());
	TStrongObjectPtr<ULNPOctantSurfaceData> Again(NewObject<ULNPOctantSurfaceData>());
	FLNPOctantBakeReport Report;
	const FSoftObjectPath Level(LNPRegressionFixture::LevelPath);
	if (!FLNPOctantSurfaceBaker::Bake(Level, Options, *Data, Report, Error)) { AddError(Error); return false; }
	FLNPSupportAtlas Base;
	FLNPNavData Nav;
	FLNPConditionalPatchData Patches;
	if (!LNPSupportAtlas::Decode(Data->SupportPayload, Base, Error)
		|| !LNPNavData::DecodeNavigation(Data->NavigationPayload, Nav, Error)
		|| !LNPConditionalPatch::Decode(Data->ConditionalPatchPayload, Base, Nav, Patches, Error)) { AddError(Error); return false; }
	TestEqual(TEXT("One conditional marker is baked"), Patches.Patches.Num(), 1);
	if (Patches.Patches.Num() != 1) { return false; }
	const FLNPConditionalPatch& Patch = Patches.Patches[0];
	TestTrue(TEXT("Stable floor has sparse Support and Nav"), !Patch.SupportLayers.IsEmpty() && !Patch.Navigation.Layers.IsEmpty());
	TestEqual(TEXT("Conditional IDs follow static Layers"), int32(Patch.FirstSupportLayer), Base.Layers.Num());
	TestTrue(TEXT("Exact blocker records base occupancy"), !Patch.BlockedNodes.IsEmpty());
	TestTrue(TEXT("Exact blocker records base edge closures"), !Patch.BlockedEdges.IsEmpty());
	TestTrue(TEXT("Isolated floor does not invent a Walk connection"), Patch.WalkEdges.IsEmpty());
	if (!FLNPOctantSurfaceBaker::Bake(Level, Options, *Again, Report, Error)) { AddError(Error); return false; }
	TestTrue(TEXT("Patch bake is byte deterministic"), Again->ConditionalPatchPayload == Data->ConditionalPatchPayload);
	Marker->PatchCondition = ELNPConditionalPatchCondition::Bridge;
	TestFalse(TEXT("Disconnected Bridge is rejected"), FLNPOctantSurfaceBaker::Bake(Level, Options, *Again, Report, Error));
	TestTrue(TEXT("Bridge error names missing Walk connection"), Error.Contains(TEXT("Walk connection")));
	Marker->PatchCondition = ELNPConditionalPatchCondition::Intact;
	Marker->PatchSources.SetNum(1);
	Marker->SetActorLocation(Frame.At(30100.0));
	TestFalse(TEXT("Interior static overlap is rejected"), FLNPOctantSurfaceBaker::Bake(Level, Options, *Again, Report, Error));
	TestTrue(TEXT("Overlap error identifies capsule height"), Error.Contains(TEXT("capsule height")));
	Marker->SetActorTransform(FTransform(FQuat::Identity, FVector(0.0, 20000.0, 20000.0)));
	TestFalse(TEXT("Seam crossing is rejected"), FLNPOctantSurfaceBaker::Bake(Level, Options, *Again, Report, Error));
	TestTrue(TEXT("Boundary error identifies octant seam"), Error.Contains(TEXT("octant seam")));
	const LNPRegressionFixture::FCaseFrame Island = LNPRegressionFixture::IslandOne();
	Marker->SetActorTransform(FTransform(Island.UpRotation(), Island.At(28150.0, 1150.0)));
	Marker->PatchCondition = ELNPConditionalPatchCondition::Bridge;
	if (!FLNPOctantSurfaceBaker::Bake(Level, Options, *Again, Report, Error)
		|| !LNPSupportAtlas::Decode(Again->SupportPayload, Base, Error)
		|| !LNPNavData::DecodeNavigation(Again->NavigationPayload, Nav, Error)
		|| !LNPConditionalPatch::Decode(Again->ConditionalPatchPayload, Base, Nav, Patches, Error)) { AddError(Error); return false; }
	TestTrue(TEXT("Boundary joins export exact Walk edges to static island"),
		Patches.Patches.Num() == 1 && !Patches.Patches[0].WalkEdges.IsEmpty());
	Marker->PatchCondition = ELNPConditionalPatchCondition::Intact;
	Marker->SetActorTransform(FTransform(Frame.UpRotation(), Frame.At(28150.0)));
	Marker->PatchSources.Reset();
	Marker->PatchSources.Add({Cube, FTransform(FQuat::Identity, FVector(0.0, 0.0, -1850.0), FVector(8.0, 8.0, 4.0)), false});
	if (!FLNPOctantSurfaceBaker::Bake(Level, Options, *Again, Report, Error)
		|| !LNPSupportAtlas::Decode(Again->SupportPayload, Base, Error)
		|| !LNPNavData::DecodeNavigation(Again->NavigationPayload, Nav, Error)
		|| !LNPConditionalPatch::Decode(Again->ConditionalPatchPayload, Base, Nav, Patches, Error)) { AddError(Error); return false; }
	TestTrue(TEXT("Blocker-only patch adds no floor and retains occupancy"),
		Patches.Patches.Num() == 1 && Patches.Patches[0].SupportLayers.IsEmpty() && !Patches.Patches[0].BlockedNodes.IsEmpty());
	AddInfo(Report.ToString());
	return !HasAnyErrors();
}
#endif
