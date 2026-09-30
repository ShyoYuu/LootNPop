// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Config/LNPSettings.h"
#include "DataAsset/LNPOctantPoolData.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPNavBaking.h"
#include "SurfaceNavigation/LNPNavQuery.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "SurfaceNavigation/LNPSpawnData.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"

namespace
{
	constexpr TCHAR TestLevelPath[] = TEXT("/Game/Tests/LVI_SurfaceLoader.LVI_SurfaceLoader");
	constexpr TCHAR TestSurfacePath[] = TEXT("/Game/Tests/DA_SurfaceLoader.DA_SurfaceLoader");

	constexpr TCHAR RegressionLevelPath[] =
		TEXT("/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Regression.LVI_Octant_Fixture_Regression");
	constexpr TCHAR RegressionSurfacePath[] =
		TEXT("/Game/Maps/SurfaceNavigation/Fixtures/DA_OctantSurface_Fixture_Regression.DA_OctantSurface_Fixture_Regression");

	/** 합성 Support에서 blocker 없는 Nav/Traversal payload를 굽는다. RejectNode가 true인 node는 clearance 실패로 뺀다. */
	bool BakeSyntheticNav(
		ULNPOctantSurfaceData& Data,
		TFunctionRef<bool(const FVector3d& LocalDirection)> RejectNode,
		TFunctionRef<bool(const FVector3d& FromDirection, const FVector3d& ToDirection)> RejectEdge,
		FString& OutError)
	{
		FLNPSupportAtlas Atlas;
		if (!LNPSupportAtlas::Decode(Data.SupportPayload, Atlas, OutError))
		{
			return false;
		}
		FLNPNavData Navigation;
		FLNPNavTraversalData Traversal;
		FLNPNavBakeReport Report;
		if (!LNPNavBaking::Build(
			Atlas,
			FLNPNavBakeSettings(),
			[&RejectNode](uint16, const FVector3d& Position, const FVector3f&)
			{
				return !RejectNode(Position.GetSafeNormal());
			},
			[&RejectEdge](uint16, const FVector3d& From, const FVector3f&, uint16, const FVector3d& To, const FVector3f&)
			{
				return !RejectEdge(From.GetSafeNormal(), To.GetSafeNormal());
			},
			Navigation, Traversal, Report, OutError)
			|| !LNPNavData::EncodeNavigation(Navigation, Data.NavigationPayload, OutError)
			|| !LNPNavData::EncodeTraversal(Traversal, Navigation, Data.TraversalPayload, OutError))
		{
			return false;
		}
		Data.Header.Navigation.ElementCount = Report.CellCount;
		Data.Header.Navigation.UncompressedSize = Data.NavigationPayload.Num();
		Data.Header.Navigation.ContentHash = FLNPContentHash(
			FIoHash::HashBuffer(Data.NavigationPayload.GetData(), Data.NavigationPayload.Num()));
		Data.Header.Traversal.ElementCount = Traversal.Portals.Num() + Traversal.SeamEndpoints.Num();
		Data.Header.Traversal.UncompressedSize = Data.TraversalPayload.Num();
		Data.Header.Traversal.ContentHash = FLNPContentHash(
			FIoHash::HashBuffer(Data.TraversalPayload.GetData(), Data.TraversalPayload.Num()));
		return true;
	}

	ULNPOctantSurfaceData* MakeSurfaceData(
		const double ChangedRadius = 30000.0,
		TFunctionRef<bool(const FVector3d& LocalDirection)> RejectNavNode = [](const FVector3d&) { return false; },
		TFunctionRef<bool(const FVector3d& FromDirection, const FVector3d& ToDirection)> RejectNavEdge =
			[](const FVector3d&, const FVector3d&) { return false; })
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
		return BakeSyntheticNav(*Data, RejectNavNode, RejectNavEdge, Error) ? Data : nullptr;
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
	FLNPSurfaceDataNavAssemblyTest,
	"LootNPop.SurfaceNavigation.Runtime.NavAssembly",
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

bool FLNPSurfaceDataNavAssemblyTest::RunTest(const FString& Parameters)
{
	TArray<FLNPOctantDefinition> Definitions = MakeDefinitions();
	ULNPOctantSurfaceData* ValidData = MakeSurfaceData();
	if (!TestNotNull(TEXT("Synthetic SurfaceData with Nav encodes"), ValidData))
	{
		return false;
	}
	TArray<ULNPOctantSurfaceData*> LoadedData;
	LoadedData.Init(ValidData, 8);
	FLNPSurfaceDataSnapshot Snapshot;
	FString Error;
	if (!TestTrue(TEXT("Eight slots with Nav validate"),
		LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, LoadedData, 17, Snapshot, Error)))
	{
		AddError(Error);
		return false;
	}

	const FLNPNavSnapshot& Nav = Snapshot.Nav;
	TestEqual(TEXT("Nav view carries the Surface generation"), Nav.SnapshotGeneration, static_cast<uint64>(17));
	TestTrue(TEXT("Decoded Navigation is shared by slots using one asset"),
		Snapshot.Slots[0].Navigation.IsValid() && Snapshot.Slots[0].Navigation == Snapshot.Slots[7].Navigation);
	TestTrue(TEXT("Decoded Traversal is shared by slots using one asset"),
		Snapshot.Slots[0].Traversal.IsValid() && Snapshot.Slots[0].Traversal == Snapshot.Slots[7].Traversal);
	for (int32 Slot = 0; Slot <= 8; ++Slot)
	{
		TestEqual(FString::Printf(TEXT("Slot %d runtime Nav Layer base"), Slot),
			static_cast<int32>(Nav.SlotLayerBase[Slot]), Slot);
	}
	const int32 N = Snapshot.Slots[0].Navigation->Layers[0].Subdivisions;
	AddInfo(FString::Printf(TEXT("Synthetic crust Nav subdivisions=%d"), N));
	TestEqual(TEXT("Every crust seam sample links across all 12 world seams"), Nav.SeamLinks.Num(), 12 * (N + 1));
	TestEqual(TEXT("Eight seamless crusts merge into one runtime StaticNavComponent"),
		Nav.RuntimeStaticComponentCount, 1u);
	TestTrue(TEXT("Seam radius mismatch stays within tolerance"),
		Nav.MaxSeamRadiusDelta <= LNPNavRuntime::MaxSeamRadiusDelta);
	TestTrue(TEXT("Symmetric synthetic seams have no blocked seam nodes or edges"),
		Nav.BlockedSeamNodes.IsEmpty() && Nav.BlockedSeamEdges.IsEmpty());

	const FLNPNavTile& FirstTile = Snapshot.Slots[5].Navigation->Layers[0].Tiles[0];
	FLNPLocalNavNodeRef Local;
	Local.LocalNavLayerId = 0;
	Local.TileId = FirstTile.TileId;
	Local.LocalCellIndex = FirstTile.Cells[0].LocalCellIndex;
	FLNPNavNodeRef Runtime;
	TestTrue(TEXT("Local node gets a slot-specific runtime ref"), LNPNavRuntime::MakeRuntimeNodeRef(Nav, 5, Local, Runtime));
	TestEqual(TEXT("Runtime ref uses slot 5 Layer ID"), Runtime.RuntimeNavLayerId, static_cast<uint16>(5));
	int32 ResolvedSlot = INDEX_NONE;
	FLNPLocalNavNodeRef ResolvedLocal;
	TestTrue(TEXT("Runtime ref resolves back"),
		LNPNavRuntime::ResolveRuntimeNodeRef(Nav, Runtime, ResolvedSlot, ResolvedLocal));
	TestEqual(TEXT("Runtime ref resolves to slot 5"), ResolvedSlot, 5);
	TestTrue(TEXT("Runtime ref resolves to the same local node"), ResolvedLocal == Local);
	FLNPNavNodeRef StaleRef = Runtime;
	StaleRef.SnapshotGeneration = 16;
	TestFalse(TEXT("Stale generation ref is rejected"),
		LNPNavRuntime::ResolveRuntimeNodeRef(Nav, StaleRef, ResolvedSlot, ResolvedLocal));

	// 초기 ReachabilityGroup은 StaticNavComponent와 1:1이고 version 1로 게시된다.
	TestEqual(TEXT("Initial ConnectivityGraphVersion is 1"), Nav.ConnectivityGraphVersion, 1u);
	TestEqual(TEXT("Initial ReachabilityGroups match StaticNavComponents"),
		Nav.ReachabilityGroupCount, Nav.RuntimeStaticComponentCount);
	const FVector3d CenterDirection = FVector3d(1.0, 1.0, 1.0).GetSafeNormal();
	FLNPNavGroupRef Groups[2];
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const int32 Slot = Index == 0 ? 0 : 5;
		const FVector3d Position = Snapshot.Slots[Slot].SlotRotation.RotateVector(CenterDirection * 29990.0);
		FLNPNavProjection Projection;
		TestTrue(FString::Printf(TEXT("Slot %d octant center projects to a crust node"), Slot),
			LNPNavQuery::ProjectToNode(Snapshot, Position, {static_cast<uint16>(Slot), 0, Snapshot.Generation},
				LNPNavQuery::DefaultProjectionRadius, Projection)
			&& Projection.Distance < 200.0
			&& LNPNavQuery::GetReachabilityGroup(Snapshot, Projection.Node, Groups[Index]));
		FVector3d NodePoint;
		FVector3f NodeNormal;
		TestTrue(FString::Printf(TEXT("Slot %d projected node returns its Support point"), Slot),
			LNPNavQuery::GetNodeSupport(Snapshot, Projection.Node, NodePoint, NodeNormal)
			&& NodePoint.Equals(Projection.Point, 1.e-6)
			&& FMath::IsNearlyEqual(NodePoint.Length(), 30000.0, 0.5)
			&& FVector3d::DotProduct(FVector3d(NodeNormal), -NodePoint.GetSafeNormal()) > 0.999);
	}
	TestTrue(TEXT("Seam-merged crusts are reachable across slots"),
		LNPNavQuery::TestReachability(Nav, Groups[0], Groups[1]) == ELNPNavReachability::Reachable);
	FLNPNavGroupRef OldVersionGroup = Groups[1];
	OldVersionGroup.ConnectivityGraphVersion = Nav.ConnectivityGraphVersion + 1;
	TestTrue(TEXT("A group from another ConnectivityGraphVersion is stale"),
		LNPNavQuery::TestReachability(Nav, Groups[0], OldVersionGroup) == ELNPNavReachability::Stale);
	FLNPNavGroupRef OldGenerationGroup = Groups[1];
	OldGenerationGroup.SnapshotGeneration = 16;
	TestTrue(TEXT("A group from another snapshot generation is stale"),
		LNPNavQuery::TestReachability(Nav, OldGenerationGroup, Groups[0]) == ELNPNavReachability::Stale);
	FLNPNavGroupRef StaleNodeGroup;
	TestFalse(TEXT("Stale node ref has no ReachabilityGroup"),
		LNPNavQuery::GetReachabilityGroup(Snapshot, StaleRef, StaleNodeGroup));
	FLNPNavProjection StaleHandleProjection;
	TestFalse(TEXT("Stale Surface handle does not project"),
		LNPNavQuery::ProjectToNode(Snapshot, CenterDirection * 29990.0, {0, 0, 16},
			LNPNavQuery::DefaultProjectionRadius, StaleHandleProjection));
	TestFalse(TEXT("A missing Nav Layer does not project"),
		LNPNavQuery::ProjectToNode(Snapshot, CenterDirection * 29990.0, {0, 1, Snapshot.Generation},
			LNPNavQuery::DefaultProjectionRadius, StaleHandleProjection));
	TestFalse(TEXT("Projection does not snap beyond its radius"),
		LNPNavQuery::ProjectToNode(Snapshot, CenterDirection * 29000.0, {0, 0, Snapshot.Generation},
			LNPNavQuery::DefaultProjectionRadius, StaleHandleProjection));

	ValidData->NavigationPayload.Last() ^= 0x1;
	TestFalse(TEXT("Navigation payload corruption rejects the whole snapshot"),
		LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, LoadedData, 18, Snapshot, Error));
	TestTrue(TEXT("Navigation corruption reports hash mismatch"), Error.Contains(TEXT("Navigation payload hash mismatch")));
	TestTrue(TEXT("Rejected Nav leaves no partial Support snapshot"),
		Snapshot.Generation == 0 && Snapshot.Slots.IsEmpty() && !Snapshot.Nav.IsValid());
	ValidData->NavigationPayload.Last() ^= 0x1;

	const TArray<uint8> TraversalPayload = ValidData->TraversalPayload;
	const FLNPSurfacePayloadDescriptor TraversalDescriptor = ValidData->Header.Traversal;
	ValidData->TraversalPayload.Reset();
	ValidData->Header.Traversal = FLNPSurfacePayloadDescriptor();
	TestFalse(TEXT("Missing Traversal payload is rejected"),
		LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, LoadedData, 19, Snapshot, Error));
	TestTrue(TEXT("Missing Traversal reports its cause"), Error.Contains(TEXT("Traversal payload is empty")));
	ValidData->TraversalPayload = TraversalPayload;
	ValidData->Header.Traversal = TraversalDescriptor;

	// slot 7 사본만 x=0 이음매 중점 node를 잃는다. 다른 사본은 막힘으로 기록되고 그 step만 연결되지 않는다(D-060).
	const FVector3d SeamMidpoint = FVector3d(0.0, 1.0, 1.0).GetSafeNormal();
	ULNPOctantSurfaceData* OneSidedData = MakeSurfaceData(30000.0, [&SeamMidpoint](const FVector3d& Direction)
	{
		return FVector3d::Dist(Direction, SeamMidpoint) < 0.004;
	});
	if (TestNotNull(TEXT("One-sided seam SurfaceData encodes"), OneSidedData))
	{
		LoadedData[7] = OneSidedData;
		if (TestTrue(TEXT("A one-sided Nav seam node still publishes"),
			LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, LoadedData, 20, Snapshot, Error)))
		{
			TestEqual(TEXT("The surviving copy is recorded as a blocked seam node"), Snapshot.Nav.BlockedSeamNodes.Num(), 1);
			TestEqual(TEXT("The blocked seam step is not linked"), Snapshot.Nav.SeamLinks.Num(), 12 * (N + 1) - 1);
			int32 BlockedSlot = INDEX_NONE;
			FLNPLocalNavNodeRef BlockedLocal;
			TestTrue(TEXT("Blocked seam node is the copy outside slot 7"),
				Snapshot.Nav.BlockedSeamNodes.Num() == 1
				&& LNPNavRuntime::ResolveRuntimeNodeRef(Snapshot.Nav, Snapshot.Nav.BlockedSeamNodes[0], BlockedSlot, BlockedLocal)
				&& BlockedSlot != 7);
			if (Snapshot.Nav.BlockedSeamNodes.Num() == 1 && BlockedSlot != INDEX_NONE)
			{
				const FLNPNavNodeRef& Blocked = Snapshot.Nav.BlockedSeamNodes[0];
				FVector3d BlockedPoint;
				FVector3f BlockedNormal;
				FLNPNavProjection Projection;
				uint32 BlockedComponent = MAX_uint32;
				TestTrue(TEXT("Blocked seam node is recognized"), LNPNavRuntime::IsBlockedSeamNode(Snapshot.Nav, Blocked));
				TestFalse(TEXT("Blocked seam node has no StaticNavComponent"),
					LNPNavQuery::GetStaticComponent(Snapshot, Blocked, BlockedComponent));
				TestTrue(TEXT("Projection onto a blocked seam node picks another node"),
					LNPNavQuery::GetNodeSupport(Snapshot, Blocked, BlockedPoint, BlockedNormal)
					&& LNPNavQuery::ProjectToNode(Snapshot, BlockedPoint,
						{static_cast<uint16>(BlockedSlot), 0, Snapshot.Generation},
						LNPNavQuery::DefaultProjectionRadius, Projection)
					&& LNPNavRuntime::MakeNodeKey(Projection.Node) != LNPNavRuntime::MakeNodeKey(Blocked)
					&& Projection.Distance > 1.0);
			}
		}
		else
		{
			AddError(Error);
		}
		LoadedData[7] = ValidData;
	}

	// slot 7 사본만 x=0 이음매 중점 양옆 seam 방향 edge 두 개를 닫는다. 열린 사본 두 개가 막힘으로 기록된다.
	ULNPOctantSurfaceData* AsymmetricEdgeData = MakeSurfaceData(
		30000.0,
		[](const FVector3d&) { return false; },
		[&SeamMidpoint](const FVector3d& From, const FVector3d& To)
		{
			return FMath::Abs(From.X) < 1.e-9 && FMath::Abs(To.X) < 1.e-9
				&& FVector3d::Dist(From, SeamMidpoint) < 0.008 && FVector3d::Dist(To, SeamMidpoint) < 0.008;
		});
	if (TestNotNull(TEXT("Asymmetric seam edge SurfaceData encodes"), AsymmetricEdgeData))
	{
		LoadedData[7] = AsymmetricEdgeData;
		if (TestTrue(TEXT("Asymmetric seam edges still publish"),
			LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, LoadedData, 21, Snapshot, Error)))
		{
			TestEqual(TEXT("Both open copies are recorded as blocked seam edges"), Snapshot.Nav.BlockedSeamEdges.Num(), 2);
			TestTrue(TEXT("Asymmetric edges keep every seam node linked"),
				Snapshot.Nav.BlockedSeamNodes.IsEmpty() && Snapshot.Nav.SeamLinks.Num() == 12 * (N + 1));
		}
		else
		{
			AddError(Error);
		}
		LoadedData[7] = ValidData;
	}

	// 지면 반지름이 다른 seam은 여전히 전체 실패다.
	ULNPOctantSurfaceData* ChangedSeamData = MakeSurfaceData(30001.0);
	if (TestNotNull(TEXT("Mismatched seam payload encodes"), ChangedSeamData))
	{
		LoadedData[7] = ChangedSeamData;
		TestFalse(TEXT("A geometric seam mismatch rejects the whole snapshot"),
			LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, LoadedData, 22, Snapshot, Error));
		TestTrue(TEXT("Geometric seam mismatch leaves no partial snapshot"), Snapshot.Slots.IsEmpty());
		LoadedData[7] = ValidData;
	}

	// 정적 회귀 LVI 8-slot: 지각은 seam으로, 동굴은 portal로 하나의 runtime component가 된다.
	ULNPOctantSurfaceData* Regression = TSoftObjectPtr<ULNPOctantSurfaceData>(
		FSoftObjectPath(RegressionSurfacePath)).LoadSynchronous();
	if (TestNotNull(TEXT("Regression SurfaceData loads"), Regression))
	{
		TArray<FLNPOctantDefinition> RegressionDefinitions = MakeDefinitions();
		for (FLNPOctantDefinition& Definition : RegressionDefinitions)
		{
			Definition.LevelAsset = TSoftObjectPtr<UWorld>(FSoftObjectPath(RegressionLevelPath));
			Definition.SurfaceData = TSoftObjectPtr<ULNPOctantSurfaceData>(FSoftObjectPath(RegressionSurfacePath));
		}
		LoadedData.Init(Regression, 8);
		if (TestTrue(TEXT("Regression 8-slot Nav assembles"),
			LNPSurfaceDataLoading::ValidateAndBuildSnapshot(RegressionDefinitions, LoadedData, 3, Snapshot, Error)))
		{
			const FLNPNavTraversalData& Traversal = *Snapshot.Slots[0].Traversal;
			const FLNPNavData& Navigation = *Snapshot.Slots[0].Navigation;
			const FLNPNavCell* CrustCell = LNPNavData::ResolveLocalNode(Navigation, Traversal.SeamEndpoints[0].Node);
			TestNotNull(TEXT("Regression crust seam endpoint resolves"), CrustCell);
			const uint32 CrustComponent = CrustCell
				? LNPNavRuntime::GetRuntimeStaticComponent(Snapshot.Nav, 0, CrustCell->LocalStaticComponentId)
				: MAX_uint32;
			for (const FLNPNavSeamLink& Link : Snapshot.Nav.SeamLinks)
			{
				int32 Slot = INDEX_NONE;
				FLNPLocalNavNodeRef LinkLocal;
				const FLNPNavCell* Cell = LNPNavRuntime::ResolveRuntimeNodeRef(Snapshot.Nav, Link.A, Slot, LinkLocal)
					? LNPNavData::ResolveLocalNode(*Snapshot.Slots[Slot].Navigation, LinkLocal)
					: nullptr;
				if (Cell == nullptr
					|| LNPNavRuntime::GetRuntimeStaticComponent(Snapshot.Nav, Slot, Cell->LocalStaticComponentId) != CrustComponent)
				{
					AddError(TEXT("A regression seam link is not in the merged crust component."));
					break;
				}
			}
			for (int32 Slot = 0; Slot < 8; ++Slot)
			{
				for (const FLNPNavPortal& Portal : Traversal.Portals)
				{
					const FLNPNavCell* A = LNPNavData::ResolveLocalNode(Navigation, Portal.A);
					const FLNPNavCell* B = LNPNavData::ResolveLocalNode(Navigation, Portal.B);
					TestTrue(FString::Printf(TEXT("Slot %d portal joins the crust component"), Slot),
						A && B
						&& LNPNavRuntime::GetRuntimeStaticComponent(Snapshot.Nav, Slot, A->LocalStaticComponentId) == CrustComponent
						&& LNPNavRuntime::GetRuntimeStaticComponent(Snapshot.Nav, Slot, B->LocalStaticComponentId) == CrustComponent);
				}
			}
			AddInfo(FString::Printf(
				TEXT("Regression Nav: runtimeLayers=%u localComponents=%u runtimeComponents=%u seamLinks=%d blockedSeamNodes=%d blockedSeamEdges=%d portals/slot=%d maxSeamRadiusDelta=%.3f minSeamNormalDot=%.5f"),
				Snapshot.Nav.SlotLayerBase.Last(), Snapshot.Nav.SlotComponentBase.Last(),
				Snapshot.Nav.RuntimeStaticComponentCount, Snapshot.Nav.SeamLinks.Num(),
				Snapshot.Nav.BlockedSeamNodes.Num(), Snapshot.Nav.BlockedSeamEdges.Num(), Traversal.Portals.Num(),
				Snapshot.Nav.MaxSeamRadiusDelta, Snapshot.Nav.MinSeamNormalDot));
		}
		else
		{
			AddError(Error);
		}
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
	else
	{
		TestTrue(TEXT("Production Nav view is published with the Surface generation"),
			Snapshot.Nav.SnapshotGeneration == 1 && Snapshot.Nav.RuntimeStaticComponentCount > 0
			&& !Snapshot.Nav.SeamLinks.IsEmpty());
		AddInfo(FString::Printf(
			TEXT("Production Nav: runtimeLayers=%u localComponents=%u runtimeComponents=%u seamLinks=%d blockedSeamNodes=%d blockedSeamEdges=%d maxSeamRadiusDelta=%.3f minSeamNormalDot=%.5f"),
			Snapshot.Nav.SlotLayerBase.Last(), Snapshot.Nav.SlotComponentBase.Last(),
			Snapshot.Nav.RuntimeStaticComponentCount, Snapshot.Nav.SeamLinks.Num(),
			Snapshot.Nav.BlockedSeamNodes.Num(), Snapshot.Nav.BlockedSeamEdges.Num(),
			Snapshot.Nav.MaxSeamRadiusDelta, Snapshot.Nav.MinSeamNormalDot));
	}

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
