// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Enemy/LNPEnemyActivity.h"
#include "Enemy/LNPEnemyActivityProcessor.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemyProcessors.h"
#include "Enemy/LNPEnemyFlightMovementProcessor.h"
#include "Enemy/LNPEnemySpatialGrid.h"
#include "Enemy/LNPEnemyWanderProcessor.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "Replication/LNPWanderReplication.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"
#include "DataAsset/LNPOctantPoolData.h"
#include "DataAsset/LNPOctantSurfaceData.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"
#include "LootPod/LNPLootPodMassTypes.h"
#include "GAS/LNPPoiseTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassNavigationFragments.h"
#include "MassEntityView.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPActivityBoundaryTest,
	"LootNPop.SurfaceNavigation.Activity.Boundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPActivityPodWakeTest,
	"LootNPop.SurfaceNavigation.Activity.PodWake", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPActivitySafetyTest,
	"LootNPop.SurfaceNavigation.Activity.Safety", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPActivityDormantConsumersTest,
	"LootNPop.SurfaceNavigation.Activity.DormantConsumers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	/** 실제 Mass 쿼리·deferred 태그 반영을 사용한다. 시간만 테스트에서 명시적으로 전진시킨다. */
	struct FActivityTestWorld
	{
		UWorld* World;
		FMassEntityManager* Manager;
		ULNPEnemyActivityProcessor* Processor;
		ULNPEnemyConfig* Ground;
		ULNPEnemyConfig* Flight;
		FMassEntityHandle Pod;
		FMassEntityHandle Player;
		FMassEntityHandle Members[2];
		float VisibilityDistance;

		FActivityTestWorld(float InVisibilityDistance = 12000.f) : VisibilityDistance(InVisibilityDistance)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ActivityTestWorld"));
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Manager = &World->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
			Ground = NewObject<ULNPEnemyConfig>(World);
			Ground->CombatMode = ELNPEnemyCombatMode::PureEntity;
			Flight = NewObject<ULNPEnemyConfig>(World);
			Flight->CombatMode = ELNPEnemyCombatMode::PureEntity;
			Flight->NavigationDomain = ELNPNavigationDomain::FreeFlight;
			TArray<FInstancedStruct> PodFragments{ FInstancedStruct::Make<FTransformFragment>(), FInstancedStruct::Make<FLNPLootPodFragment>() };
			Pod = Manager->CreateEntity(PodFragments);
			Manager->AddTagToEntity(Pod, FLNPLootPodTag::StaticStruct());
			Manager->GetFragmentDataChecked<FTransformFragment>(Pod).GetMutableTransform().SetLocation(FVector(30000, 0, 0));
			TArray<FInstancedStruct> PlayerFragments{ FInstancedStruct::Make<FTransformFragment>() };
			Player = Manager->CreateEntity(PlayerFragments);
			Manager->AddTagToEntity(Player, FLNPPlayerTag::StaticStruct());
			MovePlayer(30000.f);
			Members[0] = Enemy(Ground, Pod);
			Members[1] = Enemy(Flight, Pod);
			Processor = NewObject<ULNPEnemyActivityProcessor>(World);
			Processor->CallInitialize(World, Manager->AsShared());
		}
		~FActivityTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		FMassEntityHandle Enemy(ULNPEnemyConfig* Config, FMassEntityHandle Parent)
		{
			TArray<FInstancedStruct> Fragments{
				FInstancedStruct::Make<FTransformFragment>(), FInstancedStruct::Make<FLNPEnemyFragment>(),
				FInstancedStruct::Make<FLNPEnemyTargetingFragment>(), FInstancedStruct::Make<FLNPEnemyVelocityFragment>(),
				FInstancedStruct::Make<FLNPEntityAttackFragment>(), FInstancedStruct::Make<FMassActorFragment>(),
				FInstancedStruct::Make<FLNPPoiseFragment>(), FInstancedStruct::Make<FMassMoveTargetFragment>(),
				FInstancedStruct::Make<FLNPEnemySeparationFragment>(), FInstancedStruct::Make<FLNPEnemyIdleFragment>(),
				FInstancedStruct::Make<FLNPEnemyPathFragment>() };
			if (Config->IsFlying())
				Fragments.Add(FInstancedStruct::Make<FLNPEnemyFlightFragment>());
			FLNPEnemySharedFragment Shared;
			Shared.Config = Config;
			Shared.ActivityActiveDistance = 6000.f;
			Shared.ActivityCullDistance = 12000.f;
			Shared.ActivityVisibilityDistance = VisibilityDistance;
			FMassArchetypeSharedFragmentValues SharedValues;
			SharedValues.Add(Manager->GetOrCreateConstSharedFragment(Shared));
			SharedValues.Sort();
			// 인스턴스 목록 오버로드는 shared 값의 composition을 추가하지 않는다.
			const FMassArchetypeHandle Archetype = Manager->CreateArchetype(FMassArchetypeCompositionDescriptor(FMassFragmentBitSet(Fragments)));
			const FMassEntityHandle Handle = Manager->CreateEntity(Archetype, SharedValues);
			Manager->GetFragmentDataChecked<FTransformFragment>(Handle).GetMutableTransform().SetLocation(FVector(30000, 0, 0));
			Manager->AddTagToEntity(Handle, FLNPEnemyTag::StaticStruct());
			if (Config->IsFlying())
				Manager->AddTagToEntity(Handle, FLNPEnemyFlyingTag::StaticStruct());
			FLNPEnemyFragment& Data = Manager->GetFragmentDataChecked<FLNPEnemyFragment>(Handle);
			Data.ParentLootPod = Parent;
			Data.ParentPodLocation = FVector(30000, 0, 0);
			Data.SurfaceHandle.OctantSlot = 0;
			Data.SurfaceHandle.LocalLayerId = 0;
			Data.SurfaceHandle.Generation = 1;
			return Handle;
		}
		void MovePlayer(float X)
		{
			Manager->GetFragmentDataChecked<FTransformFragment>(Player).GetMutableTransform().SetLocation(FVector(30000.f + X, 0, 0));
		}
		void Step(double Time)
		{
			World->TimeSeconds = Time;
			FMassExecutionContext Context = Manager->CreateExecutionContext(1.f / 60.f);
			Context.SetExecutionType(EMassExecutionContextType::Processor);
			Context.SetFlushDeferredCommands(false);
			Processor->CallExecute(*Manager, Context);
			Manager->FlushCommands();
		}
		bool Dormant(int32 Index) const { return FMassEntityView(*Manager, Members[Index]).HasTag<FLNPEnemyDormantTag>(); }
		FLNPLootPodFragment& PodData() { return Manager->GetFragmentDataChecked<FLNPLootPodFragment>(Pod); }
	};
}

bool FLNPActivityBoundaryTest::RunTest(const FString& Parameters)
{
	using EBand = ELNPEnemyActivityBand;
	auto Band = [](EBand Previous, double Distance, bool Force = false, bool CanSleep = true)
	{
		return LNPEnemyActivity::SelectBand(Previous, Distance * Distance, 6000.f, 12000.f, Force, CanSleep);
	};
	TestEqual(TEXT("Active entry includes 60m"), Band(EBand::Wander, 6000), EBand::Active);
	TestEqual(TEXT("Active retains 65m"), Band(EBand::Active, 6500), EBand::Active);
	TestEqual(TEXT("Active exits past 66m"), Band(EBand::Active, 6601), EBand::Wander);
	TestEqual(TEXT("Dormant enters past 132m"), Band(EBand::Wander, 13201), EBand::Dormant);
	TestEqual(TEXT("Dormant retains 129m"), Band(EBand::Dormant, 12900), EBand::Dormant);
	TestEqual(TEXT("Dormant exits at 126m"), Band(EBand::Dormant, 12600), EBand::Wander);
	TestEqual(TEXT("Visible member prevents sleeping"), Band(EBand::Wander, 15000, false, false), EBand::Wander);
	TestEqual(TEXT("Event wake overrides distance"), Band(EBand::Dormant, 30000, true), EBand::Active);
	TestEqual(TEXT("Missing distance stays active"), LNPEnemyActivity::SelectBand(EBand::Dormant, DBL_MAX, 0, 12000, false, true), EBand::Active);
	TestEqual(TEXT("Changed settings drive boundary"), LNPEnemyActivity::SelectBand(EBand::Wander, 18000.0 * 18000.0, 8000, 20000, false, true), EBand::Wander);
	return true;
}

bool FLNPActivityPodWakeTest::RunTest(const FString& Parameters)
{
	FActivityTestWorld Fixture;
	const FMassEntityHandle Orphan = Fixture.Enemy(Fixture.Ground, FMassEntityHandle());
	Fixture.Step(0);
	TestTrue(TEXT("Ground and flight sleep together"), Fixture.Dormant(0) && Fixture.Dormant(1));
	TestFalse(TEXT("Unowned enemy stays active"), FMassEntityView(*Fixture.Manager, Orphan).HasTag<FLNPEnemyDormantTag>());
	Fixture.Manager->GetFragmentDataChecked<FLNPEnemyFragment>(Fixture.Members[0]).bActivityWakeRequested = true;
	Fixture.Step(0.01);
	TestFalse(TEXT("Hit wakes ground before distance poll"), Fixture.Dormant(0));
	TestFalse(TEXT("Hit wakes flying peer"), Fixture.Dormant(1));
	Fixture.Step(5.0);
	TestFalse(TEXT("Wake hold survives distance poll"), Fixture.Dormant(0));
	Fixture.Step(11.0);
	TestTrue(TEXT("Returns to sleep after hold"), Fixture.Dormant(0));
	Fixture.PodData().State = ELNPLootPodState::Looting;
	Fixture.Step(11.01);
	TestFalse(TEXT("Looting wakes whole pod immediately"), Fixture.Dormant(1));
	Fixture.PodData().State = ELNPLootPodState::Idle;
	Fixture.MovePlayer(7000);
	Fixture.Step(22.0);
	TestEqual(TEXT("Middle distance uses wander"), Fixture.PodData().ActivityBand, ELNPEnemyActivityBand::Wander);
	Fixture.MovePlayer(5000);
	Fixture.Step(22.3);
	TestEqual(TEXT("Player approach activates pod"), Fixture.PodData().ActivityBand, ELNPEnemyActivityBand::Active);
	Fixture.MovePlayer(30000);
	Fixture.Step(23);
	Fixture.Manager->GetFragmentDataChecked<FLNPEnemyFragment>(Fixture.Members[0]).ParentLootPod = FMassEntityHandle();
	Fixture.Step(23.01);
	TestFalse(TEXT("Released member wakes before next distance poll"), Fixture.Dormant(0));
	return true;
}

bool FLNPActivitySafetyTest::RunTest(const FString& Parameters)
{
	FActivityTestWorld Fixture(24000.f);
	Fixture.Step(0);
	Fixture.Manager->GetFragmentDataChecked<FLNPEnemyVelocityFragment>(Fixture.Members[0]).Velocity = FVector(100, 0, 0);
	Fixture.Step(0.01);
	TestFalse(TEXT("Knockback cannot freeze ground or flight"), Fixture.Dormant(0) || Fixture.Dormant(1));
	Fixture.Manager->GetFragmentDataChecked<FLNPEnemyVelocityFragment>(Fixture.Members[0]).Velocity = FVector::ZeroVector;
	Fixture.Step(11);
	Fixture.Manager->GetFragmentDataChecked<FLNPEnemyFragment>(Fixture.Members[0]).SurfaceHandle = FLNPSurfaceHandle();
	Fixture.Step(11.01);
	TestFalse(TEXT("Ground without support cannot sleep"), Fixture.Dormant(0));
	Fixture.Manager->GetFragmentDataChecked<FLNPEnemyFragment>(Fixture.Members[0]).SurfaceHandle =
		Fixture.Manager->GetFragmentDataChecked<FLNPEnemyFragment>(Fixture.Members[1]).SurfaceHandle;
	Fixture.MovePlayer(13500);
	Fixture.Manager->GetFragmentDataChecked<FTransformFragment>(Fixture.Members[1]).GetMutableTransform().SetLocation(FVector(43000, 0, 0));
	Fixture.Step(22);
	TestEqual(TEXT("Pod distance cannot hide a visible flying member"), Fixture.PodData().ActivityBand, ELNPEnemyActivityBand::Wander);
	Fixture.MovePlayer(20000);
	Fixture.Step(22.3);
	TestFalse(TEXT("Visualization beyond replication cull prevents sleep"), Fixture.Dormant(0));
	Fixture.MovePlayer(50000);
	Fixture.Step(22.6);
	TestTrue(TEXT("Group sleeps once every member is outside visibility"), Fixture.Dormant(0));
	Fixture.Manager->GetFragmentDataChecked<FLNPEnemyFragment>(Fixture.Members[0]).Health = 0.f;
	Fixture.Step(22.61);
	TestFalse(TEXT("Death wakes peers before movement consumes death velocity"), Fixture.Dormant(0) || Fixture.Dormant(1));
	Fixture.Manager->GetFragmentDataChecked<FLNPEnemyFragment>(Fixture.Members[0]).Health = 100.f;
	Fixture.Step(34);
	TestTrue(TEXT("Group can sleep again after death wake hold"), Fixture.Dormant(0));
	Fixture.PodData().State = ELNPLootPodState::Popped;
	Fixture.Step(34.01);
	TestFalse(TEXT("Popped pod releases sleeping members"), Fixture.Dormant(0) || Fixture.Dormant(1));
	return true;
}

bool FLNPActivityDormantConsumersTest::RunTest(const FString& Parameters)
{
	FActivityTestWorld Fixture;
	Fixture.Step(0);
	TestTrue(TEXT("Both domains enter dormant before consumer check"), Fixture.Dormant(0) && Fixture.Dormant(1));
	uint64 BeforeCount, QueryNs, LockNs;
	const auto* Collision = Fixture.World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
	Collision->GetTotals(BeforeCount, QueryNs, LockNs);
	ULNPEnemyMovementProcessor* GroundMove = NewObject<ULNPEnemyMovementProcessor>(Fixture.World);
	ULNPEnemyFlightMovementProcessor* FlightMove = NewObject<ULNPEnemyFlightMovementProcessor>(Fixture.World);
	ULNPEnemySpatialGridProcessor* Grid = NewObject<ULNPEnemySpatialGridProcessor>(Fixture.World);
	for (UMassProcessor* Processor : { static_cast<UMassProcessor*>(GroundMove), static_cast<UMassProcessor*>(FlightMove), static_cast<UMassProcessor*>(Grid) })
	{
		Processor->CallInitialize(Fixture.World, Fixture.Manager->AsShared());
		FMassExecutionContext Context = Fixture.Manager->CreateExecutionContext(1.f / 60.f);
		Context.SetExecutionType(EMassExecutionContextType::Processor);
		Context.SetFlushDeferredCommands(false);
		Processor->CallExecute(*Fixture.Manager, Context);
	}
	Fixture.Manager->FlushCommands();
	uint64 AfterCount;
	Collision->GetTotals(AfterCount, QueryNs, LockNs);
	TestEqual(TEXT("Dormant consumers issue no exact collision queries"), AfterCount, BeforeCount);
	TestEqual(TEXT("Sleeping ground does not move"), Fixture.Manager->GetFragmentDataChecked<FTransformFragment>(Fixture.Members[0]).GetTransform().GetLocation(), FVector(30000, 0, 0));
	TestEqual(TEXT("Sleeping flight does not move"), Fixture.Manager->GetFragmentDataChecked<FTransformFragment>(Fixture.Members[1]).GetTransform().GetLocation(), FVector(30000, 0, 0));
	int32 GroundNeighbors = 0;
	int32 FlyingNeighbors = 0;
	Fixture.World->GetSubsystem<ULNPEnemySpatialGridSubsystem>()->ForEachNeighbor(FVector(30000, 0, 0), 1000,
		[&GroundNeighbors](FMassEntityHandle, const FVector&) { ++GroundNeighbors; });
	Fixture.World->GetSubsystem<ULNPFlyingSpatialGridSubsystem>()->ForEachNeighbor(FVector(30000, 0, 0), 1000,
		[&FlyingNeighbors](FMassEntityHandle, const FVector&) { ++FlyingNeighbors; });
	TestEqual(TEXT("Dormant ground remains in hit-query grid"), GroundNeighbors, 1);
	TestEqual(TEXT("Dormant flight remains in hit-query grid"), FlyingNeighbors, 1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPEnemyWanderRuntimeTest,
	"LootNPop.SurfaceNavigation.Wander.Runtime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPEnemyWanderRuntimeTest::RunTest(const FString& Parameters)
{
	FActivityTestWorld Fixture;
	const TCHAR* SurfacePath = TEXT("/Game/Maps/SurfaceNavigation/Fixtures/DA_OctantSurface_Fixture_Regression.DA_OctantSurface_Fixture_Regression");
	ULNPOctantSurfaceData* Asset = LoadObject<ULNPOctantSurfaceData>(nullptr, SurfacePath);
	if (!TestNotNull(TEXT("Saved regression surface loads"), Asset))
		return false;
	TArray<FLNPOctantDefinition> Definitions;
	TArray<ULNPOctantSurfaceData*> Assets;
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		auto& Definition = Definitions.AddDefaulted_GetRef();
		Definition.LevelAsset = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Regression.LVI_Octant_Fixture_Regression")));
		Definition.SurfaceData = TSoftObjectPtr<ULNPOctantSurfaceData>(FSoftObjectPath(SurfacePath));
		Definition.SeamSignature = TEXT("WanderRegressionSeam");
		Assets.Add(Asset);
	}
	auto Snapshot = MakeShared<FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe>();
	FString Error;
	if (!TestTrue(TEXT("Saved fixture assembles"), LNPSurfaceDataLoading::ValidateAndBuildSnapshot(Definitions, Assets, 1, *Snapshot, Error)))
	{
		AddError(Error);
		return false;
	}
	auto* Surfaces = Fixture.World->GetSubsystem<ULNPSurfaceDataSubsystem>();
	Surfaces->PublishedSnapshot = Snapshot;
	Surfaces->bSnapshotReady.store(true, std::memory_order_release);
	const FVector Out = FVector(1, 1, 1).GetSafeNormal();
	const FVector Home = Out * 30000.f;
	for (int32 I = 0; I < 2; ++I)
	{
		auto& Enemy = Fixture.Manager->GetFragmentDataChecked<FLNPEnemyFragment>(Fixture.Members[I]);
		Enemy.ActivityBand = ELNPEnemyActivityBand::Wander;
		Enemy.ParentPodLocation = Home;
		const FVector Position = Home - Out * (I == 0 ? Fixture.Ground->CapsuleHalfHeight : 1000.f);
		Fixture.Manager->GetFragmentDataChecked<FTransformFragment>(Fixture.Members[I]).GetMutableTransform().SetLocation(Position);
	}
	auto* Processor = NewObject<ULNPEnemyWanderProcessor>(Fixture.World);
	Processor->CallInitialize(Fixture.World, Fixture.Manager->AsShared());
	auto Step = [&](double Time)
	{
		Fixture.World->TimeSeconds = Time;
		FMassExecutionContext Context = Fixture.Manager->CreateExecutionContext(1.f / 60.f);
		Context.SetExecutionType(EMassExecutionContextType::Processor);
		Context.SetFlushDeferredCommands(false);
		Processor->CallExecute(*Fixture.Manager, Context);
		Fixture.Manager->FlushCommands();
	};
	auto Idle = [&](int32 I) -> FLNPEnemyIdleFragment& { return Fixture.Manager->GetFragmentDataChecked<FLNPEnemyIdleFragment>(Fixture.Members[I]); };
	auto Enemy = [&](int32 I) -> FLNPEnemyFragment& { return Fixture.Manager->GetFragmentDataChecked<FLNPEnemyFragment>(Fixture.Members[I]); };
	auto Position = [&](int32 I) { return Fixture.Manager->GetFragmentDataChecked<FTransformFragment>(Fixture.Members[I]).GetTransform().GetLocation(); };
	Step(20.0);
	TestTrue(TEXT("Ground creates a moving validated segment"), Idle(0).WanderSegment.ArrivalTime > 20.0);
	TestTrue(TEXT("Flight creates a swept segment"), Idle(1).WanderSegment.ArrivalTime > 20.0);
	const auto GroundSegment = Idle(0).WanderSegment;
	const auto FlightSegment = Idle(1).WanderSegment;
	const auto GroundData = FLNPReplicatedWanderData::Encode(GroundSegment, false, Enemy(0).SurfaceHandle, GroundSegment.LocalYaw);
	const auto FlightData = FLNPReplicatedWanderData::Encode(FlightSegment, true, {}, FlightSegment.LocalYaw);
	FLNPSurfaceDataSnapshot ClientSnapshot = *Snapshot;
	++ClientSnapshot.Generation;
	LNPEnemyExactMovement::FParams GroundParams;
	GroundParams.CapsuleHalfHeight = Fixture.Ground->CapsuleHalfHeight;
	const auto* Collision = Fixture.World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
	const uint64 GroundQueries = Collision->GetQueryCount(ELNPWorldQueryClass::GroundRiskFallback);
	const uint64 FlightQueries = Collision->GetQueryCount(ELNPWorldQueryClass::FlightSteering);
	for (int32 Frame = 1; Frame <= 60; ++Frame)
	{
		const double Now = 20.0 + FMath::Min(GroundSegment.ArrivalTime - 20.0, FlightSegment.ArrivalTime - 20.0) * Frame / 60.0;
		Step(Now);
		FTransform GroundPose, FlightPose;
		TestTrue(TEXT("Ground client resolves its own snapshot generation"), GroundData.Evaluate(Now, &ClientSnapshot, GroundParams, GroundPose));
		TestTrue(TEXT("Ground client matches server at same time"), GroundPose.GetLocation().Equals(Position(0), 0.01));
		TestTrue(TEXT("Flight client evaluates"), FlightData.Evaluate(Now, nullptr, {}, FlightPose));
		TestTrue(TEXT("Flight client matches server at same time"), FlightPose.GetLocation().Equals(Position(1), 0.01));
	}
	TestEqual(TEXT("Static interpolation adds no ground exact query"), Collision->GetQueryCount(ELNPWorldQueryClass::GroundRiskFallback), GroundQueries);
	TestEqual(TEXT("Static interpolation adds no flight exact query"), Collision->GetQueryCount(ELNPWorldQueryClass::FlightSteering), FlightQueries);
	TestEqual(TEXT("Ground keeps the same Layer"), Enemy(0).SurfaceHandle.LocalLayerId, uint16(0));
	const int32 FirstArrival = GroundSegment.ArrivalTime <= FlightSegment.ArrivalTime ? 0 : 1;
	TestTrue(TEXT("First domain arrives at its endpoint"), Position(FirstArrival).Equals(FirstArrival == 0 ? GroundSegment.Goal : FlightSegment.Goal, 2.f));
	Step(FMath::Max(GroundSegment.ArrivalTime, FlightSegment.ArrivalTime));
	TestTrue(TEXT("Second domain arrives at its endpoint"), Position(1 - FirstArrival).Equals(FirstArrival == 0 ? FlightSegment.Goal : GroundSegment.Goal, 2.f));
	const FVector HitPosition = Position(1);
	Enemy(1).bActivityWakeRequested = true;
	Step(Fixture.World->TimeSeconds + 0.01);
	TestFalse(TEXT("Hit cancels the flight segment immediately"), Idle(1).WanderSegment.bValid);
	TestFalse(TEXT("Hit releases ordinary movement"), Idle(1).bWanderHandled);
	TestTrue(TEXT("Cancellation preserves current position"), Position(1).Equals(HitPosition));
	Enemy(0).ActivityBand = ELNPEnemyActivityBand::Active;
	Step(Fixture.World->TimeSeconds + 0.01);
	TestFalse(TEXT("Active transition cancels ground segment"), Idle(0).WanderSegment.bValid);
	TestTrue(TEXT("Active transition requests a new ordinary goal"), Idle(0).bNeedNewWanderTarget);
	Enemy(0).ActivityBand = ELNPEnemyActivityBand::Wander;
	Step(40.0);
	TestTrue(TEXT("Ground recreates a segment"), Idle(0).WanderSegment.bValid);
	auto Changed = MakeShared<FLNPSurfaceDataSnapshot, ESPMode::ThreadSafe>(*Snapshot);
	Changed->Generation = 2;
	Surfaces->PublishedSnapshot = Changed;
	Step(40.01);
	TestFalse(TEXT("Terrain publication cancels the segment before movement"), Idle(0).WanderSegment.bValid);
	TestTrue(TEXT("Terrain change requests group wake"), Enemy(0).bActivityWakeRequested);
	Surfaces->PublishedSnapshot = Snapshot;
	const FVector CancelPosition = Position(1);
	auto CheckCancel = [&](const TCHAR* Reason)
	{
		Enemy(1).ActivityBand = ELNPEnemyActivityBand::Wander;
		Enemy(1).bActivityWakeRequested = false;
		Idle(1).WanderSegment = FlightSegment;
		Step(50.0);
		TestFalse(Reason, Idle(1).WanderSegment.bValid);
		TestFalse(TEXT("Unsafe state releases ordinary movement"), Idle(1).bWanderHandled);
		TestTrue(TEXT("Unsafe state cancels without displacement"), Position(1).Equals(CancelPosition));
	};
	auto& Target = Fixture.Manager->GetFragmentDataChecked<FLNPEnemyTargetingFragment>(Fixture.Members[1]);
	Target.State = ELNPTargetingState::Confirmed;
	CheckCancel(TEXT("Combat cancels segment"));
	Target.State = ELNPTargetingState::None;
	auto& Velocity = Fixture.Manager->GetFragmentDataChecked<FLNPEnemyVelocityFragment>(Fixture.Members[1]);
	Velocity.Velocity = FVector(100, 0, 0);
	CheckCancel(TEXT("Knockback cancels segment"));
	Velocity.Velocity = FVector::ZeroVector;
	Enemy(1).bNeedsHomeCheck = true;
	CheckCancel(TEXT("Home reassignment cancels segment"));
	Enemy(1).bNeedsHomeCheck = false;
	auto& Poise = Fixture.Manager->GetFragmentDataChecked<FLNPPoiseFragment>(Fixture.Members[1]);
	Poise.bIsGroggy = true;
	CheckCancel(TEXT("Groggy cancels segment"));
	Poise.bIsGroggy = false;
	// 같은 Pod의 겹친 출발점은 구간 정지에 갇히지 않고 기존 분리 경로로 복귀한다.
	const FMassEntityHandle Twin = Fixture.Enemy(Fixture.Flight, Fixture.Pod);
	Fixture.Manager->GetFragmentDataChecked<FTransformFragment>(Twin).GetMutableTransform().SetLocation(Position(1));
	Enemy(1).ActivityBand = ELNPEnemyActivityBand::Wander;
	Enemy(1).bActivityWakeRequested = false;
	Idle(1).WanderSegment = FLNPEnemyWanderSegment();
	Step(51.0);
	TestTrue(TEXT("Overlapping start wakes the group"), Enemy(1).bActivityWakeRequested);
	TestFalse(TEXT("Overlap releases ordinary separation"), Idle(1).bWanderHandled);
	Fixture.Manager->DestroyEntity(Twin);
	// 새 구간 시점의 실제 Chaos blocker가 지상·비행 후보 모두를 막는다.
	AActor* Barrier = Fixture.World->SpawnActor<AActor>();
	auto* Mesh = NewObject<UStaticMeshComponent>(Barrier);
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Mesh->SetCollisionProfileName(TEXT("LNPStaticBlocker"));
	Mesh->SetWorldLocation(Home - Out * 1000.f);
	Mesh->SetWorldScale3D(FVector(40));
	Barrier->SetRootComponent(Mesh);
	Mesh->RegisterComponent();
	auto* Identities = Fixture.World->GetSubsystem<ULNPHitIdentitySubsystem>();
	Identities->RegisterRuntimeSource(Mesh);
	Identities->Tick(0.f);
	for (int32 I = 0; I < 2; ++I)
	{
		Enemy(I).ActivityBand = ELNPEnemyActivityBand::Wander;
		Enemy(I).bActivityWakeRequested = false;
		Idle(I).WanderSegment = FLNPEnemyWanderSegment();
	}
	const FVector BeforeGround = Position(0), BeforeFlight = Position(1);
	Step(60.0);
	TestTrue(TEXT("Blocked ground segment stays stationary"), Idle(0).WanderSegment.Start.Equals(Idle(0).WanderSegment.Goal));
	TestTrue(TEXT("Blocked flight segment stays stationary"), Idle(1).WanderSegment.Start.Equals(Idle(1).WanderSegment.Goal));
	Step(61.0);
	TestTrue(TEXT("Blocked ground has no penetration movement beyond coordinate quantization"), Position(0).Equals(BeforeGround, 1.f));
	TestTrue(TEXT("Blocked flight has no penetration movement"), Position(1).Equals(BeforeFlight));
	TestEqual(TEXT("Registered barriers produce no unknown hit"), Collision->GetUnknownHitCount(), uint64(0));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPEnemyWanderTimeTest,
	"LootNPop.SurfaceNavigation.Wander.TimeAndRevision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPEnemyWanderTimeTest::RunTest(const FString& Parameters)
{
	FLNPEnemyWanderSegment Segment;
	Segment.bValid = true;
	Segment.Start = FVector(10, 0, 0);
	Segment.Goal = FVector(110, 0, 0);
	Segment.StartTime = 20.0;
	Segment.ArrivalTime = 24.0;
	Segment.StopUntil = 27.0;
	Segment.Generation = 5;
	Segment.TileCount = 1;
	Segment.Tiles[0] = {7, 3};
	FLNPNavOverlay Overlay;
	Overlay.TileRevisions.Add(7, 3);
	TestTrue(TEXT("Before start clamps to start"), Segment.Evaluate(19.0).Equals(Segment.Start));
	TestTrue(TEXT("Server time midpoint"), Segment.Evaluate(22.0).Equals(FVector(60, 0, 0)));
	TestTrue(TEXT("Stop phase holds exact endpoint"), Segment.Evaluate(26.0).Equals(Segment.Goal));
	TestTrue(TEXT("Current tile is valid"), Segment.IsCurrent(5, &Overlay));
	Overlay.TileRevisions.Add(8, 10);
	TestTrue(TEXT("Unrelated tile does not invalidate"), Segment.IsCurrent(5, &Overlay));
	Overlay.TileRevisions.Add(7, 4);
	TestFalse(TEXT("Traversed tile invalidates"), Segment.IsCurrent(5, &Overlay));
	TestFalse(TEXT("Generation invalidates"), Segment.IsCurrent(6, nullptr));
	Segment.ArrivalTime = Segment.StartTime;
	TestTrue(TEXT("Rejected stationary segment has no divide by zero"), Segment.Evaluate(20.0).Equals(Segment.Goal));
	return !HasAnyErrors();
}
#endif
