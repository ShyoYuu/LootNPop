// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "DataAsset/LNPOctantSurfaceData.h"
#include "SurfaceNavigation/LNPConditionalPatchRuntime.h"
#include "SurfaceNavigation/LNPNavPathScheduler.h"
#include "SurfaceNavigation/LNPNavQuery.h"
#include "SurfaceNavigation/LNPOctantSurfaceBaker.h"
#include "SurfaceNavigation/LNPRegressionFixture.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPConditionalRegressionTest,
	"LootNPop.SurfaceNavigation.ConditionalPatch.RegressionFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPConditionalRegressionTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath Level(TEXT("/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Conditional.LVI_Octant_Fixture_Conditional"));
	ULNPOctantSurfaceData* Saved = LoadObject<ULNPOctantSurfaceData>(nullptr,
		TEXT("/Game/Maps/SurfaceNavigation/Fixtures/DA_OctantSurface_Fixture_Conditional.DA_OctantSurface_Fixture_Conditional"));
	if (!TestNotNull(TEXT("Conditional fixture data loads"), Saved)) { return false; }
	TStrongObjectPtr<ULNPOctantSurfaceData> Fresh(NewObject<ULNPOctantSurfaceData>());
	FLNPOctantBakeReport Report;
	FString Error;
	if (!FLNPOctantSurfaceBaker::Bake(Level, {}, *Fresh, Report, Error)) { AddError(Error); return false; }
	TestTrue(TEXT("Saved marker authoring and all source dependencies are current"),
		Saved->Header.SourceContentHash == Fresh->Header.SourceContentHash);
	TestTrue(TEXT("Saved base and patch streams match a fresh bake"),
		Saved->SupportPayload == Fresh->SupportPayload && Saved->NavigationPayload == Fresh->NavigationPayload
		&& Saved->TraversalPayload == Fresh->TraversalPayload && Saved->ConditionalPatchPayload == Fresh->ConditionalPatchPayload
		&& Saved->SpawnPayload == Fresh->SpawnPayload);
	TestEqual(TEXT("Bridge, floor and blocker patches are baked"), Saved->Header.ConditionalPatch.ElementCount, 3u);
	if (HasAnyErrors()) { return false; }

	TArray<FLNPOctantDefinition> Definitions;
	Definitions.SetNum(8);
	for (auto& Definition : Definitions)
	{
		Definition.LevelAsset = TSoftObjectPtr<UWorld>(Level);
		Definition.SurfaceData = Saved;
		Definition.SeamSignature = TEXT("RegressionSeam");
	}
	TArray<ULNPOctantSurfaceData*> Loaded;
	Loaded.Init(Saved, 8);
	FLNPSurfaceDataSnapshot Base, Intact, Bridged, Destroyed;
	if (!LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, Loaded, 81, Base, Error)) { AddError(Error); return false; }
	TArray<FLNPPlacementId> Active;
	FGuid BridgeId;
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		for (const FLNPConditionalPatch& Patch : Base.Slots[Slot].ConditionalPatches->Patches)
		{
			if (Patch.Condition == ELNPConditionalPatchCondition::Bridge) { BridgeId = Patch.MarkerId; }
			else { Active.Add({static_cast<int8>(Slot), Patch.MarkerId}); }
		}
	}
	if (!LNPConditionalPatchRuntime::BuildActiveSnapshot(Base, Active, Base, Intact, Error)) { AddError(Error); return false; }
	Active.Add({0, BridgeId});
	if (!LNPConditionalPatchRuntime::BuildActiveSnapshot(Base, Active, Intact, Bridged, Error)) { AddError(Error); return false; }
	Active.RemoveAll([](const FLNPPlacementId& Id) { return Id.Slot == 0; });
	if (!LNPConditionalPatchRuntime::BuildActiveSnapshot(Base, Active, Bridged, Destroyed, Error)) { AddError(Error); return false; }

	auto Support = [](const FLNPSurfaceDataSnapshot& Snapshot, int32 Slot, const FVector& LocalFeet, FLNPSurfaceQueryResult& Result)
	{
		FLNPSurfaceQuery Query;
		Query.WorldPosition = Snapshot.Slots[Slot].SlotRotation.RotateVector(LocalFeet);
		Query.MaxStepUp = Query.MaxDrop = 20.0;
		return LNPSurfaceDataLoading::QuerySupport(Snapshot, Query, Result);
	};
	auto Group = [&](const FLNPSurfaceDataSnapshot& Snapshot, int32 Slot, const FVector& Feet, FLNPNavGroupRef& Out)
	{
		FLNPSurfaceQueryResult Result;
		FLNPNavProjection Projection;
		return Support(Snapshot, Slot, Feet, Result) == ELNPSurfaceQueryStatus::HighConfidence
			&& LNPNavQuery::ProjectToNode(Snapshot, Result.Point, Result.Surface, 300.0, Projection)
			&& LNPNavQuery::GetReachabilityGroup(Snapshot, Projection.Node, Out);
	};
	const auto Bridge = LNPRegressionFixture::IslandOne();
	const auto Floor = LNPRegressionFixture::IslandTwo();
	auto Route = [&](const FLNPSurfaceDataSnapshot& Snapshot, const FVector& Start, const FVector& Goal, bool bReachable)
	{
		FLNPSurfaceQueryResult From, To;
		if (!TestTrue(TEXT("Saved fixture path endpoints have support"),
			Support(Snapshot, 0, Start, From) == ELNPSurfaceQueryStatus::HighConfidence
			&& Support(Snapshot, 0, Goal, To) == ELNPSurfaceQueryStatus::HighConfidence)) { return; }
		FLNPNavPathScheduler Scheduler;
		Scheduler.Settings.bParallel = false;
		FLNPNavPathRequest Request;
		Request.Owner = FMassEntityHandle(101, 1);
		Request.StartPosition = From.Point;
		Request.GoalPosition = To.Point;
		Request.StartSurface = From.Surface;
		Request.GoalSurface = To.Surface;
		Request.SnapRadius = 300.0;
		Request.ApproachRadius = 0.0;
		const uint32 Serial = Scheduler.Submit(Request);
		for (int32 Tick = 0; Tick < 8 && Scheduler.HasWork(); ++Tick)
		{
			Scheduler.Tick(Snapshot.Nav, nullptr, [](FMassEntityHandle) { return true; });
		}
		FLNPNavPathResult Result;
		TestTrue(TEXT("NPC path scheduler follows the saved fixture's open and closed roads"),
			Scheduler.GetResult(Request.Owner, Serial, Result)
			== (bReachable ? ELNPNavPathStatus::Succeeded : ELNPNavPathStatus::Unreachable));
		TestEqual(TEXT("Only an open fixture road returns a path"), Result.Path.IsValid(), bReachable);
	};
	for (const FLNPSurfaceDataSnapshot* Snapshot : {&Intact, &Bridged, &Destroyed})
	{
		Route(*Snapshot, Bridge.At(28000.0), Bridge.At(28000.0, 2150.0), Snapshot == &Bridged);
		Route(*Snapshot, Floor.At(28200.0), Floor.At(28200.0, 2250.0), Snapshot != &Destroyed);
	}
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		for (const FLNPSurfaceDataSnapshot* Snapshot : {&Intact, &Bridged, &Destroyed})
		{
			FLNPNavGroupRef A, B, C, D;
			if (!TestTrue(TEXT("Both bridge endpoints and both floor endpoints project to Nav"),
				Group(*Snapshot, Slot, Bridge.At(28000.0), A) && Group(*Snapshot, Slot, Bridge.At(28000.0, 2150.0), B)
				&& Group(*Snapshot, Slot, Floor.At(28200.0), C) && Group(*Snapshot, Slot, Floor.At(28200.0, 2250.0), D))) { continue; }
			const bool bBridge = Slot == 0 && Snapshot == &Bridged;
			const bool bFloor = Slot != 0 || Snapshot != &Destroyed;
			TestEqual(TEXT("Bridge opens only its slot and closes on destruction"), A.Group == B.Group, bBridge);
			TestEqual(TEXT("Floor joins distinct islands only while Intact"), C.Group == D.Group, bFloor);
			FLNPSurfaceQueryResult Result;
			TestEqual(TEXT("Bridge support follows its activation"),
				Support(*Snapshot, Slot, Bridge.At(28000.0, 1150.0), Result) == ELNPSurfaceQueryStatus::HighConfidence, bBridge);
			TestEqual(TEXT("Floor support disappears with its link"),
				Support(*Snapshot, Slot, Floor.At(28200.0, 1225.0), Result) == ELNPSurfaceQueryStatus::HighConfidence, bFloor);
		}
	}
	return !HasAnyErrors();
}
#endif
