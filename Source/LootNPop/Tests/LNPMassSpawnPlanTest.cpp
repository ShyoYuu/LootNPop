// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurfaceNavigation/LNPMassSpawnPlan.h"
#include "Enemy/LNPEnemyConfig.h"

namespace
{
	FLNPSpawnRandomCandidate MakeCandidate(
		const uint32 Index,
		const FVector3f Position,
		const uint16 Layer,
		const ELNPSpawnCandidateFlags Allowed)
	{
		FLNPSpawnRandomCandidate Candidate;
		Candidate.CandidateIndex = Index;
		Candidate.LocalPosition = Position;
		Candidate.LocalNormal = FVector3f(0.0f, 0.0f, 1.0f);
		Candidate.LocalLayerId = Layer;
		Candidate.Allowed = Allowed;
		Candidate.SlopeDot = 1.0f;
		Candidate.EdgeClearance = 500.0f;
		Candidate.CapsuleClearance = 500.0f;
		return Candidate;
	}

	TSharedPtr<FLNPSpawnData, ESPMode::ThreadSafe> MakeEmptySpawn()
	{
		return MakeShared<FLNPSpawnData, ESPMode::ThreadSafe>();
	}

	FLNPSurfaceDataSnapshot MakeSnapshot()
	{
		FLNPSurfaceDataSnapshot Snapshot;
		Snapshot.Generation = 17;
		Snapshot.Slots.SetNum(8);
		for (FLNPSurfaceDataSlotSnapshot& Slot : Snapshot.Slots)
		{
			Slot.SlotRotation = FQuat4d::Identity;
			Slot.WorldToSlotRotation = FQuat4d::Identity;
			Slot.Spawn = MakeEmptySpawn();
		}

		TSharedPtr<FLNPSpawnData, ESPMode::ThreadSafe> SlotZero = MakeEmptySpawn();
		FLNPSpawnAuthoredAnchor& Specific = SlotZero->AuthoredAnchors.AddDefaulted_GetRef();
		Specific.SpawnPointId = FGuid(1, 0, 0, 0);
		Specific.TargetSpawnSetId = TEXT("Ground");
		Specific.LocalTransform = FTransform3f(FQuat4f::Identity, FVector3f(1000.0f, 0.0f, 0.0f));
		Specific.LocalLayerId = 2;

		FLNPSpawnAuthoredAnchor& Generic = SlotZero->AuthoredAnchors.AddDefaulted_GetRef();
		Generic.SpawnPointId = FGuid(2, 0, 0, 0);
		Generic.LocalTransform = FTransform3f(FQuat4f::Identity, FVector3f(0.0f, 2000.0f, 0.0f));
		Generic.LocalLayerId = 0;

		SlotZero->RandomCandidates.Add(MakeCandidate(10, FVector3f(0.0f, 0.0f, 3000.0f), 1, ELNPSpawnCandidateFlags::Pod));
		SlotZero->RandomCandidates.Add(MakeCandidate(11, FVector3f(1000.0f, 300.0f, 0.0f), 2, ELNPSpawnCandidateFlags::Enemy));
		SlotZero->RandomCandidates.Add(MakeCandidate(12, FVector3f(0.0f, 2300.0f, 0.0f), 0, ELNPSpawnCandidateFlags::Enemy));
		SlotZero->RandomCandidates.Add(MakeCandidate(13, FVector3f(300.0f, 0.0f, 3000.0f), 1, ELNPSpawnCandidateFlags::Enemy));
		Snapshot.Slots[0].Spawn = SlotZero;

		// 위치와 LocalLayerId가 같아도 slot이 다르면 연관 적 후보가 아니다.
		TSharedPtr<FLNPSpawnData, ESPMode::ThreadSafe> SlotOne = MakeEmptySpawn();
		SlotOne->RandomCandidates.Add(MakeCandidate(20, FVector3f(1000.0f, 250.0f, 0.0f), 2, ELNPSpawnCandidateFlags::Enemy));
		Snapshot.Slots[1].Spawn = SlotOne;
		return Snapshot;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPMassSpawnPlanTest,
	"LootNPop.SurfaceNavigation.Runtime.MassSpawnPlanning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPMassSpawnPlanTest::RunTest(const FString& Parameters)
{
	const FLNPSurfaceDataSnapshot Snapshot = MakeSnapshot();
	FLNPMassSpawnSetPlanInput Set;
	Set.SpawnSetId = TEXT("Ground");
	Set.RequestedPods = 3;
	Set.PodAssetIndex = 4;
	Set.Enemies.Add({1, 5});

	FLNPMassSpawnPlan First;
	FLNPMassSpawnPlan Second;
	FString Error;
	if (!TestTrue(TEXT("World Spawn snapshot builds a plan"),
		LNPMassSpawnPlanning::BuildPlan(Snapshot, MakeArrayView(&Set, 1), 1234, 500.0f, 500.0f, First, Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("Same seed builds the same plan"),
		LNPMassSpawnPlanning::BuildPlan(Snapshot, MakeArrayView(&Set, 1), 1234, 500.0f, 500.0f, Second, Error));

	TestEqual(TEXT("Config count remains the Pod total"), First.Pods.Num(), 3);
	TestEqual(TEXT("Specific authored anchor is first"), First.Pods[0].PlacementSource, ELNPPodPlacementSource::AuthoredForSet);
	TestEqual(TEXT("Generic authored anchor is second"), First.Pods[1].PlacementSource, ELNPPodPlacementSource::AuthoredGeneric);
	TestEqual(TEXT("Random candidate fills only the remainder"), First.Pods[2].PlacementSource, ELNPPodPlacementSource::RandomCandidate);
	TestEqual(TEXT("All pods receive one associated enemy"),
		First.Pods[0].Enemies[0].Transforms.Num() + First.Pods[1].Enemies[0].Transforms.Num() + First.Pods[2].Enemies[0].Transforms.Num(), 3);
	for (const FLNPMassSpawnPlannedPod& Pod : First.Pods)
	{
		TestEqual(TEXT("Surface generation is initialized"), Pod.Surface.Generation, Snapshot.Generation);
		TestEqual(TEXT("Associated enemy group uses the requested asset"), Pod.Enemies[0].AssetIndex, 5);
	}

	const FLNPMassSpawnSetStats& Stats = First.SetStats[0];
	TestEqual(TEXT("Requested stat"), Stats.Requested, 3);
	TestEqual(TEXT("Specific authored stat"), Stats.Authored, 1);
	TestEqual(TEXT("Generic authored stat"), Stats.Generic, 1);
	TestEqual(TEXT("Random stat"), Stats.Random, 1);
	TestEqual(TEXT("Pod shortfall stat"), Stats.Shortfall, 0);
	TestEqual(TEXT("Enemy requested stat"), Stats.EnemyRequested, 3);
	TestEqual(TEXT("Enemy placed stat"), Stats.EnemyPlaced, 3);

	TestEqual(TEXT("Deterministic pod count"), Second.Pods.Num(), First.Pods.Num());
	for (int32 Index = 0; Index < First.Pods.Num(); ++Index)
	{
		TestTrue(TEXT("Deterministic pod location"),
			First.Pods[Index].Transform.GetLocation().Equals(Second.Pods[Index].Transform.GetLocation()));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPMassSpawnHeadroomTest,
	"LootNPop.SurfaceNavigation.Runtime.MassSpawnHeadroom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPMassSpawnHeadroomTest::RunTest(const FString& Parameters)
{
	FLNPSurfaceDataSnapshot Snapshot = MakeSnapshot();
	auto Spawn = MakeShared<FLNPSpawnData, ESPMode::ThreadSafe>(*Snapshot.Slots[0].Spawn);
	Spawn->AuthoredAnchors[0].Headroom = 1999.0f;
	Spawn->AuthoredAnchors[1].Headroom = 2000.0f;
	Spawn->RandomCandidates[0].Headroom = 2001.0f;
	Spawn->RandomCandidates[2].Headroom = 1999.0f;
	FLNPSpawnRandomCandidate Flyer = MakeCandidate(14, FVector3f(300.0f, 2000.0f, 0.0f), 0, ELNPSpawnCandidateFlags::Enemy);
	Flyer.Headroom = 2001.0f;
	Spawn->RandomCandidates.Add(Flyer);
	Snapshot.Slots[0].Spawn = Spawn;
	FLNPMassSpawnSetPlanInput Set;
	Set.SpawnSetId = TEXT("Ground");
	Set.RequestedPods = 3;
	Set.RequiredHeadroom = 2000.0f;
	FLNPMassSpawnPlan Plan;
	FString Error;
	auto Build = [&]()
	{
		return LNPMassSpawnPlanning::BuildPlan(Snapshot, MakeArrayView(&Set, 1), 1234, 500.0f, 500.0f, Plan, Error);
	};
	if (!TestTrue(TEXT("Headroom-filtered plan builds"), Build())) { AddError(Error); return false; }
	TestEqual(TEXT("Below-threshold specific anchor is skipped"), Plan.SetStats[0].Authored, 0);
	TestEqual(TEXT("Equal-threshold generic anchor survives"), Plan.SetStats[0].Generic, 1);
	TestEqual(TEXT("Above-threshold random candidate survives"), Plan.SetStats[0].Random, 1);
	TestEqual(TEXT("Filtered candidates report shortfall"), Plan.SetStats[0].Shortfall, 1);
	TestEqual(TEXT("Skipped authored anchor remains unused"), Plan.SetStats[0].UnusedAuthored, 1);
	Set.RequiredHeadroom = 1999.0f;
	TestTrue(TEXT("Lower requirement reuses the same snapshot"), Build());
	TestEqual(TEXT("Specific authored priority returns"), Plan.Pods[0].PlacementSource, ELNPPodPlacementSource::AuthoredForSet);
	TestEqual(TEXT("All three placements return without rebake"), Plan.Pods.Num(), 3);
	Set.RequiredHeadroom = 2002.0f;
	TestTrue(TEXT("Insufficient headroom remains a valid plan"), Build());
	TestEqual(TEXT("All placement paths apply the headroom filter"), Plan.SetStats[0].Shortfall, 3);
	Set.RequiredHeadroom = 2000.0f;
	Set.RequestedPods = 1;
	Set.Enemies = {{1, 6, 2000.0f}, {1, 5, 0.0f}};
	TestTrue(TEXT("Mixed ground and flying group builds"), Build());
	if (TestEqual(TEXT("Both enemy groups are placed"), Plan.Pods[0].Enemies.Num(), 2))
	{
		TestTrue(TEXT("Flying spawn uses the high-headroom enemy point"),
			Plan.Pods[0].Enemies[0].Transforms[0].GetLocation().Equals(FVector(Flyer.LocalPosition)));
	}
	ULNPEnemyConfig* Config = NewObject<ULNPEnemyConfig>();
	TestEqual(TEXT("Ground config requires no headroom"), Config->GetSpawnRequiredHeadroom(), 0.0f);
	Config->NavigationDomain = ELNPNavigationDomain::FreeFlight;
	const float Required = Config->GetSpawnRequiredHeadroom();
	TestEqual(TEXT("Flying requirement includes body sphere, clearance and margin"), Required, 2638.0f);
	Config->FlightConfig.IdleAltitudeMax += 500.0f;
	TestEqual(TEXT("Config change changes requirement without a new bake"), Config->GetSpawnRequiredHeadroom(), Required + 500.0f);
	return !HasAnyErrors();
}

#endif
