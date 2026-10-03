// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Enemy/LNPEnemyNavigation.h"
#include "DynamicTerrain/LNPStatefulTerrain.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "SurfaceNavigation/LNPConditionalPatchRuntime.h"
#include "SurfaceNavigation/LNPNavPathScheduler.h"
#include "SurfaceNavigation/LNPNavPathSubsystem.h"
#include "SurfaceNavigation/LNPNavQuery.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

namespace
{
	bool MakePatchFixture(FLNPSurfaceDataSnapshot& Out, FString& Error)
	{
		auto Atlas = MakeShared<FLNPSupportAtlas, ESPMode::ThreadSafe>();
		FLNPSupportLayerRaster Raster;
		Raster.Layout = FLNPSupportLayout::MakeFull(32);
		Raster.Samples.SetNum(Raster.Layout.Num());
		FLNPSupportCodecSettings Codec;
		Codec.BaseRadius = 3000.0;
		for (double Radius : {3000.0, 2000.0, 2500.0})
		{
			for (int32 J = 0; J <= 32; ++J)
			{
				for (int32 I = 0; I + J <= 32; ++I)
				{
					Raster.Samples[Raster.Layout.Find(I, J)] = {Radius,
						FVector3f(-LNPSupportAtlas::GetSampleDirection(32, I, J)),
						ELNPSupportSampleFlags::Valid | ELNPSupportSampleFlags::Walkable};
				}
			}
			FLNPSupportAtlasLayer Layer;
			if (!LNPSupportAtlas::QuantizeSparseLayer(Raster, Codec, Layer, Error)) { return false; }
			Atlas->Layers.Add(MoveTemp(Layer));
		}
		auto Navigation = MakeShared<FLNPNavData, ESPMode::ThreadSafe>();
		Navigation->LocalStaticComponentCount = 2;
		for (uint16 Id = 0; Id < 2; ++Id)
		{
			FLNPNavLayer& Layer = Navigation->Layers.AddDefaulted_GetRef();
			Layer.LocalNavLayerId = Layer.LocalSupportLayerId = Id;
			Layer.Subdivisions = 8;
			FLNPNavTile& Tile = Layer.Tiles.AddDefaulted_GetRef();
			Tile.TileId = 0;
			FLNPNavCell& Cell = Tile.Cells.AddDefaulted_GetRef();
			Cell.LocalCellIndex = 34;
			Cell.Flags = ELNPNavCellFlags::Walkable;
			Cell.LocalStaticComponentId = Id;
			Cell.ClearanceClass = 2;
		}
		auto& BaseTile = Navigation->Layers[0].Tiles[0];
		BaseTile.Cells[0].EdgeMask = 1u << static_cast<uint8>(ELNPNavNeighbor::IPositive);
		FLNPNavCell Neighbor = BaseTile.Cells[0];
		Neighbor.LocalCellIndex = 35;
		Neighbor.EdgeMask = 1u << static_cast<uint8>(ELNPNavNeighbor::INegative);
		BaseTile.Cells.Add(Neighbor);
		auto Patches = MakeShared<FLNPConditionalPatchData, ESPMode::ThreadSafe>();
		FLNPConditionalPatch& Bridge = Patches->Patches.AddDefaulted_GetRef();
		Bridge.MarkerId = FGuid(1, 0, 0, 0);
		Bridge.FirstSupportLayer = 2;
		Bridge.SupportLayers.Add(Atlas->Layers.Pop());
		Bridge.Navigation.Agent = Navigation->Agent;
		Bridge.Navigation.LocalStaticComponentCount = 1;
		Bridge.Navigation.Layers.Add(Navigation->Layers[0]);
		Bridge.Navigation.Layers[0].LocalNavLayerId = Bridge.Navigation.Layers[0].LocalSupportLayerId = 2;
		Bridge.WalkEdges = {{{0, 0, 34}, {2, 0, 34}}, {{1, 0, 34}, {2, 0, 34}}};
		for (uint32 Id : {2u, 3u})
		{
			FLNPConditionalPatch& Blocker = Patches->Patches.AddDefaulted_GetRef();
			Blocker.MarkerId = FGuid(Id, 0, 0, 0);
			Blocker.FirstSupportLayer = 3;
			Blocker.BlockedNodes = {{0, 0, 34}};
		}
		FLNPConditionalPatch& EdgeBlocker = Patches->Patches.AddDefaulted_GetRef();
		EdgeBlocker.MarkerId = FGuid(4, 0, 0, 0);
		EdgeBlocker.FirstSupportLayer = 3;
		EdgeBlocker.BlockedEdges = {{{0, 0, 34}, {0, 0, 35}}};
		auto Traversal = MakeShared<FLNPNavTraversalData, ESPMode::ThreadSafe>();
		Out.Generation = 17;
		TArray<FLNPNavSlotInput> Inputs;
		for (int32 Slot = 0; Slot < 8; ++Slot)
		{
			auto& Data = Out.Slots.AddDefaulted_GetRef();
			Data.Support = Atlas;
			Data.Navigation = Navigation;
			Data.Traversal = Traversal;
			Data.ConditionalPatches = Patches;
			Data.SlotRotation = FQuat4d(ULNPOctantSpawnSubsystem::OctantRotations[Slot].Quaternion());
			Data.WorldToSlotRotation = Data.SlotRotation.Inverse();
			Inputs.Add({Atlas, Navigation, Traversal});
		}
		return LNPNavRuntime::BuildSnapshot(Inputs, MakeArrayView(ULNPOctantSpawnSubsystem::OctantRotations),
			Out.Generation, Out.Nav, Error) && LNPConditionalPatchRuntime::Prepare(Out, Error);
	}

	int32 Node(const FLNPSurfaceDataSnapshot& Snapshot, int32 Slot, uint16 Layer, uint16 Cell = 34)
	{
		FLNPNavNodeRef Ref;
		LNPNavRuntime::MakeRuntimeNodeRef(Snapshot.Nav, Slot, {Layer, 0, Cell}, Ref);
		return LNPNavGraph::ToGraphNode(Snapshot.Nav, Ref);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPConditionalPatchRuntimeTest,
	"LootNPop.SurfaceNavigation.ConditionalPatch.Runtime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPConditionalPatchRuntimeTest::RunTest(const FString& Parameters)
{
	using LNPConditionalPatchRuntime::BuildActiveSnapshot;
	FString Error;
	FLNPSurfaceDataSnapshot Base;
	if (!MakePatchFixture(Base, Error)) { AddError(Error); return false; }
	const FLNPPlacementId Bridge{0, FGuid(1, 0, 0, 0)};
	const FLNPPlacementId BlockerA{0, FGuid(2, 0, 0, 0)}, BlockerB{0, FGuid(3, 0, 0, 0)};
	const int32 A = Node(Base, 0, 0), B = Node(Base, 0, 1), C = Node(Base, 0, 2);
	TestTrue(TEXT("Inactive patch keeps a stable, blocked node address"), C != INDEX_NONE && Base.Nav.Graph.BlockedNodes[C]);
	TestTrue(TEXT("Static regions start disconnected"), LNPNavGraph::GetGroup(Base.Nav, A) != LNPNavGraph::GetGroup(Base.Nav, B));
	FLNPSurfaceQuery Query;
	Query.WorldPosition = Base.Nav.Graph.GetWorldPoint(0, C);
	Query.MaxStepUp = Query.MaxDrop = 20.0;
	Query.PreferredSurface = {0, 2, Base.Generation};
	FLNPSurfaceQueryResult Support;
	TestTrue(TEXT("Inactive floor is absent even with a preferred handle"),
		LNPSurfaceDataLoading::QuerySupport(Base, Query, Support) == ELNPSurfaceQueryStatus::NoSupport);
	FLNPSurfaceDataSnapshot Open;
	if (!BuildActiveSnapshot(Base, MakeArrayView(&Bridge, 1), Base, Open, Error)) { AddError(Error); return false; }
	TestEqual(TEXT("Opening keeps node addresses"), Node(Open, 0, 2), C);
	TestTrue(TEXT("Bridge merges regions"), LNPNavGraph::GetGroup(Open.Nav, A) == LNPNavGraph::GetGroup(Open.Nav, B));
	TestTrue(TEXT("Active floor supports the same handle"),
		LNPSurfaceDataLoading::QuerySupport(Open, Query, Support) == ELNPSurfaceQueryStatus::HighConfidence && Support.Surface.LocalLayerId == 2);
	TestTrue(TEXT("Other slot is still disconnected"), LNPNavGraph::GetGroup(Open.Nav, Node(Open, 1, 0)) != LNPNavGraph::GetGroup(Open.Nav, Node(Open, 1, 1)));
	FLNPNavNodeRef PatchRef;
	LNPNavGraph::ToNodeRef(Open.Nav, C, PatchRef);
	FVector3d Point; FVector3f Normal;
	TestTrue(TEXT("Nav Support resolves an active patch node"), LNPNavQuery::GetNodeSupport(Open, PatchRef, Point, Normal));
	FLNPNavGroupRef OldGroup;
	TestTrue(TEXT("Patch has a current reachability group"), LNPNavQuery::GetReachabilityGroup(Open, PatchRef, OldGroup));
	FLNPNavOverlay OpenOverlay;
	TestTrue(TEXT("Link changes publish Tile revisions without occupancy changes"), LNPNavOverlay::BuildPodOverlay(Open, {}, nullptr, OpenOverlay));
	FLNPNavOverlay DuplicateOverlay;
	TestFalse(TEXT("Same patch revisions do not increment the overlay"), LNPNavOverlay::BuildPodOverlay(Open, {}, &OpenOverlay, DuplicateOverlay));
	FLNPNavPathScheduler Scheduler;
	Scheduler.Settings.bParallel = false;
	FLNPNavPathRequest Request;
	Request.Owner = FMassEntityHandle(101, 1);
	Request.StartPosition = Open.Nav.Graph.GetWorldPoint(0, A);
	Request.GoalPosition = Open.Nav.Graph.GetWorldPoint(0, B);
	Request.StartSurface = {0, 0, Base.Generation};
	Request.GoalSurface = {0, 1, Base.Generation};
	Request.SnapRadius = 10.0;
	Request.ApproachRadius = 0.0;
	const uint32 Serial = Scheduler.Submit(Request);
	for (int32 Tick = 0; Tick < 8; ++Tick) { Scheduler.Tick(Open.Nav, &OpenOverlay, [](FMassEntityHandle) { return true; }); }
	FLNPNavPathResult Result;
	Scheduler.GetResult(Request.Owner, Serial, Result);
	if (!TestTrue(TEXT("A* uses the explicit Bridge Walk edges"), Result.Path.IsValid())) { return false; }
	FLNPNavPath Unrelated;
	Unrelated.SnapshotGeneration = Base.Generation;
	Unrelated.ConnectivityGraphVersion = Open.Nav.ConnectivityGraphVersion;
	Unrelated.TraversedTiles = {LNPNavGraph::GetTileKey(Open.Nav, Node(Open, 1, 0))};
	Unrelated.TileRevisions = {0};
	FLNPSurfaceDataSnapshot Closed;
	if (!BuildActiveSnapshot(Base, {}, Open, Closed, Error)) { AddError(Error); return false; }
	FLNPNavOverlay ClosedOverlay;
	TestTrue(TEXT("Closing increments only affected Tile revisions"), LNPNavOverlay::BuildPodOverlay(Closed, {}, &OpenOverlay, ClosedOverlay));
	TestFalse(TEXT("Closed floor has no Nav Support"), LNPNavQuery::GetNodeSupport(Closed, PatchRef, Point, Normal));
	TestTrue(TEXT("Closing rebuilds disconnected regions"), LNPNavGraph::GetGroup(Closed.Nav, A) != LNPNavGraph::GetGroup(Closed.Nav, B));
	TestTrue(TEXT("Previous group refs become stale"), LNPNavQuery::TestReachability(Closed.Nav, OldGroup, OldGroup) == ELNPNavReachability::Stale);
	TestFalse(TEXT("Bridge path is invalidated"), Result.Path->IsCurrent(Closed.Nav, &ClosedOverlay));
	TestTrue(TEXT("Unrelated slot path survives connectivity version changes"), Unrelated.IsCurrent(Closed.Nav, &ClosedOverlay));
	TestEqual(TEXT("Scheduler queues only the invalidated Bridge path"), Scheduler.RequeueInvalidatedPaths(Closed.Nav, &ClosedOverlay), 1);
	for (int32 Tick = 0; Tick < 8; ++Tick) { Scheduler.Tick(Closed.Nav, &ClosedOverlay, [](FMassEntityHandle) { return true; }); }
	TestTrue(TEXT("Replan detects unreachable destination"), Scheduler.GetResult(Request.Owner, Serial, Result) == ELNPNavPathStatus::Unreachable);
	FLNPSurfaceDataSnapshot Reopened;
	if (!BuildActiveSnapshot(Base, MakeArrayView(&Bridge, 1), Closed, Reopened, Error)) { AddError(Error); return false; }
	FLNPNavOverlay ReopenedOverlay;
	LNPNavOverlay::BuildPodOverlay(Reopened, {}, &ClosedOverlay, ReopenedOverlay);
	TestEqual(TEXT("Opening queues the previous negative result"), Scheduler.RequeueInvalidatedPaths(Reopened.Nav, &ReopenedOverlay), 1);
	for (int32 Tick = 0; Tick < 8; ++Tick) { Scheduler.Tick(Reopened.Nav, &ReopenedOverlay, [](FMassEntityHandle) { return true; }); }
	TestTrue(TEXT("Reopened link reaches the original goal"), Scheduler.GetResult(Request.Owner, Serial, Result) == ELNPNavPathStatus::Succeeded);
	FLNPNavPath Approach = Unrelated;
	Approach.bApproach = true;
	TestFalse(TEXT("Approach paths reassess reachability after a connection opens"), Approach.IsCurrent(Reopened.Nav, &ReopenedOverlay));
	TestTrue(TEXT("Old immutable view remains open"), !Open.Nav.Graph.BlockedNodes[C] && Open.Slots[0].ActiveSupportLayers[2]);
	FLNPSurfaceDataSnapshot Duplicate;
	TArray<FLNPPlacementId> Active = {Bridge, Bridge};
	TestTrue(TEXT("Duplicate state keeps connectivity version and revisions"), BuildActiveSnapshot(Base, Active, Open, Duplicate, Error)
		&& Duplicate.Nav.ConnectivityGraphVersion == Open.Nav.ConnectivityGraphVersion && Duplicate.PatchTileRevisions.OrderIndependentCompareEqual(Open.PatchTileRevisions));
	Active = {Bridge, BlockerA, BlockerB};
	FLNPSurfaceDataSnapshot Both, One, None;
	TestTrue(TEXT("Overlapping patch blockers compose"), BuildActiveSnapshot(Base, Active, Open, Both, Error) && Both.Nav.Graph.BlockedNodes[A]);
	Active.Remove(BlockerA);
	TestTrue(TEXT("Removing one blocker keeps the other"), BuildActiveSnapshot(Base, Active, Both, One, Error) && One.Nav.Graph.BlockedNodes[A]);
	TestEqual(TEXT("Overlapping blocker removal keeps connectivity version"), One.Nav.ConnectivityGraphVersion, Both.Nav.ConnectivityGraphVersion);
	TestTrue(TEXT("Overlapping blocker removal keeps regional revisions"), One.PatchTileRevisions.OrderIndependentCompareEqual(Both.PatchTileRevisions));
	Active.Remove(BlockerB);
	TestTrue(TEXT("Removing all blockers opens the node"), BuildActiveSnapshot(Base, Active, One, None, Error) && !None.Nav.Graph.BlockedNodes[A]);
	const uint32 Version = Duplicate.Nav.ConnectivityGraphVersion;
	const FLNPPlacementId Bad{-1, Bridge.MarkerId};
	TestFalse(TEXT("Invalid slot is rejected"), BuildActiveSnapshot(Base, MakeArrayView(&Bad, 1), Open, Duplicate, Error));
	TestEqual(TEXT("Rejected activation leaves output untouched"), Duplicate.Nav.ConnectivityGraphVersion, Version);
	TArray<FLNPEnemyHomePod> Pods;
	Pods.Add({FMassEntityHandle(201, 1), Request.GoalPosition, Request.GoalSurface,
		{LNPNavGraph::GetGroup(Open.Nav, B), Open.Nav.ConnectivityGraphVersion, Base.Generation}});
	Pods.Add({FMassEntityHandle(202, 1), Request.StartPosition, Request.StartSurface,
		{LNPNavGraph::GetGroup(Open.Nav, A), Open.Nav.ConnectivityGraphVersion, Base.Generation}});
	int32 Home;
	LNPEnemyNavigation::SelectHomePod(Open.Nav, Request.StartPosition, Request.StartSurface, Pods[0].Entity, Pods, Home);
	TestEqual(TEXT("Connected parent Pod is retained"), Home, 0);
	Pods[0].Group = {LNPNavGraph::GetGroup(Closed.Nav, B), Closed.Nav.ConnectivityGraphVersion, Base.Generation};
	Pods[1].Group = {LNPNavGraph::GetGroup(Closed.Nav, A), Closed.Nav.ConnectivityGraphVersion, Base.Generation};
	LNPEnemyNavigation::SelectHomePod(Closed.Nav, Request.StartPosition, Request.StartSurface, Pods[0].Entity, Pods, Home);
	TestEqual(TEXT("Closing reassigns Home to reachable Pod"), Home, 1);
	const FLNPPlacementId EdgeBlocker{0, FGuid(4, 0, 0, 0)};
	FLNPSurfaceDataSnapshot Cut;
	if (!BuildActiveSnapshot(Base, MakeArrayView(&EdgeBlocker, 1), Base, Cut, Error)) { AddError(Error); return false; }
	const int32 D = Node(Cut, 0, 0, 35);
	TestTrue(TEXT("Edge-only patch keeps both nodes"), !Cut.Nav.Graph.BlockedNodes[A] && !Cut.Nav.Graph.BlockedNodes[D]);
	TestFalse(TEXT("Edge-only patch blocks direct walking"), LNPNavGraph::IsDirectWalkable(Cut.Nav, A, D));
	TestTrue(TEXT("Edge deletion splits one static component into two groups"), LNPNavGraph::GetGroup(Cut.Nav, A) != LNPNavGraph::GetGroup(Cut.Nav, D));
	FLNPSurfaceDataSnapshot Restored;
	TestTrue(TEXT("Removing edge patch restores the original connection"), BuildActiveSnapshot(Base, {}, Cut, Restored, Error)
		&& LNPNavGraph::IsDirectWalkable(Restored.Nav, A, D));
	FLNPSurfaceDataSnapshot WrongGeneration = Open;
	WrongGeneration.Generation += 1;
	TestFalse(TEXT("Stale generation is rejected"), BuildActiveSnapshot(Base, {}, WrongGeneration, Duplicate, Error));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPConditionalPatchPublicationTest,
	"LootNPop.SurfaceNavigation.ConditionalPatch.Publication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPConditionalPatchPublicationTest::RunTest(const FString& Parameters)
{
	FString Error;
	FLNPSurfaceDataSnapshot Base;
	if (!MakePatchFixture(Base, Error)) { AddError(Error); return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ConditionalPatchPublicationTest"));
	ULNPSurfaceDataSubsystem* Surfaces = World->GetSubsystem<ULNPSurfaceDataSubsystem>();
	ULNPNavPathSubsystem* Paths = World->GetSubsystem<ULNPNavPathSubsystem>();
	if (!TestNotNull(TEXT("Surface subsystem exists"), Surfaces) || !TestNotNull(TEXT("Path subsystem exists"), Paths))
	{
		World->DestroyWorld(false); return false;
	}
	Surfaces->BaseSnapshot = MakeShared<FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe>(Base);
	Surfaces->PublishedSnapshot = Surfaces->BaseSnapshot;
	Surfaces->bSnapshotReady.store(true, std::memory_order_release);
	const auto Old = Surfaces->TakeSnapshot();
	const FLNPPlacementId Bridge{0, FGuid(1, 0, 0, 0)};
	const int32 C = Node(Base, 0, 2);
	TestTrue(TEXT("Server queues patch activation"), Surfaces->SetPatchActive(Bridge, true));
	TestTrue(TEXT("State change does not publish during the phase"), Surfaces->TakeSnapshot() == Old);
	// 엔진 phase 시작 delegate가 부르는 같은 경계 함수를 실행한다. 실제 PIE 틱 순서는 최종 Gate에서 검사한다.
	Paths->OnMassPhaseStart(0.016f);
	const auto Open = Surfaces->TakeSnapshot();
	TestTrue(TEXT("Boundary publishes active Support and Nav together"), Open != Old && !Open->Nav.Graph.BlockedNodes[C] && Open->Slots[0].ActiveSupportLayers[2]);
	TestTrue(TEXT("Boundary publishes matching path revision view"), Paths->TakeOverlay().IsValid()
		&& Paths->TakeOverlay()->GetTileRevision(LNPNavGraph::GetTileKey(Open->Nav, C)) > 0);
	TestTrue(TEXT("Reader holding the previous snapshot sees immutable inactive data"), Old->Nav.Graph.BlockedNodes[C] && !Old->Slots[0].ActiveSupportLayers[2]);
	Surfaces->SetPatchActive(Bridge, true);
	Paths->OnMassPhaseStart(0.016f);
	TestTrue(TEXT("Duplicate state does not replace the published snapshot"), Surfaces->TakeSnapshot() == Open);
	Surfaces->SetPatchActive(Bridge, false);
	Surfaces->SetPatchActive(Bridge, true);
	Paths->OnMassPhaseStart(0.016f);
	TestTrue(TEXT("Same-frame transitions collapse to the final state"), Surfaces->TakeSnapshot() == Open);
	Surfaces->SetPatchActive(Bridge, false);
	TestTrue(TEXT("Closing also waits for the phase boundary"), Surfaces->TakeSnapshot() == Open);
	Paths->OnMassPhaseStart(0.016f);
	TestTrue(TEXT("Closing publishes Support removal and blocked Nav"), Surfaces->TakeSnapshot()->Nav.Graph.BlockedNodes[C]
		&& !Surfaces->TakeSnapshot()->Slots[0].ActiveSupportLayers[2]);
	TestFalse(TEXT("Unknown marker is rejected"), Surfaces->SetPatchActive({0, FGuid(100, 0, 0, 0)}, true));
	World->DestroyWorld(false);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPStatefulTerrainTest,
	"LootNPop.SurfaceNavigation.ConditionalPatch.StatefulTerrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPStatefulTerrainTest::RunTest(const FString& Parameters)
{
	FString Error;
	FLNPSurfaceDataSnapshot Base;
	if (!MakePatchFixture(Base, Error)) { AddError(Error); return false; }
	// 실제 stable ordinal→face identity를 같은 slot snapshot에 연결한다.
	auto Patches = MakeShared<FLNPConditionalPatchData, ESPMode::ThreadSafe>(*Base.Slots[0].ConditionalPatches);
	Patches->Patches[0].Condition = ELNPConditionalPatchCondition::Bridge;
	FLNPConditionalPatchFaceMap Map;
	Map.FaceMap.UniformLayer = 2;
	Patches->Patches[0].FaceMaps.Add(Map);
	Base.Slots[0].ConditionalPatches = Patches;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("StatefulTerrainTest"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ULNPSurfaceDataSubsystem* Surfaces = World->GetSubsystem<ULNPSurfaceDataSubsystem>();
	ULNPNavPathSubsystem* Paths = World->GetSubsystem<ULNPNavPathSubsystem>();
	ULNPHitIdentitySubsystem* Identities = World->GetSubsystem<ULNPHitIdentitySubsystem>();
	Surfaces->BaseSnapshot = MakeShared<FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe>(Base);
	Surfaces->PublishedSnapshot = Surfaces->BaseSnapshot;
	Surfaces->bSnapshotReady.store(true, std::memory_order_release);
	ALNPPlacementMarker* Marker = World->SpawnActor<ALNPPlacementMarker>();
	Marker->PatchCondition = ELNPConditionalPatchCondition::Bridge;
	Marker->StandingTransform = FTransform(FRotator(0, 0, 90), FVector(0, 0, 200));
	Marker->TransitionSeconds = 2.f;
	Marker->PatchSources.Add({LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")), FTransform::Identity, true});
	Marker->PatchSources.Add({Marker->PatchSources[0].Mesh, FTransform(FVector(200, 0, 0)), false});
	const FLNPPlacementId Id{0, Patches->Patches[0].MarkerId};
	ALNPStatefulTerrain* Terrain = Cast<ALNPStatefulTerrain>(ULNPDynamicTerrainSubsystem::SpawnPlacedActor(
		*World, ALNPStatefulTerrain::StaticClass(), Marker->GetActorTransform(), [&](AActor& Actor)
		{ CastChecked<ALNPStatefulTerrain>(&Actor)->InitializeFromMarker(*Marker, Id); }));
	Terrain->Apply(0.0);
	Paths->OnMassPhaseStart(0.f);
	Identities->PublishRuntimeChanges();
	FHitResult Hit;
	Hit.Component = Terrain->Meshes[0];
	TestTrue(TEXT("Standing collision is active, patch remains inactive"), Terrain->Meshes[0]->IsCollisionEnabled()
		&& Surfaces->TakeSnapshot()->ActivePatches.IsEmpty());
	TestTrue(TEXT("Standing exact identity is dynamic"), Identities->ResolveHit(Hit).Lifetime == ELNPExactSourceLifetime::Dynamic);
	TestEqual(TEXT("Moving support has no stable Layer"), Identities->ResolveHit(Hit).LocalLayerId, LNPSupportLayers::NoLayer);
	Hit.Component = Terrain->Meshes[1];
	TestEqual(TEXT("Blocker source has no Support role"), Identities->ResolveHit(Hit).Roles, ELNPExactSourceRole::Blocker);
	TestTrue(TEXT("Server starts transition"), Terrain->StartTransition());
	const uint32 Revision = Terrain->GetStateRevision();
	TestFalse(TEXT("Duplicate transition is ignored"), Terrain->StartTransition());
	TestEqual(TEXT("Duplicate preserves revision"), Terrain->GetStateRevision(), Revision);
	const double Start = Terrain->State.StartServerTime;
	Terrain->Apply(Start + 1.0);
	FLNPDynamicSupportSnapshot Support;
	TestTrue(TEXT("Transition carries incidental riders"), Terrain->GetDynamicSupport(Support) && Support.bWalkable && !Support.AngularVelocity.IsNearlyZero());
	TestTrue(TEXT("Midpoint pose is deterministic"), Terrain->GetActorTransform().Equals(ALNPStatefulTerrain::EvaluatePose(Terrain->Config, Terrain->State, Start + 1.0)));
	TestTrue(TEXT("Transition does not open traversal"), Surfaces->TakeSnapshot()->ActivePatches.IsEmpty());
	ALNPStatefulTerrain* Client = World->SpawnActor<ALNPStatefulTerrain>();
	Client->SetRole(ROLE_SimulatedProxy);
	Client->State = Terrain->State;
	Client->Apply(Start + 1.0);
	TestTrue(TEXT("State arriving before config waits for geometry"), Client->Meshes.IsEmpty());
	Client->Config = Terrain->Config;
	Client->Apply(Start + 1.0);
	TestTrue(TEXT("Late received config reproduces pose without activating server patch"), Client->GetActorTransform().Equals(Terrain->GetActorTransform())
		&& Surfaces->TakeSnapshot()->ActivePatches.IsEmpty());
	// 복제 초기 데이터만으로 중간/종점 자세를 재구성한다. 실제 네트워크 late join은 단위 5 Gate다.
	const FLNPStatefulTerrainConfig ReceivedConfig = Terrain->Config;
	const FLNPStatefulTerrainState ReceivedState = Terrain->State;
	TestTrue(TEXT("Late received transition reaches exact bake pose"), ALNPStatefulTerrain::EvaluatePose(ReceivedConfig, ReceivedState, Start + 10.0).Equals(Marker->GetActorTransform()));
	Terrain->State.StartServerTime = -2.0;
	Terrain->Tick(0.f);
	TestTrue(TEXT("Server tick completes transition"), Terrain->State.Value == ELNPTerrainState::Bridge);
	TestTrue(TEXT("Bridge awaits phase publication"), Surfaces->TakeSnapshot()->ActivePatches.IsEmpty());
	Paths->OnMassPhaseStart(0.f);
	Identities->PublishRuntimeChanges();
	Hit.Component = Terrain->Meshes[0];
	const auto Identity = Identities->ResolveHit(Hit);
	TestTrue(TEXT("Bridge publishes patch and stable exact identity"), Surfaces->TakeSnapshot()->ActivePatches.Contains(Id)
		&& Identity.LocalLayerId == 2 && Identity.SurfaceDataGeneration == Base.Generation && Identity.MarkerId == Id.MarkerId);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(StatefulTerrainTest), true);
	Query.bReturnFaceIndex = true;
	Query.AddIgnoredActor(Client);
	FHitResult Exact;
	TestTrue(TEXT("Actual collision query hits stable source and resolves its patch Layer"),
		World->LineTraceSingleByChannel(Exact, FVector(0, 0, 300), FVector(0, 0, -300), ECC_GameTraceChannel2, Query)
		&& Exact.GetComponent() == Terrain->Meshes[0] && Identities->ResolveHit(Exact).LocalLayerId == 2);
	const auto Open = Surfaces->TakeSnapshot();
	Terrain->Apply(Start + 3.0);
	Paths->OnMassPhaseStart(0.f);
	TestTrue(TEXT("Repeated stable update does not republish"), Surfaces->TakeSnapshot() == Open);
	TestTrue(TEXT("Bridge can be destroyed"), Terrain->BreakTerrain());
	TestFalse(TEXT("Duplicate destruction is ignored"), Terrain->BreakTerrain());
	Terrain->Apply(Start + 3.0);
	Paths->OnMassPhaseStart(0.f);
	Identities->PublishRuntimeChanges();
	TestTrue(TEXT("Destroyed source loses collision, identity, support and path"), !Terrain->Meshes[0]->IsCollisionEnabled()
		&& !Identities->ResolveHit(Hit).IsKnown() && !Terrain->GetDynamicSupport(Support)
		&& Surfaces->TakeSnapshot()->ActivePatches.IsEmpty());
	TestFalse(TEXT("Destroyed geometry no longer blocks exact query"),
		World->LineTraceSingleByChannel(Exact, FVector(0, 0, 300), FVector(0, 0, -300), ECC_GameTraceChannel2, Query));
	Marker->PatchCondition = ELNPConditionalPatchCondition::Intact;
	ALNPStatefulTerrain* Blocker = Cast<ALNPStatefulTerrain>(ULNPDynamicTerrainSubsystem::SpawnPlacedActor(
		*World, ALNPStatefulTerrain::StaticClass(), FTransform::Identity, [&](AActor& Actor)
		{ CastChecked<ALNPStatefulTerrain>(&Actor)->InitializeFromMarker(*Marker, {0, FGuid(2, 0, 0, 0)}); }));
	Blocker->Apply(0.0);
	Paths->OnMassPhaseStart(0.f);
	TestTrue(TEXT("Intact blocker activates initial occupancy"), Surfaces->TakeSnapshot()->Nav.Graph.BlockedNodes[Node(Base, 0, 0)]);
	Blocker->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client cannot destroy terrain"), Blocker->BreakTerrain());
	Blocker->SetRole(ROLE_Authority);
	Blocker->BreakTerrain(); Blocker->Apply(0.0); Paths->OnMassPhaseStart(0.f);
	TestFalse(TEXT("Breaking intact blocker opens original route"), Surfaces->TakeSnapshot()->Nav.Graph.BlockedNodes[Node(Base, 0, 0)]);
	Client->Destroy();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return !HasAnyErrors();
}

#endif
