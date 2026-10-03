// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Movement/LNPAsyncWalkingMode.h"
#include "Components/BoxComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPDestroyedMovementBaseTest,
	"LootNPop.SurfaceNavigation.ConditionalPatch.DestroyedMovementBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPDestroyedMovementBaseTest::RunTest(const FString& Parameters)
{
	UMoverComponent* Mover = NewObject<UMoverComponent>();
	ULNPAsyncWalkingMode* Mode = NewObject<ULNPAsyncWalkingMode>(Mover);
	UCommonLegacyMovementSettings* Settings = NewObject<UCommonLegacyMovementSettings>();
	Mode->CommonLegacySettings = Settings;
	UBoxComponent* Base = NewObject<UBoxComponent>();
	Base->SetMobility(EComponentMobility::Movable);
	Base->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FSimulationTickParams Params;
	Params.TimeStep.StepMs = 16.0;
	FMoverDefaultSyncState& Start = Params.StartState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	const FVector Position(20000.0, 8000.0, 15000.0);
	const FVector Velocity(10.0, 20.0, 30.0);
	Start.SetTransforms_WorldSpace(Position, FRotator::ZeroRotator, Velocity, FVector::ZeroVector, Base);
	FMoverTickEndData End;
	Mode->SimulationTick_Implementation(Params, End);
	const FMoverDefaultSyncState* Out = End.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!TestNotNull(TEXT("Stationary character retains a valid sync state"), Out)) { return false; }
	TestTrue(TEXT("Destroyed base is released without player movement"), Out->GetMovementBase() == nullptr);
	TestTrue(TEXT("Releasing the base preserves world position"), Out->GetLocation_WorldSpace().Equals(Position));
	TestTrue(TEXT("Releasing the base preserves velocity"), Out->GetVelocity_WorldSpace().Equals(Velocity));
	TestEqual(TEXT("Falling begins in the same simulation step"), End.MovementEndState.NextModeName, Settings->AirMovementModeName);
	TestEqual(TEXT("Falling receives the whole remaining step"), End.MovementEndState.RemainingMs, Params.TimeStep.StepMs);
	return !HasAnyErrors();
}
#endif
