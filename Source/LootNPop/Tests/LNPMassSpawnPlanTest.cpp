// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurfaceNavigation/LNPMassSpawnPlan.h"

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

#endif
