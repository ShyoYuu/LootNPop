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
#endif
