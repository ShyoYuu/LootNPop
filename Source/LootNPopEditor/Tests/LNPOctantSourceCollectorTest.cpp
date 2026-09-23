// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PackageTools.h"
#include "SurfaceNavigation/LNPOctantSourceCollector.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
constexpr TCHAR SourceLevelPackageName[] =
	TEXT("/Game/SurfaceNavigationTests/MeshTerrain/LVI_Octant_COption");
constexpr TCHAR CubeMeshPath[] = TEXT("/Engine/BasicShapes/Cube.Cube");
constexpr TCHAR SphereMeshPath[] = TEXT("/Engine/BasicShapes/Sphere.Sphere");
constexpr TCHAR SurfaceDataPackageName[] =
	TEXT("/Game/SurfaceNavigationTests/Schema/DA_MinimalOctantSurfaceData");
constexpr TCHAR SurfaceDataAssetName[] = TEXT("DA_MinimalOctantSurfaceData");

FLNPOctantSourcePackage MakeManifestEntry(
	const TCHAR* PackageName,
	ELNPOctantSourcePackageKind Kind,
	const TCHAR* Hash)
{
	FLNPOctantSourcePackage Result;
	Result.PackageName = PackageName;
	Result.Kind = Kind;
	Result.PackageSavedHash = FLNPContentHash(FIoHash(FWideStringView(Hash)));
	return Result;
}

FLNPTerrainSourceSemantic MakeSemantic(
	TArray<FName> Tags,
	const FTransform& Transform,
	FName Profile,
	FName MeshPackage)
{
	FLNPTerrainSourceSemantic Result;
	Result.TerrainTags = MoveTemp(Tags);
	Result.Transform = Transform;
	Result.CollisionProfileName = Profile;
	Result.StaticMeshPackageName = MeshPackage;
	return Result;
}

bool IsWithinExternalRoots(FName PackageName, TConstArrayView<FString> Roots)
{
	const FString PackageString = PackageName.ToString();
	return Roots.ContainsByPredicate([&PackageString](const FString& Root)
	{
		return PackageString == Root || PackageString.StartsWith(Root + TEXT("/"));
	});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPOctantSourceHashDeterminismTest,
	"LootNPop.SurfaceNavigation.Schema.SourceManifestAndHashDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPOctantSourceDependencyCollectionTest,
	"LootNPop.SurfaceNavigation.Schema.SourceDependencyCollection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPOctantSurfaceDataPackageRoundTripTest,
	"LootNPop.SurfaceNavigation.Schema.SurfaceDataPackageSaveReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPOctantSourceHashDeterminismTest::RunTest(const FString& Parameters)
{
	const FLNPOctantSourcePackage SourcePackage = MakeManifestEntry(
		TEXT("/Game/Maps/Test/LVI_Test"),
		ELNPOctantSourcePackageKind::SourceLevel,
		TEXT("00112233445566778899aabbccddeeff00112233"));
	const FLNPOctantSourcePackage ExternalPackage = MakeManifestEntry(
		TEXT("/Game/__ExternalActors__/Maps/Test/LVI_Test/1/23/External"),
		ELNPOctantSourcePackageKind::ExternalActor,
		TEXT("102132435465768798a9bacbdcedfe0f10213243"));
	const FLNPOctantSourcePackage MeshPackage = MakeManifestEntry(
		TEXT("/Game/Maps/Meshes/SM_Test"),
		ELNPOctantSourcePackageKind::TerrainMesh,
		TEXT("abcdef0123456789abcdef0123456789abcdef01"));

	TArray<FLNPOctantSourcePackage> ManifestA = {
		MeshPackage,
		SourcePackage,
		ExternalPackage,
		MeshPackage,
		SourcePackage
	};
	TArray<FLNPOctantSourcePackage> ManifestB = {
		ExternalPackage,
		MeshPackage,
		SourcePackage
	};
	FString Error;
	TestTrue(TEXT("Manifest A canonicalization succeeds"),
		FLNPOctantSourceCollector::CanonicalizeManifest(ManifestA, Error));
	TestTrue(TEXT("Manifest B canonicalization succeeds"),
		FLNPOctantSourceCollector::CanonicalizeManifest(ManifestB, Error));
	TestEqual(TEXT("Exact duplicate manifest rows are removed"), ManifestA.Num(), 3);
	TestEqual(TEXT("Canonical manifests have the same count"), ManifestA.Num(), ManifestB.Num());
	for (int32 Index = 0; Index < FMath::Min(ManifestA.Num(), ManifestB.Num()); ++Index)
	{
		TestEqual(TEXT("Canonical package order is stable"), ManifestA[Index].PackageName, ManifestB[Index].PackageName);
		TestEqual(TEXT("Canonical package kind order is stable"), ManifestA[Index].Kind, ManifestB[Index].Kind);
		TestTrue(TEXT("Canonical package hash is stable"),
			ManifestA[Index].PackageSavedHash == ManifestB[Index].PackageSavedHash);
	}

	TArray<FLNPOctantSourcePackage> ConflictingManifest = ManifestB;
	ConflictingManifest.Add(MakeManifestEntry(
		TEXT("/Game/Maps/Meshes/SM_Test"),
		ELNPOctantSourcePackageKind::TerrainMesh,
		TEXT("ffeeddccbbaa99887766554433221100ffeeddcc")));
	TestFalse(TEXT("Conflicting duplicate package hashes are rejected"),
		FLNPOctantSourceCollector::CanonicalizeManifest(ConflictingManifest, Error));
	TestTrue(TEXT("Conflicting package hash reports a reason"), !Error.IsEmpty());

	const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(47.0));
	const FTransform TransformA(Rotation, FVector(100.0, -200.0, 300.0), FVector(1.0, 2.0, 0.5));
	const FTransform TransformB(FQuat::Identity, FVector(-50.0, 25.0, 10.0), FVector::OneVector);
	const FLNPTerrainSourceSemantic SemanticA = MakeSemantic(
		{TEXT("LNP.Surface.Static"), TEXT("LNP.Surface.Support"), TEXT("Ignored.Authoring.Tag")},
		TransformA,
		TEXT("LNPStaticSupport"),
		TEXT("/Game/Maps/Meshes/SM_Test"));
	const FLNPTerrainSourceSemantic SemanticB = MakeSemantic(
		{TEXT("LNP.Surface.Blocker"), TEXT("LNP.Surface.Static")},
		TransformB,
		TEXT("LNPStaticBlocker"),
		TEXT("/Game/Maps/Meshes/SM_Blocker"));
	FLNPTerrainSourceSemantic SemanticAReordered = SemanticA;
	SemanticAReordered.TerrainTags = {
		TEXT("LNP.Surface.Support"),
		TEXT("LNP.Surface.Static"),
		TEXT("LNP.Surface.Support")
	};
	FQuat NegatedRotation = Rotation * -1.0;
	SemanticAReordered.Transform.SetRotation(NegatedRotation);

	FLNPContentHash SemanticHashA;
	FLNPContentHash SemanticHashB;
	TestTrue(TEXT("Semantic hash A succeeds"),
		FLNPOctantSourceCollector::BuildSourceSemanticHash(
			TArray<FLNPTerrainSourceSemantic>{SemanticA, SemanticB}, SemanticHashA, Error));
	TestTrue(TEXT("Semantic hash B succeeds"),
		FLNPOctantSourceCollector::BuildSourceSemanticHash(
			TArray<FLNPTerrainSourceSemantic>{SemanticB, SemanticAReordered}, SemanticHashB, Error));
	TestTrue(TEXT("Semantic hash ignores component/tag input order and quaternion sign"),
		SemanticHashA == SemanticHashB);

	FLNPTerrainSourceSemantic ChangedSemantic = SemanticB;
	ChangedSemantic.CollisionProfileName = TEXT("LNPStaticTerrain");
	FLNPContentHash ChangedSemanticHash;
	TestTrue(TEXT("Changed semantic hash succeeds"),
		FLNPOctantSourceCollector::BuildSourceSemanticHash(
			TArray<FLNPTerrainSourceSemantic>{SemanticA, ChangedSemantic}, ChangedSemanticHash, Error));
	TestTrue(TEXT("Collision profile changes semantic hash"), ChangedSemanticHash != SemanticHashA);

	const TArray<FLNPOctantBakeSetting> SettingsA = {
		FLNPOctantBakeSetting::Real(TEXT("SupportSpacingCm"), 100.0),
		FLNPOctantBakeSetting::UnsignedInteger(TEXT("MaxLayers"), 4),
		FLNPOctantBakeSetting::Boolean(TEXT("BuildNavigation"), true),
		FLNPOctantBakeSetting::Real(TEXT("SupportSpacingCm"), 100.0)
	};
	const TArray<FLNPOctantBakeSetting> SettingsB = {
		FLNPOctantBakeSetting::Boolean(TEXT("BuildNavigation"), true),
		FLNPOctantBakeSetting::Real(TEXT("SupportSpacingCm"), 100.0),
		FLNPOctantBakeSetting::UnsignedInteger(TEXT("MaxLayers"), 4)
	};
	FLNPContentHash SettingsHashA;
	FLNPContentHash SettingsHashB;
	FLNPContentHash DifferentVersionHash;
	TestTrue(TEXT("Bake settings hash A succeeds"),
		FLNPOctantSourceCollector::BuildBakeSettingsHash(7, SettingsA, SettingsHashA, Error));
	TestTrue(TEXT("Bake settings hash B succeeds"),
		FLNPOctantSourceCollector::BuildBakeSettingsHash(7, SettingsB, SettingsHashB, Error));
	TestTrue(TEXT("Bake settings hash ignores order and identical duplicates"), SettingsHashA == SettingsHashB);
	TestTrue(TEXT("Different baker schema version hash succeeds"),
		FLNPOctantSourceCollector::BuildBakeSettingsHash(8, SettingsB, DifferentVersionHash, Error));
	TestTrue(TEXT("Baker schema version changes settings hash"), DifferentVersionHash != SettingsHashA);

	TArray<FLNPOctantBakeSetting> ConflictingSettings = SettingsB;
	ConflictingSettings.Add(FLNPOctantBakeSetting::Real(TEXT("SupportSpacingCm"), 200.0));
	FLNPContentHash RejectedSettingsHash;
	TestFalse(TEXT("Conflicting duplicate bake settings are rejected"),
		FLNPOctantSourceCollector::BuildBakeSettingsHash(7, ConflictingSettings, RejectedSettingsHash, Error));

	FLNPContentHash SourceContentHashA;
	FLNPContentHash SourceContentHashB;
	TestTrue(TEXT("Source content hash A succeeds"),
		FLNPOctantSourceCollector::BuildSourceContentHash(
			TArray<FLNPOctantSourcePackage>{MeshPackage, SourcePackage, ExternalPackage, MeshPackage},
			SemanticHashA,
			SettingsHashA,
			SourceContentHashA,
			Error));
	TestTrue(TEXT("Source content hash B succeeds"),
		FLNPOctantSourceCollector::BuildSourceContentHash(
			TArray<FLNPOctantSourcePackage>{ExternalPackage, SourcePackage, MeshPackage},
			SemanticHashB,
			SettingsHashB,
			SourceContentHashB,
			Error));
	TestTrue(TEXT("Source content hash ignores manifest/semantic/settings input order and duplicates"),
		SourceContentHashA == SourceContentHashB);

	FLNPContentHash SourceWithChangedSettings;
	TestTrue(TEXT("Changed settings source hash succeeds"),
		FLNPOctantSourceCollector::BuildSourceContentHash(
			ManifestB,
			SemanticHashA,
			DifferentVersionHash,
			SourceWithChangedSettings,
			Error));
	TestTrue(TEXT("BakeSettingsHash contributes to SourceContentHash"),
		SourceWithChangedSettings != SourceContentHashA);

	return !HasAnyErrors();
}

bool FLNPOctantSourceDependencyCollectionTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();

	TArray<FLNPOctantSourcePackage> LevelManifest;
	FString Error;
	const bool bCollectedLevelPackages = FLNPOctantSourceCollector::CollectLevelPackages(
		FName(SourceLevelPackageName), AssetRegistry, LevelManifest, Error);
	TestTrue(TEXT("Source LVI and direct external package collection succeeds"), bCollectedLevelPackages);
	if (!bCollectedLevelPackages)
	{
		AddError(Error);
		return false;
	}

	int32 SourceLevelCount = 0;
	int32 ExternalPackageCount = 0;
	const TArray<FString> ExternalRoots = ULevel::GetExternalObjectsPaths(SourceLevelPackageName);
	for (const FLNPOctantSourcePackage& Entry : LevelManifest)
	{
		if (Entry.Kind == ELNPOctantSourcePackageKind::SourceLevel)
		{
			++SourceLevelCount;
			TestEqual(TEXT("Source manifest keeps the requested LVI package"),
				Entry.PackageName, FName(SourceLevelPackageName));
		}
		else if (Entry.Kind == ELNPOctantSourcePackageKind::ExternalActor)
		{
			++ExternalPackageCount;
			TestTrue(TEXT("External manifest entry belongs to the source LVI roots"),
				IsWithinExternalRoots(Entry.PackageName, ExternalRoots));
		}
		else
		{
			AddError(FString::Printf(
				TEXT("Level-only collector admitted non-level dependency '%s'."),
				*Entry.PackageName.ToString()));
		}
	}
	TestEqual(TEXT("Source LVI appears exactly once"), SourceLevelCount, 1);
	TestTrue(TEXT("Source LVI exposes at least one direct external actor/object package"),
		ExternalPackageCount > 0);

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, CubeMeshPath);
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, SphereMeshPath);
	TestNotNull(TEXT("Cube mesh fixture loads"), CubeMesh);
	TestNotNull(TEXT("Sphere mesh fixture loads"), SphereMesh);
	if (!CubeMesh || !SphereMesh)
	{
		return false;
	}

	TStrongObjectPtr<UWorld> TestWorld(NewObject<UWorld>(GetTransientPackage()));
	TestWorld->WorldType = EWorldType::EditorPreview;
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(TestWorld->WorldType);
	WorldContext.SetCurrentWorld(TestWorld.Get());
	TestWorld->InitializeNewWorld(UWorld::InitializationValues()
		.AllowAudioPlayback(false)
		.CreatePhysicsScene(false)
		.RequiresHitProxies(false)
		.CreateNavigation(false)
		.CreateAISystem(false)
		.ShouldSimulatePhysics(false)
		.SetTransactional(false));

	auto SpawnMeshActor = [&TestWorld](UStaticMesh* Mesh, TArray<FName> Tags, const FVector& Location)
	{
		AStaticMeshActor* Actor = TestWorld->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
		if (Actor)
		{
			UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
			Component->SetStaticMesh(Mesh);
			Component->ComponentTags = MoveTemp(Tags);
			Component->SetCollisionProfileName(TEXT("BlockAll"));
		}
		return Actor;
	};

	AStaticMeshActor* TerrainActorA = SpawnMeshActor(
		CubeMesh,
		{TEXT("LNP.Surface.Support"), TEXT("LNP.Surface.Blocker"), TEXT("LNP.Surface.Static"), TEXT("Ignored.Tag")},
		FVector(10.0, 20.0, 30.0));
	AStaticMeshActor* TerrainActorB = SpawnMeshActor(
		CubeMesh,
		{TEXT("LNP.Surface.Support"), TEXT("LNP.Surface.Static")},
		FVector(-10.0, 15.0, 45.0));
	AStaticMeshActor* DecorationActor = SpawnMeshActor(
		SphereMesh,
		{TEXT("LNP.Surface.Decoration")},
		FVector(0.0, 100.0, 0.0));
	AStaticMeshActor* UntaggedActor = SpawnMeshActor(
		SphereMesh,
		{},
		FVector(0.0, -100.0, 0.0));
	TestNotNull(TEXT("First Terrain Contract actor spawns"), TerrainActorA);
	TestNotNull(TEXT("Second Terrain Contract actor spawns"), TerrainActorB);
	TestNotNull(TEXT("Decoration actor spawns"), DecorationActor);
	TestNotNull(TEXT("Untagged actor spawns"), UntaggedActor);

	TArray<FLNPOctantSourcePackage> MeshManifest;
	TArray<FLNPTerrainSourceSemantic> Semantics;
	const bool bCollectedTerrain = TerrainActorA && TerrainActorB && DecorationActor && UntaggedActor
		&& FLNPOctantSourceCollector::CollectTerrainComponents(
			*TestWorld,
			AssetRegistry,
			MeshManifest,
			Semantics,
			Error);
	TestTrue(TEXT("Terrain Contract Static Mesh collection succeeds"), bCollectedTerrain);
	if (!bCollectedTerrain)
	{
		AddError(Error);
	}
	else
	{
		TestEqual(TEXT("Only role components contribute semantic rows"), Semantics.Num(), 2);
		TestEqual(TEXT("Shared Terrain Mesh package is deduplicated"), MeshManifest.Num(), 1);
		if (MeshManifest.Num() == 1)
		{
			TestEqual(TEXT("Only the role mesh package enters the manifest"),
				MeshManifest[0].PackageName, CubeMesh->GetOutermost()->GetFName());
			TestEqual(TEXT("Role mesh package uses TerrainMesh kind"),
				MeshManifest[0].Kind, ELNPOctantSourcePackageKind::TerrainMesh);
		}
		for (const FLNPTerrainSourceSemantic& Semantic : Semantics)
		{
			TestFalse(TEXT("Unrelated Component Tags do not enter semantic input"),
				Semantic.TerrainTags.Contains(TEXT("Ignored.Tag")));
			TestEqual(TEXT("Decoration mesh never enters role semantics"),
				Semantic.StaticMeshPackageName, CubeMesh->GetOutermost()->GetFName());
		}

		const TArray<FLNPOctantBakeSetting> BakeSettings = {
			FLNPOctantBakeSetting::Real(TEXT("SupportSpacingCm"), 100.0),
			FLNPOctantBakeSetting::Boolean(TEXT("BuildNavigation"), true)
		};
		FLNPOctantSourceCollection CompleteCollection;
		const bool bCollectedCompleteSource = FLNPOctantSourceCollector::CollectFromWorld(
			*TestWorld,
			FName(SourceLevelPackageName),
			AssetRegistry,
			3,
			BakeSettings,
			CompleteCollection,
			Error);
		TestTrue(TEXT("Complete source manifest and aggregate hash collection succeeds"),
			bCollectedCompleteSource);
		if (!bCollectedCompleteSource)
		{
			AddError(Error);
		}
		else
		{
			TestTrue(TEXT("Complete source manifest includes Level, external, and Terrain Mesh packages"),
				CompleteCollection.Manifest.ContainsByPredicate([](const FLNPOctantSourcePackage& Entry)
				{
					return Entry.Kind == ELNPOctantSourcePackageKind::SourceLevel;
				})
				&& CompleteCollection.Manifest.ContainsByPredicate([](const FLNPOctantSourcePackage& Entry)
				{
					return Entry.Kind == ELNPOctantSourcePackageKind::ExternalActor;
				})
				&& CompleteCollection.Manifest.ContainsByPredicate([](const FLNPOctantSourcePackage& Entry)
				{
					return Entry.Kind == ELNPOctantSourcePackageKind::TerrainMesh;
				}));
			TestFalse(TEXT("Complete SourceSemanticHash is non-zero"),
				CompleteCollection.SourceSemanticHash.IsZero());
			TestFalse(TEXT("Complete BakeSettingsHash is non-zero"),
				CompleteCollection.BakeSettingsHash.IsZero());
			TestFalse(TEXT("Complete SourceContentHash is non-zero"),
				CompleteCollection.SourceContentHash.IsZero());
		}
	}

	GEngine->DestroyWorldContext(TestWorld.Get());
	TestWorld->DestroyWorld(true);
	TestWorld.Reset();
	return !HasAnyErrors();
}

bool FLNPOctantSurfaceDataPackageRoundTripTest::RunTest(const FString& Parameters)
{
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), SurfaceDataPackageName, SurfaceDataAssetName);
	ULNPOctantSurfaceData* SurfaceData = FPackageName::DoesPackageExist(SurfaceDataPackageName)
		? LoadObject<ULNPOctantSurfaceData>(nullptr, *ObjectPath)
		: nullptr;
	bool bCreatedAsset = false;
	if (!SurfaceData)
	{
		UPackage* Package = CreatePackage(SurfaceDataPackageName);
		SurfaceData = NewObject<ULNPOctantSurfaceData>(
			Package,
			SurfaceDataAssetName,
			RF_Public | RF_Standalone);
		bCreatedAsset = SurfaceData != nullptr;
	}
	TestNotNull(TEXT("Minimal SurfaceData asset exists"), SurfaceData);
	if (!SurfaceData)
	{
		return false;
	}
	if (bCreatedAsset)
	{
		FAssetRegistryModule::AssetCreated(SurfaceData);
	}

	SurfaceData->Header = FLNPSurfaceBakeHeader();
	SurfaceData->Header.SourceSemanticHash = FLNPContentHash(
		FIoHash(TEXTVIEW("00112233445566778899aabbccddeeff00112233")));
	SurfaceData->Header.BakeSettingsHash = FLNPContentHash(
		FIoHash(TEXTVIEW("102132435465768798a9bacbdcedfe0f10213243")));
	SurfaceData->Header.SourceContentHash = FLNPContentHash(
		FIoHash(TEXTVIEW("abcdef0123456789abcdef0123456789abcdef01")));
	SurfaceData->Header.SourceManifest = {
		MakeManifestEntry(
			TEXT("/Game/Maps/Test/LVI_Test"),
			ELNPOctantSourcePackageKind::SourceLevel,
			TEXT("ffeeddccbbaa99887766554433221100ffeeddcc")),
		MakeManifestEntry(
			TEXT("/Game/Maps/Meshes/SM_Test"),
			ELNPOctantSourcePackageKind::TerrainMesh,
			TEXT("1234567890abcdef1234567890abcdef12345678"))
	};
	SurfaceData->SupportPayload = {0x10, 0x20, 0x30};
	SurfaceData->NavigationPayload = {0x40, 0x50};
	SurfaceData->TraversalPayload = {0x60};
	SurfaceData->SpawnPayload = {0x70, 0x80, 0x90, 0xa0};
	SurfaceData->Header.Support.ElementCount = 3;
	SurfaceData->Header.Support.UncompressedSize = SurfaceData->SupportPayload.Num();
	SurfaceData->Header.Support.ContentHash = FLNPContentHash(
		FIoHash(TEXTVIEW("234567890abcdef1234567890abcdef123456789")));
	SurfaceData->Header.Navigation.ElementCount = 2;
	SurfaceData->Header.Navigation.UncompressedSize = SurfaceData->NavigationPayload.Num();
	SurfaceData->Header.Navigation.ContentHash = FLNPContentHash(
		FIoHash(TEXTVIEW("34567890abcdef1234567890abcdef1234567890")));
	SurfaceData->Header.Traversal.ElementCount = 1;
	SurfaceData->Header.Traversal.UncompressedSize = SurfaceData->TraversalPayload.Num();
	SurfaceData->Header.Traversal.ContentHash = FLNPContentHash(
		FIoHash(TEXTVIEW("4567890abcdef1234567890abcdef12345678901")));
	SurfaceData->Header.Spawn.ElementCount = 4;
	SurfaceData->Header.Spawn.UncompressedSize = SurfaceData->SpawnPayload.Num();
	SurfaceData->Header.Spawn.ContentHash = FLNPContentHash(
		FIoHash(TEXTVIEW("567890abcdef1234567890abcdef123456789012")));

	UPackage* Package = SurfaceData->GetOutermost();
	Package->MarkPackageDirty();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		SurfaceDataPackageName,
		FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	TestTrue(TEXT("Minimal SurfaceData saves to a real asset package"),
		UPackage::SavePackage(Package, SurfaceData, *PackageFilename, SaveArgs));

	FText ReloadError;
	TestTrue(TEXT("Minimal SurfaceData package reloads without interaction"),
		UPackageTools::ReloadPackages(
			{Package},
			ReloadError,
			EReloadPackagesInteractionMode::AssumePositive));
	if (!ReloadError.IsEmpty())
	{
		AddError(FString::Printf(TEXT("SurfaceData package reload reported: %s"), *ReloadError.ToString()));
	}

	ULNPOctantSurfaceData* Reloaded = LoadObject<ULNPOctantSurfaceData>(nullptr, *ObjectPath);
	TestNotNull(TEXT("Reloaded SurfaceData resolves by object path"), Reloaded);
	if (Reloaded)
	{
		TestEqual(TEXT("Reload preserves DataVersion"),
			Reloaded->Header.DataVersion, FLNPSurfaceBakeHeader::CurrentDataVersion);
		TestEqual(TEXT("Reload preserves manifest count"), Reloaded->Header.SourceManifest.Num(), 2);
		TestTrue(TEXT("Reload preserves SourceContentHash"),
			Reloaded->Header.SourceContentHash == FLNPContentHash(
				FIoHash(TEXTVIEW("abcdef0123456789abcdef0123456789abcdef01"))));
		TestTrue(TEXT("Reload preserves Support payload"),
			Reloaded->SupportPayload == TArray<uint8>({0x10, 0x20, 0x30}));
		TestTrue(TEXT("Reload preserves Navigation payload"),
			Reloaded->NavigationPayload == TArray<uint8>({0x40, 0x50}));
		TestTrue(TEXT("Reload preserves Traversal payload"),
			Reloaded->TraversalPayload == TArray<uint8>({0x60}));
		TestTrue(TEXT("Reload preserves Spawn payload"),
			Reloaded->SpawnPayload == TArray<uint8>({0x70, 0x80, 0x90, 0xa0}));
		TestEqual(TEXT("Reload preserves Support element count"),
			Reloaded->Header.Support.ElementCount, 3u);
		TestEqual(TEXT("Reload preserves Navigation element count"),
			Reloaded->Header.Navigation.ElementCount, 2u);
		TestEqual(TEXT("Reload preserves Traversal element count"),
			Reloaded->Header.Traversal.ElementCount, 1u);
		TestEqual(TEXT("Reload preserves Spawn element count"),
			Reloaded->Header.Spawn.ElementCount, 4u);
	}

	return !HasAnyErrors();
}

#endif
