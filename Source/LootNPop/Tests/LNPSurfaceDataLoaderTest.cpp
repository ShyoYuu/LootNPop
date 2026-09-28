// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Config/LNPSettings.h"
#include "DataAsset/LNPOctantPoolData.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "SurfaceNavigation/LNPSpawnData.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"

namespace
{
	constexpr TCHAR TestLevelPath[] = TEXT("/Game/Tests/LVI_SurfaceLoader.LVI_SurfaceLoader");
	constexpr TCHAR TestSurfacePath[] = TEXT("/Game/Tests/DA_SurfaceLoader.DA_SurfaceLoader");

	ULNPOctantSurfaceData* MakeSurfaceData(const double ChangedRadius = 30000.0)
	{
		ULNPOctantSurfaceData* Data = NewObject<ULNPOctantSurfaceData>(GetTransientPackage());
		Data->Header.DataVersion = FLNPSurfaceBakeHeader::CurrentDataVersion;

		FLNPOctantSourcePackage& Manifest = Data->Header.SourceManifest.AddDefaulted_GetRef();
		Manifest.PackageName = FName(TEXT("/Game/Tests/LVI_SurfaceLoader"));
		Manifest.Kind = ELNPOctantSourcePackageKind::SourceLevel;

		FLNPSupportLayerRaster Crust;
		Crust.Layout = FLNPSupportLayout::MakeFull(2);
		Crust.SourceIndex = 0;
		Crust.Samples.SetNum(Crust.Layout.Num());
		for (int32 J = 0; J <= Crust.Layout.Subdivisions; ++J)
		{
			for (int32 I = 0; I <= Crust.Layout.Subdivisions - J; ++I)
			{
				const int32 Index = LNPSupportAtlas::GetSampleIndex(Crust.Layout.Subdivisions, I, J);
				const FVector3d Direction = LNPSupportAtlas::GetSampleDirection(Crust.Layout.Subdivisions, I, J);
				Crust.Samples[Index].Radius = Index == 0 ? ChangedRadius : 30000.0;
				Crust.Samples[Index].Normal = FVector3f(-Direction);
				Crust.Samples[Index].Flags = ELNPSupportSampleFlags::Valid | ELNPSupportSampleFlags::Walkable;
			}
		}

		FLNPSupportAtlasSource Source;
		Source.Key = TEXT("CrustActor.CrustComponent");
		Source.FaceMap.UniformLayer = 0;
		TArray<FLNPSupportAtlasSource> Sources = {MoveTemp(Source)};
		FLNPSupportCodecSettings Codec;
		Codec.BaseRadius = 30000.0;
		Codec.RadiusStep = 0.25;
		FString Error;
		if (!LNPSupportAtlas::Encode(MakeArrayView(&Crust, 1), Sources, Codec, Data->SupportPayload, Error))
		{
			return nullptr;
		}

		Data->Header.Support.ElementCount = Crust.Samples.Num();
		Data->Header.Support.UncompressedSize = Data->SupportPayload.Num();
		Data->Header.Support.ContentHash = FLNPContentHash(
			FIoHash::HashBuffer(Data->SupportPayload.GetData(), Data->SupportPayload.Num()));

		FLNPSpawnData SpawnData;
		FLNPSpawnRandomCandidate& Candidate = SpawnData.RandomCandidates.AddDefaulted_GetRef();
		Candidate.CandidateIndex = 0;
		Candidate.LocalPosition = FVector3f(0.0f, 0.0f, 30000.0f);
		Candidate.LocalNormal = FVector3f(0.0f, 0.0f, -1.0f);
		Candidate.LocalLayerId = 0;
		Candidate.Allowed = ELNPSpawnCandidateFlags::Pod | ELNPSpawnCandidateFlags::Enemy;
		Candidate.SlopeDot = 1.0f;
		Candidate.EdgeClearance = 800.0f;
		Candidate.CapsuleClearance = 800.0f;
		if (!LNPSpawnData::Encode(SpawnData, Data->SpawnPayload, Error))
		{
			return nullptr;
		}
		Data->Header.Spawn.ElementCount = 1;
		Data->Header.Spawn.UncompressedSize = Data->SpawnPayload.Num();
		Data->Header.Spawn.ContentHash = FLNPContentHash(
			FIoHash::HashBuffer(Data->SpawnPayload.GetData(), Data->SpawnPayload.Num()));
		return Data;
	}

	TArray<FLNPOctantDefinition> MakeDefinitions()
	{
		TArray<FLNPOctantDefinition> Definitions;
		Definitions.SetNum(8);
		for (FLNPOctantDefinition& Definition : Definitions)
		{
			Definition.LevelAsset = TSoftObjectPtr<UWorld>(FSoftObjectPath(TestLevelPath));
			Definition.SurfaceData = TSoftObjectPtr<ULNPOctantSurfaceData>(FSoftObjectPath(TestSurfacePath));
			Definition.SeamSignature = TEXT("LoaderTestSeam");
		}
		return Definitions;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceDataLoaderValidationTest,
	"LootNPop.SurfaceNavigation.Runtime.SurfaceDataLoaderValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPProductionSurfaceDataDefinitionsTest,
	"LootNPop.SurfaceNavigation.Runtime.ProductionSurfaceDataDefinitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSurfaceDataLoaderValidationTest::RunTest(const FString& Parameters)
{
	TArray<FLNPOctantDefinition> Definitions = MakeDefinitions();
	ULNPOctantSurfaceData* ValidData = MakeSurfaceData();
	TestNotNull(TEXT("Synthetic Support payload encodes"), ValidData);
	if (ValidData == nullptr)
	{
		return false;
	}

	TArray<ULNPOctantSurfaceData*> LoadedData;
	LoadedData.Init(ValidData, 8);
	FLNPSurfaceDataSnapshot Snapshot;
	FString Error;
	TestTrue(TEXT("Eight compatible slots validate"),
		LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, LoadedData, 17, Snapshot, Error));
	TestEqual(TEXT("Snapshot preserves generation"), Snapshot.Generation, static_cast<uint64>(17));
	TestEqual(TEXT("Snapshot contains eight slots"), Snapshot.Slots.Num(), 8);
	if (Snapshot.Slots.Num() == 8)
	{
		TestTrue(TEXT("Decoded Support is shared into every slot"), Snapshot.Slots[0].Support.IsValid());
		TestTrue(TEXT("Repeated SurfaceData decodes only once"),
			Snapshot.Slots[0].Support == Snapshot.Slots[1].Support);
		TestTrue(TEXT("Repeated Spawn payload decodes only once"),
			Snapshot.Slots[0].Spawn == Snapshot.Slots[1].Spawn);
		TestEqual(TEXT("Slot rotation 0 is identity"), Snapshot.Slots[0].SlotRotation, FQuat4d::Identity);

		for (int32 Slot = 0; Slot < Snapshot.Slots.Num(); ++Slot)
		{
			const FVector3d LocalDirection = FVector3d(1.0, 1.0, 1.0).GetSafeNormal();
			const FVector3d WorldDirection = Snapshot.Slots[Slot].SlotRotation.RotateVector(LocalDirection);
			FVector3d SurfacePoint;
			TestTrue(FString::Printf(TEXT("Slot %d inverse-rotation Layer 0 query succeeds"), Slot),
				LNPSurfaceDataLoading::QueryLayerZero(Snapshot, WorldDirection, SurfacePoint));
			TestTrue(FString::Printf(TEXT("Slot %d query stays on the 30000 cm crust"), Slot),
				FMath::IsNearlyEqual(SurfacePoint.Length(), 30000.0, 0.3));

			FLNPSurfaceQuery Query;
			Query.WorldPosition = WorldDirection * 29990.0;
			Query.MaxStepUp = 20.0;
			Query.MaxDrop = 20.0;
			FLNPSurfaceQueryResult Result;
			TestEqual(FString::Printf(TEXT("Slot %d Support query is high confidence"), Slot),
				LNPSurfaceDataLoading::QuerySupport(Snapshot, Query, Result), ELNPSurfaceQueryStatus::HighConfidence);
			TestEqual(FString::Printf(TEXT("Slot %d handle records its slot"), Slot),
				Result.Surface.OctantSlot, static_cast<uint16>(Slot));
			TestEqual(FString::Printf(TEXT("Slot %d handle records generation"), Slot),
				Result.Surface.Generation, static_cast<uint64>(17));
		}
	}

	{
		FLNPSurfaceDataSnapshot EmptySnapshot;
		FLNPSurfaceQuery Query;
		Query.WorldPosition = FVector3d(0.0, 0.0, 30000.0);
		FLNPSurfaceQueryResult Result;
		TestEqual(TEXT("Query before publication returns NotReady"),
			LNPSurfaceDataLoading::QuerySupport(EmptySnapshot, Query, Result), ELNPSurfaceQueryStatus::NotReady);
	}

	TArray<TObjectPtr<UStaticMeshComponent>> RuntimeComponents;
	TArray<TArray<FLNPRuntimeSupportSource>> RuntimeSources;
	RuntimeSources.SetNum(8);
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(
			GetTransientPackage(), *FString::Printf(TEXT("LoaderRuntimeSource_%d"), Slot));
		RuntimeComponents.Add(Component);
		FLNPRuntimeSupportSource& Source = RuntimeSources[Slot].AddDefaulted_GetRef();
		Source.Key = TEXT("CrustActor.CrustComponent");
		Source.Component = Component;
	}
	TArray<FLNPSurfaceSourceBinding> Bindings;
	TestTrue(TEXT("All eight runtime source keys bind"),
		LNPSurfaceDataLoading::BuildSourceBindings(Snapshot, RuntimeSources, Bindings, Error));
	TestEqual(TEXT("One source per slot creates eight bindings"), Bindings.Num(), 8);

	UStaticMeshComponent* DuplicateComponent = NewObject<UStaticMeshComponent>(
		GetTransientPackage(), TEXT("LoaderDuplicateRuntimeSource"));
	RuntimeComponents.Add(DuplicateComponent);
	FLNPRuntimeSupportSource& DuplicateSource = RuntimeSources[3].AddDefaulted_GetRef();
	DuplicateSource.Key = TEXT("CrustActor.CrustComponent");
	DuplicateSource.Component = DuplicateComponent;
	TestFalse(TEXT("Duplicate runtime source key is rejected"),
		LNPSurfaceDataLoading::BuildSourceBindings(Snapshot, RuntimeSources, Bindings, Error));
	TestTrue(TEXT("Duplicate source reports its cause"), Error.Contains(TEXT("duplicated")));
	RuntimeSources[3].Pop();

	RuntimeSources[5].Reset();
	TestFalse(TEXT("Missing runtime source key is rejected"),
		LNPSurfaceDataLoading::BuildSourceBindings(Snapshot, RuntimeSources, Bindings, Error));
	TestTrue(TEXT("Missing source reports its cause"), Error.Contains(TEXT("missing")));

	{
		FLNPHitIdentitySnapshot IdentitySnapshot;
		IdentitySnapshot.Generation = 9;
		IdentitySnapshot.SurfaceDataGeneration = 17;
		FLNPExactSourceEntry Entry;
		Entry.Lifetime = ELNPExactSourceLifetime::Static;
		Entry.Roles = ELNPExactSourceRole::Support | ELNPExactSourceRole::Blocker;
		Entry.Slot = 2;
		TSharedPtr<FLNPSupportAtlas, ESPMode::ThreadSafe> IdentityAtlas = MakeShared<FLNPSupportAtlas, ESPMode::ThreadSafe>();
		IdentityAtlas->Sources.AddDefaulted_GetRef().FaceMap.UniformLayer = 7;
		Entry.Support = IdentityAtlas;
		Entry.SupportSourceIndex = 0;
		IdentitySnapshot.Sources.Add(RuntimeComponents[2].Get(), Entry);
		FHitResult Hit;
		Hit.Component = RuntimeComponents[2].Get();
		Hit.FaceIndex = 123;
		const FLNPExactHitIdentity Identity = ULNPHitIdentitySubsystem::ResolveHit(IdentitySnapshot, Hit);
		TestEqual(TEXT("Exact hit resolves stored LocalLayerId"), Identity.LocalLayerId, static_cast<uint16>(7));
		TestEqual(TEXT("Exact hit carries SurfaceData generation"), Identity.SurfaceDataGeneration, static_cast<uint64>(17));
	}

	ValidData->Header.DataVersion = FLNPSurfaceBakeHeader::CurrentDataVersion - 1;
	TestFalse(TEXT("Old DataVersion is rejected"),
		LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, LoadedData, 18, Snapshot, Error));
	TestTrue(TEXT("Old DataVersion reports its cause"), Error.Contains(TEXT("DataVersion")));
	ValidData->Header.DataVersion = FLNPSurfaceBakeHeader::CurrentDataVersion;

	const FName OriginalLevelPackage = ValidData->Header.SourceManifest[0].PackageName;
	ValidData->Header.SourceManifest[0].PackageName = TEXT("/Game/Tests/WrongLevel");
	TestFalse(TEXT("Level and SurfaceData mismatch is rejected"),
		LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, LoadedData, 19, Snapshot, Error));
	TestTrue(TEXT("Level mismatch reports SourceLevel"), Error.Contains(TEXT("SourceLevel")));
	ValidData->Header.SourceManifest[0].PackageName = OriginalLevelPackage;

	ValidData->SupportPayload.Last() ^= 0x1;
	TestFalse(TEXT("Payload hash corruption is rejected before decode"),
		LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, LoadedData, 20, Snapshot, Error));
	TestTrue(TEXT("Payload corruption reports hash mismatch"), Error.Contains(TEXT("hash mismatch")));
	ValidData->SupportPayload.Last() ^= 0x1;

	ULNPOctantSurfaceData* ChangedSeamData = MakeSurfaceData(30001.0);
	TestNotNull(TEXT("Mismatched seam payload encodes"), ChangedSeamData);
	if (ChangedSeamData != nullptr)
	{
		LoadedData[7] = ChangedSeamData;
		TestFalse(TEXT("A mismatched world seam is rejected"),
			LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, LoadedData, 21, Snapshot, Error));
		TestTrue(TEXT("Seam mismatch reports the seam pair"), Error.Contains(TEXT("Seam pair")));
	}

	return !HasAnyErrors();
}

bool FLNPProductionSurfaceDataDefinitionsTest::RunTest(const FString& Parameters)
{
	const ULNPSettings* Settings = GetDefault<ULNPSettings>();
	ULNPOctantPoolData* Pool = Settings ? Settings->OctantPool.LoadSynchronous() : nullptr;
	TestNotNull(TEXT("Production OctantPool loads"), Pool);
	if (Pool == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("Production pool uses OctantDefinitions instead of legacy fallback"), !Pool->OctantDefinitions.IsEmpty());

	TArray<FLNPOctantDefinition> EffectiveDefinitions;
	Pool->BuildEffectiveDefinitions(EffectiveDefinitions);
	for (int32 Index = 0; Index < EffectiveDefinitions.Num(); ++Index)
	{
		TestFalse(FString::Printf(TEXT("Definition %d has SurfaceData"), Index),
			EffectiveDefinitions[Index].SurfaceData.IsNull());
	}

	TArray<FLNPOctantDefinition> SelectedDefinitions;
	FString Error;
	if (!TestTrue(TEXT("Production definitions select all eight slots"),
		ULNPOctantSpawnSubsystem::SelectOctantDefinitions(
			EffectiveDefinitions, 135792468, SelectedDefinitions, nullptr, &Error)))
	{
		AddError(Error);
		return false;
	}

	TArray<ULNPOctantSurfaceData*> LoadedData;
	LoadedData.Reserve(SelectedDefinitions.Num());
	for (const FLNPOctantDefinition& Definition : SelectedDefinitions)
	{
		LoadedData.Add(Definition.SurfaceData.LoadSynchronous());
	}
	FLNPSurfaceDataSnapshot Snapshot;
	if (!TestTrue(TEXT("Production SurfaceData builds a compatible 8-slot snapshot"),
		LNPSurfaceDataLoading::ValidateAndBuildSnapshot(
			SelectedDefinitions, LoadedData, 1, Snapshot, Error)))
	{
		AddError(Error);
	}

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
