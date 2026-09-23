// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataAsset/LNPOctantDefinition.h"
#include "DataAsset/LNPOctantPoolData.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPOctantSurfaceDataSerializationTest,
	"LootNPop.SurfaceNavigation.Schema.SurfaceDataSerializationRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPOctantDeterministicSelectionTest,
	"LootNPop.SurfaceNavigation.Schema.DeterministicOctantSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPOctantSurfaceDataCookedLoadTest,
	"LootNPop.SurfaceNavigation.PackagedRuntime.MinimalSurfaceDataCookedLoad",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FLNPOctantSurfaceDataSerializationTest::RunTest(const FString& Parameters)
{
	const FIoHash SourcePackageHash(TEXTVIEW("00112233445566778899aabbccddeeff00112233"));
	const FIoHash SourceContentHash(TEXTVIEW("ffeeddccbbaa99887766554433221100ffeeddcc"));
	const FIoHash SemanticHash(TEXTVIEW("102132435465768798a9bacbdcedfe0f10213243"));
	const FIoHash SettingsHash(TEXTVIEW("abcdef0123456789abcdef0123456789abcdef01"));
	const FIoHash PayloadHash(TEXTVIEW("1234567890abcdef1234567890abcdef12345678"));

	const FLNPContentHash ReflectedHash(SourcePackageHash);
	TestEqual(TEXT("FIoHash wrapper preserves all 20 bytes"), ReflectedHash.ToIoHash(), SourcePackageHash);
	TestEqual(TEXT("FIoHash wrapper preserves the 40-character form"), ReflectedHash.ToString(), LexToString(SourcePackageHash));

	ULNPOctantSurfaceData* Source = NewObject<ULNPOctantSurfaceData>(GetTransientPackage());
	Source->Header.DataVersion = FLNPSurfaceBakeHeader::CurrentDataVersion;
	Source->Header.SourceContentHash = FLNPContentHash(SourceContentHash);
	Source->Header.SourceSemanticHash = FLNPContentHash(SemanticHash);
	Source->Header.BakeSettingsHash = FLNPContentHash(SettingsHash);

	FLNPOctantSourcePackage& ManifestEntry = Source->Header.SourceManifest.AddDefaulted_GetRef();
	ManifestEntry.PackageName = TEXT("/Game/Maps/Meadow_00/LVI_Octant_Meadow_00");
	ManifestEntry.Kind = ELNPOctantSourcePackageKind::SourceLevel;
	ManifestEntry.PackageSavedHash = ReflectedHash;

	Source->SupportPayload = {0x10, 0x20, 0x30, 0x40};
	Source->NavigationPayload = {0x50, 0x60};
	Source->TraversalPayload = {0x70};
	Source->SpawnPayload = {0x80, 0x90, 0xa0};
	Source->Header.Support.ElementCount = 2;
	Source->Header.Support.UncompressedSize = Source->SupportPayload.Num();
	Source->Header.Support.ContentHash = FLNPContentHash(PayloadHash);

	TArray<uint8> SerializedData;
	FObjectWriter Writer(Source, SerializedData);
	TestTrue(TEXT("SurfaceData produces serialized bytes"), !SerializedData.IsEmpty());

	ULNPOctantSurfaceData* RoundTripped = NewObject<ULNPOctantSurfaceData>(GetTransientPackage());
	FObjectReader Reader(RoundTripped, SerializedData);
	TestFalse(TEXT("SurfaceData memory archive loads without an error"), Reader.IsError());
	TestEqual(TEXT("Round trip preserves DataVersion"), RoundTripped->Header.DataVersion, Source->Header.DataVersion);
	TestTrue(TEXT("Round trip preserves SourceContentHash"),
		RoundTripped->Header.SourceContentHash == Source->Header.SourceContentHash);
	TestTrue(TEXT("Round trip preserves SourceSemanticHash"),
		RoundTripped->Header.SourceSemanticHash == Source->Header.SourceSemanticHash);
	TestTrue(TEXT("Round trip preserves BakeSettingsHash"),
		RoundTripped->Header.BakeSettingsHash == Source->Header.BakeSettingsHash);
	TestEqual(TEXT("Round trip preserves source manifest length"), RoundTripped->Header.SourceManifest.Num(), 1);
	if (RoundTripped->Header.SourceManifest.Num() == 1)
	{
		const FLNPOctantSourcePackage& RoundTrippedEntry = RoundTripped->Header.SourceManifest[0];
		TestEqual(TEXT("Round trip preserves manifest package"), RoundTrippedEntry.PackageName, ManifestEntry.PackageName);
		TestEqual(TEXT("Round trip preserves manifest kind"), RoundTrippedEntry.Kind, ManifestEntry.Kind);
		TestTrue(TEXT("Round trip preserves manifest package hash"),
			RoundTrippedEntry.PackageSavedHash == ManifestEntry.PackageSavedHash);
	}
	TestTrue(TEXT("Round trip preserves Support payload"), RoundTripped->SupportPayload == Source->SupportPayload);
	TestTrue(TEXT("Round trip preserves Navigation payload"), RoundTripped->NavigationPayload == Source->NavigationPayload);
	TestTrue(TEXT("Round trip preserves Traversal payload"), RoundTripped->TraversalPayload == Source->TraversalPayload);
	TestTrue(TEXT("Round trip preserves Spawn payload"), RoundTripped->SpawnPayload == Source->SpawnPayload);
	TestEqual(TEXT("Round trip preserves Support element count"),
		RoundTripped->Header.Support.ElementCount, Source->Header.Support.ElementCount);
	TestTrue(TEXT("Round trip preserves Support payload hash"),
		RoundTripped->Header.Support.ContentHash == Source->Header.Support.ContentHash);

	ULNPOctantPoolData* Pool = NewObject<ULNPOctantPoolData>(GetTransientPackage());
	FLNPOctantDefinition& Definition = Pool->OctantDefinitions.AddDefaulted_GetRef();
	Definition.LevelAsset = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Meadow_00/LVI_Octant_Meadow_00.LVI_Octant_Meadow_00")));
	Definition.SurfaceData = TSoftObjectPtr<ULNPOctantSurfaceData>(
		FSoftObjectPath(TEXT("/Game/SurfaceNavigation/DA_Meadow_00_Surface.DA_Meadow_00_Surface")));
	Definition.AllowedSlotRotations = static_cast<uint8>(ELNPOctantSlotRotation::Pitch0Yaw0)
		| static_cast<uint8>(ELNPOctantSlotRotation::Pitch180Yaw0);
	Definition.SeamSignature = TEXT("DysonSphere_R250m_V1");
	TestTrue(TEXT("Definition permits an enabled slot"), Definition.AllowsSlot(0));
	TestFalse(TEXT("Definition rejects a disabled slot"), Definition.AllowsSlot(1));
	TestTrue(TEXT("Definition permits its second enabled slot"), Definition.AllowsSlot(4));
	TestFalse(TEXT("Definition rejects an out-of-range slot"), Definition.AllowsSlot(8));

	TArray<uint8> SerializedPoolData;
	FObjectWriter PoolWriter(Pool, SerializedPoolData);
	ULNPOctantPoolData* RoundTrippedPool = NewObject<ULNPOctantPoolData>(GetTransientPackage());
	FObjectReader PoolReader(RoundTrippedPool, SerializedPoolData);
	TestFalse(TEXT("Octant pool memory archive loads without an error"), PoolReader.IsError());
	TestEqual(TEXT("Round trip preserves definition count"), RoundTrippedPool->OctantDefinitions.Num(), 1);
	if (RoundTrippedPool->OctantDefinitions.Num() == 1)
	{
		const FLNPOctantDefinition& RoundTrippedDefinition = RoundTrippedPool->OctantDefinitions[0];
		TestEqual(TEXT("Round trip preserves LevelAsset"),
			RoundTrippedDefinition.LevelAsset.ToSoftObjectPath(), Definition.LevelAsset.ToSoftObjectPath());
		TestEqual(TEXT("Round trip preserves SurfaceData"),
			RoundTrippedDefinition.SurfaceData.ToSoftObjectPath(), Definition.SurfaceData.ToSoftObjectPath());
		TestEqual(TEXT("Round trip preserves allowed slots"),
			RoundTrippedDefinition.AllowedSlotRotations, Definition.AllowedSlotRotations);
		TestEqual(TEXT("Round trip preserves seam signature"),
			RoundTrippedDefinition.SeamSignature, Definition.SeamSignature);
	}

	return !HasAnyErrors();
}

bool FLNPOctantDeterministicSelectionTest::RunTest(const FString& Parameters)
{
	ULNPOctantPoolData* LegacyPool = NewObject<ULNPOctantPoolData>(GetTransientPackage());
	LegacyPool->OctantPool = {
		TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Tests/LegacyA.LegacyA"))),
		TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Tests/LegacyB.LegacyB")))
	};

	TArray<FLNPOctantDefinition> MigratedDefinitions;
	LegacyPool->BuildEffectiveDefinitions(MigratedDefinitions);
	TestEqual(TEXT("Legacy pool fallback preserves entry count"), MigratedDefinitions.Num(), LegacyPool->OctantPool.Num());
	for (int32 Index = 0; Index < MigratedDefinitions.Num(); ++Index)
	{
		TestEqual(
			TEXT("Legacy pool fallback preserves Level order"),
			MigratedDefinitions[Index].LevelAsset.ToSoftObjectPath(),
			LegacyPool->OctantPool[Index].ToSoftObjectPath());
		TestEqual(
			TEXT("Legacy pool fallback permits every slot"),
			MigratedDefinitions[Index].AllowedSlotRotations,
			static_cast<uint8>(ELNPOctantSlotRotation::All));
		TestTrue(TEXT("Legacy pool fallback leaves SurfaceData empty"), MigratedDefinitions[Index].SurfaceData.IsNull());
	}

	TArray<FLNPOctantDefinition> Definitions;
	const uint8 AllowedMasks[] = {0xff, 0x0f, 0xf0, 0xaa};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(AllowedMasks); ++Index)
	{
		FLNPOctantDefinition& Definition = Definitions.AddDefaulted_GetRef();
		Definition.LevelAsset = TSoftObjectPtr<UWorld>(FSoftObjectPath(
			FString::Printf(TEXT("/Game/Tests/Octant%d.Octant%d"), Index, Index)));
		Definition.SurfaceData = TSoftObjectPtr<ULNPOctantSurfaceData>(FSoftObjectPath(
			FString::Printf(TEXT("/Game/Tests/Surface%d.Surface%d"), Index, Index)));
		Definition.AllowedSlotRotations = AllowedMasks[Index];
		Definition.SeamSignature = FName(*FString::Printf(TEXT("Seam%d"), Index));
	}

	constexpr int32 SharedSeed = 135792468;
	TArray<FLNPOctantDefinition> ServerSelection;
	TArray<FLNPOctantDefinition> ClientSelection;
	TArray<int32> ServerIndices;
	TArray<int32> ClientIndices;
	FString ServerError;
	FString ClientError;

	const bool bServerSelectionSucceeded = ULNPOctantSpawnSubsystem::SelectOctantDefinitions(
		Definitions, SharedSeed, ServerSelection, &ServerIndices, &ServerError);
	const bool bClientSelectionSucceeded = ULNPOctantSpawnSubsystem::SelectOctantDefinitions(
		Definitions, SharedSeed, ClientSelection, &ClientIndices, &ClientError);
	TestTrue(TEXT("Server selection succeeds"), bServerSelectionSucceeded);
	TestTrue(TEXT("Client selection succeeds"), bClientSelectionSucceeded);
	TestEqual(TEXT("Server selection fills all slots"), ServerSelection.Num(), 8);
	TestTrue(TEXT("Same seed and pool preserve identical source indices"), ServerIndices == ClientIndices);
	TestEqual(TEXT("Same seed and pool preserve identical definition count"), ClientSelection.Num(), ServerSelection.Num());

	const int32 ComparableSelectionCount = FMath::Min3(
		ServerSelection.Num(), ClientSelection.Num(), ServerIndices.Num());
	for (int32 SlotIndex = 0; SlotIndex < ComparableSelectionCount; ++SlotIndex)
	{
		const FLNPOctantDefinition& ServerDefinition = ServerSelection[SlotIndex];
		const FLNPOctantDefinition& ClientDefinition = ClientSelection[SlotIndex];
		const FLNPOctantDefinition& SourceDefinition = Definitions[ServerIndices[SlotIndex]];

		TestTrue(TEXT("Selected definition permits its slot"), ServerDefinition.AllowsSlot(static_cast<uint8>(SlotIndex)));
		TestEqual(
			TEXT("Server and client preserve the same Level"),
			ServerDefinition.LevelAsset.ToSoftObjectPath(),
			ClientDefinition.LevelAsset.ToSoftObjectPath());
		TestEqual(
			TEXT("Selection preserves SurfaceData"),
			ServerDefinition.SurfaceData.ToSoftObjectPath(),
			SourceDefinition.SurfaceData.ToSoftObjectPath());
		TestEqual(
			TEXT("Selection preserves allowed slot mask"),
			ServerDefinition.AllowedSlotRotations,
			SourceDefinition.AllowedSlotRotations);
		TestEqual(
			TEXT("Selection preserves seam signature"),
			ServerDefinition.SeamSignature,
			SourceDefinition.SeamSignature);
	}

	TArray<FLNPOctantDefinition> FailedSelection;
	FString FailureReason;
	TestFalse(
		TEXT("Empty definition pool fails explicitly"),
		ULNPOctantSpawnSubsystem::SelectOctantDefinitions(
			TArray<FLNPOctantDefinition>(), SharedSeed, FailedSelection, nullptr, &FailureReason));
	TestTrue(TEXT("Empty pool reports a reason"), !FailureReason.IsEmpty());

	TArray<FLNPOctantDefinition> UnplaceableDefinitions = Definitions;
	for (FLNPOctantDefinition& Definition : UnplaceableDefinitions)
	{
		Definition.AllowedSlotRotations &= static_cast<uint8>(~(1u << 7));
	}
	TestFalse(
		TEXT("Pool without a slot 7 candidate fails explicitly"),
		ULNPOctantSpawnSubsystem::SelectOctantDefinitions(
			UnplaceableDefinitions, SharedSeed, FailedSelection, nullptr, &FailureReason));
	TestTrue(TEXT("Unplaceable pool identifies slot 7"), FailureReason.Contains(TEXT("slot 7")));

	return !HasAnyErrors();
}

bool FLNPOctantSurfaceDataCookedLoadTest::RunTest(const FString& Parameters)
{
	constexpr TCHAR SurfaceDataObjectPath[] =
		TEXT("/Game/SurfaceNavigationTests/Schema/DA_MinimalOctantSurfaceData.DA_MinimalOctantSurfaceData");

	ULNPOctantSurfaceData* SurfaceData = LoadObject<ULNPOctantSurfaceData>(nullptr, SurfaceDataObjectPath);
	TestNotNull(TEXT("Cooked minimal SurfaceData loads by object path"), SurfaceData);
	if (!SurfaceData)
	{
		return false;
	}

	TestEqual(TEXT("Cooked header preserves DataVersion"),
		SurfaceData->Header.DataVersion, FLNPSurfaceBakeHeader::CurrentDataVersion);
	TestEqual(TEXT("Cooked header preserves manifest count"), SurfaceData->Header.SourceManifest.Num(), 2);
	TestTrue(TEXT("Cooked header preserves SourceContentHash"),
		SurfaceData->Header.SourceContentHash == FLNPContentHash(
			FIoHash(TEXTVIEW("abcdef0123456789abcdef0123456789abcdef01"))));

	TestTrue(TEXT("Cooked package preserves Support payload"),
		SurfaceData->SupportPayload == TArray<uint8>({0x10, 0x20, 0x30}));
	TestTrue(TEXT("Cooked package preserves Navigation payload"),
		SurfaceData->NavigationPayload == TArray<uint8>({0x40, 0x50}));
	TestTrue(TEXT("Cooked package preserves Traversal payload"),
		SurfaceData->TraversalPayload == TArray<uint8>({0x60}));
	TestTrue(TEXT("Cooked package preserves Spawn payload"),
		SurfaceData->SpawnPayload == TArray<uint8>({0x70, 0x80, 0x90, 0xa0}));

	TestEqual(TEXT("Cooked header preserves Support descriptor count"),
		SurfaceData->Header.Support.ElementCount, 3u);
	TestEqual(TEXT("Cooked header preserves Navigation descriptor count"),
		SurfaceData->Header.Navigation.ElementCount, 2u);
	TestEqual(TEXT("Cooked header preserves Traversal descriptor count"),
		SurfaceData->Header.Traversal.ElementCount, 1u);
	TestEqual(TEXT("Cooked header preserves Spawn descriptor count"),
		SurfaceData->Header.Spawn.ElementCount, 4u);

	AddInfo(TEXT("Cooked minimal SurfaceData preserved its header, manifest, and four payload streams."));
	return !HasAnyErrors();
}

#endif
