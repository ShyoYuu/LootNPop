// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Replication/LNPMassReplicator.h"
#include "HitDetection/LNPPositionHistoryFragment.h"
#include "Enemy/LNPEnemyConfig.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"

namespace
{
	FLNPEnemyWanderSegment TestSegment(double StartTime = 20.0)
	{
		FLNPEnemyWanderSegment Segment;
		Segment.bValid = true;
		Segment.Start = FVector(1000, 10, 2000);
		Segment.Goal = FVector(1400, 210, 2000);
		Segment.StartTime = StartTime;
		Segment.ArrivalTime = StartTime + 4.0;
		Segment.StopUntil = StartTime + 7.0;
		return Segment;
	}
	FLNPReplicatedWanderData FlightData(double StartTime = 20.0)
	{
		return FLNPReplicatedWanderData::Encode(TestSegment(StartTime), true, {}, 0.3f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPWanderCodecTest, "LootNPop.SurfaceNavigation.WanderReplication.Codec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLNPWanderCodecTest::RunTest(const FString& Parameters)
{
	const auto Data = FlightData();
	TestTrue(TEXT("Flight encoding enabled"), Data.IsEnabled() && Data.IsFlying());
	TestEqual(TEXT("Travel milliseconds"), Data.TravelMs, uint16(4000));
	TestEqual(TEXT("Hold milliseconds"), Data.HoldMs, uint16(3000));
	FLNPEnemyWanderSegment Decoded;
	Data.ApplyTo(Decoded, 22.0);
	TestTrue(TEXT("Canonical segment re-encodes without dirty changes"), Data == FLNPReplicatedWanderData::Encode(Decoded, true, {}, Decoded.LocalYaw));
	TestFalse(TEXT("Before start uses Idle phase"), Data.IsMoving(19.0));
	TestTrue(TEXT("Midpoint uses Move phase"), Data.IsMoving(22.0));
	TestFalse(TEXT("Arrival uses Idle phase"), Data.IsMoving(24.0));
	for (const double Now : {20.0, 22.0, 24.0, 26.0, 40.0})
	{
		FTransform Pose;
		LNPEnemyExactMovement::FParams Params;
		TestTrue(TEXT("Mid-packet or late join evaluation succeeds"), Data.Evaluate(Now, nullptr, Params, Pose));
		TestTrue(TEXT("Same server time yields same position"), Pose.GetLocation().Equals(TestSegment().Evaluate(Now), 0.001));
	}
	const double Wrap = static_cast<double>(1ULL << 32) / 1000.0;
	for (const double Start : {Wrap - 1.0, Wrap + 1.0, Wrap * 2.0 + 1.0})
	{
		const auto Wrapped = FlightData(Start);
		TestTrue(TEXT("Time wraps to nearest epoch"), FMath::Abs(Wrapped.DecodeStartTime(Start + 2.0) - Start) < 0.001);
		FTransform Pose;
		TestTrue(TEXT("Segment crosses wrap"), Wrapped.Evaluate(Start + 2.0, nullptr, {}, Pose));
		TestTrue(TEXT("Wrap preserves midpoint"), Pose.GetLocation().Equals(FVector(1200, 110, 2000), 0.01));
	}
	auto Invalid = TestSegment();
	Invalid.Goal.X = 40000;
	TestFalse(TEXT("Coordinate overflow falls back rather than clamps"), FLNPReplicatedWanderData::Encode(Invalid, true, {}, 0).IsEnabled());
	Invalid = TestSegment();
	Invalid.ArrivalTime += 70.0;
	TestFalse(TEXT("Duration overflow falls back"), FLNPReplicatedWanderData::Encode(Invalid, true, {}, 0).IsEnabled());
	TestFalse(TEXT("Ground requires a surface identity"), FLNPReplicatedWanderData::Encode(TestSegment(), false, {}, 0).IsEnabled());
	FTransform GroundPose(FVector(999, 999, 999));
	const auto Ground = FLNPReplicatedWanderData::Encode(TestSegment(), false, {0, 2, 1}, 0);
	TestFalse(TEXT("Ground waits for snapshot"), Ground.Evaluate(22.0, nullptr, {}, GroundPose));
	TestTrue(TEXT("Missing snapshot preserves position"), GroundPose.GetLocation().Equals(FVector(999, 999, 999)));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPWanderDirtyTest, "LootNPop.SurfaceNavigation.WanderReplication.Dirty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLNPWanderDirtyTest::RunTest(const FString& Parameters)
{
	FLNPMassFastArrayItem Item;
	FLNPEnemyFragment Enemy;
	FLNPEnemyActionFragment Action;
	FTransform Pose(TestSegment().Start);
	Item.Agent.SetHealthPct(MAX_uint8);
	Item.LastDirtyTime = 19.99;
	const auto Wander = FlightData();
	TestTrue(TEXT("Segment start bypasses period gate"), LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, Wander, 20.0, 0.3f));
	const FVector Cached = Item.Agent.GetReplicatedPositionYawData().GetPosition();
	for (int32 I = 1; I <= 120; ++I)
	{
		Pose.SetLocation(TestSegment().Evaluate(20.0 + I / 60.0));
		TestFalse(TEXT("Moving within segment has no position dirty"), LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, Wander, 20.0 + I / 60.0, 0.3f));
	}
	Enemy.Health = 50;
	TestTrue(TEXT("Health still updates during segment"), LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, Wander, 22.01, 0.3f));
	TestTrue(TEXT("Health update preserves cached PositionYaw"), Item.Agent.GetReplicatedPositionYawData().GetPosition().Equals(Cached));
	Action.Action = ELNPEnemyAction::Attack;
	++Action.Seq;
	TestTrue(TEXT("Attack onset still bypasses gate"), LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, Wander, 22.02, 0.3f));
	++Action.HitStopSeq;
	TestTrue(TEXT("Hit event still bypasses gate"), LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, Wander, 22.03, 0.3f));
	TestTrue(TEXT("Cancel bypasses gate and resumes position"), LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, {}, 22.04, 0.3f));
	TestFalse(TEXT("Cancel disables client segment"), Item.Agent.GetWander().IsEnabled());
	TestTrue(TEXT("Cancel sends current position"), Item.Agent.GetReplicatedPositionYawData().GetPosition().Equals(Pose.GetLocation(), 1.0));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPWanderClientTest, "LootNPop.SurfaceNavigation.WanderReplication.Client",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLNPWanderClientTest::RunTest(const FString& Parameters)
{
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("WanderClientTest"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto& Manager = World->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	TArray<FInstancedStruct> Fragments{ FInstancedStruct::Make<FTransformFragment>(),
		FInstancedStruct::Make<FLNPReplicatedMovementFragment>(), FInstancedStruct::Make<FLNPEnemyActionFragment>(),
		FInstancedStruct::Make<FLNPEnemyFragment>() };
	const auto Entity = Manager.CreateEntity(Fragments);
	FMassEntityView View(Manager, Entity);
	FLNPReplicatedAgent Agent;
	Agent.GetReplicatedPositionYawDataMutable().SetPosition(TestSegment().Start);
	Agent.SetWander(FlightData());
	Agent.SetHealthPct(MAX_uint8);
	// PostReplicatedAdd가 쓰는 초기 자세와 구간, PostReplicatedChange가 쓰는 실제 수신 helper를 소비한다.
	FLNPMassClientBubbleHandler::ApplyReplicatedTransform(View.GetFragmentData<FTransformFragment>(), Agent.GetReplicatedPositionYawData());
	FLNPMassClientBubbleHandler::ApplyMovementData(View, Agent);
	FLNPMassClientBubbleHandler::ApplyReplicatedAction(View, Agent, true);
	auto* Processor = NewObject<ULNPMassSmoothingProcessor>(World);
	Processor->CallInitialize(World, Manager.AsShared());
	auto Step = [&](double Now, float Delta = 1.f / 60.f)
	{
		World->TimeSeconds = Now;
		FMassExecutionContext Context = Manager.CreateExecutionContext(Delta);
		Context.SetExecutionType(EMassExecutionContextType::Processor);
		Context.SetFlushDeferredCommands(false);
		Processor->CallExecute(Manager, Context);
		Manager.FlushCommands();
	};
	Step(22.0);
	TestTrue(TEXT("Bubble entry midway uses current server time"), View.GetFragmentData<FTransformFragment>().GetTransform().GetLocation().Equals(FVector(1200, 110, 2000), 0.01));
	TestEqual(TEXT("Moving phase overrides stale Idle action"), View.GetFragmentData<FLNPEnemyActionFragment>().Action, ELNPEnemyAction::Move);
	Agent.SetHealthPct(128);
	FLNPMassClientBubbleHandler::ApplyMovementData(View, Agent);
	FLNPMassClientBubbleHandler::ApplyReplicatedHealth(View, Agent);
	Step(23.0);
	TestTrue(TEXT("Health-only packet never pulls back to cached pose"), View.GetFragmentData<FTransformFragment>().GetTransform().GetLocation().Equals(FVector(1300, 160, 2000), 0.01));
	Step(26.0);
	TestEqual(TEXT("Stopped phase overrides stale Move action"), View.GetFragmentData<FLNPEnemyActionFragment>().Action, ELNPEnemyAction::Idle);
	const FVector Before = View.GetFragmentData<FTransformFragment>().GetTransform().GetLocation();
	Agent.SetWander({});
	Agent.GetReplicatedPositionYawDataMutable().SetPosition(Before + FVector(20, 0, 0));
	FLNPMassClientBubbleHandler::ApplyMovementData(View, Agent);
	TestTrue(TEXT("Active handoff does not snap on receive"), View.GetFragmentData<FTransformFragment>().GetTransform().GetLocation().Equals(Before));
	Step(26.05, 0.05f);
	TestTrue(TEXT("Active handoff resumes ordinary smoothing"), View.GetFragmentData<FTransformFragment>().GetTransform().GetLocation().Equals(Before + FVector(20, 0, 0), 0.01));
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
#endif
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPWanderPoseControlTest, "LootNPop.SurfaceNavigation.WanderReplication.PoseControl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLNPWanderPoseControlTest::RunTest(const FString& Parameters)
{
	FLNPMassFastArrayItem SegmentItem, PoseItem;
	FLNPEnemyFragment Enemy;
	const auto Data = FlightData();
	FTransform Transform(TestSegment().Start);
	LNP::Replication::UpdateEnemyAgent(SegmentItem, Transform, Enemy, nullptr, Data, 20.0, 0.3f);
	LNP::Replication::UpdateEnemyAgent(PoseItem, Transform, Enemy, nullptr, Data, 20.0, 0.3f, false);
	Transform.SetLocation(TestSegment().Evaluate(21.0));
	TestFalse(TEXT("Segment suppresses moving pose"), LNP::Replication::UpdateEnemyAgent(
		SegmentItem, Transform, Enemy, nullptr, Data, 21.0, 0.3f));
	TestTrue(TEXT("Control sends same server position"), LNP::Replication::UpdateEnemyAgent(
		PoseItem, Transform, Enemy, nullptr, Data, 21.0, 0.3f, false));
	TestFalse(TEXT("Control does not publish segment"), PoseItem.Agent.GetWander().IsEnabled());
	TestTrue(TEXT("Control pose matches server"), PoseItem.Agent.GetReplicatedPositionYawData().GetPosition().Equals(Transform.GetLocation(), 1.0));
	TestTrue(TEXT("Input segment stays enabled"), Data.IsEnabled());
	Enemy.Health = 50;
	TestTrue(TEXT("Control still sends health"), LNP::Replication::UpdateEnemyAgent(
		PoseItem, Transform, Enemy, nullptr, Data, 22.0, 0.3f, false));
	TestEqual(TEXT("Health agrees between modes"), PoseItem.Agent.GetHealthPct(), FLNPReplicatedAgent::EncodeHealthPct(50, Enemy.MaxHealth));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPWanderRewindTest, "LootNPop.SurfaceNavigation.WanderReplication.Rewind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLNPWanderRewindTest::RunTest(const FString& Parameters)
{
	FLNPPositionHistoryFragment History;
	History.Count = 2;
	History.NextWriteIdx = 2;
	History.Samples[0] = {22.0, FVector(999, 999, 999)};
	History.Samples[1] = {22.05, FVector(999, 999, 999)};
	History.Samples[0].Wander = History.Samples[1].Wander = FlightData();
	TestTrue(TEXT("Wandering rewind omits ordinary interpolation delay"), History.IsWanderingAt(22.025));
	TestTrue(TEXT("Rewind uses segment function at requested time"), History.GetInterpolatedLocation(22.025).Equals(TestSegment().Evaluate(22.025), 0.001));
	History.Samples[1].Wander = {};
	TestFalse(TEXT("Active record switches delay rule"), History.IsWanderingAt(22.05));
	TestTrue(TEXT("Transition interval falls back to recorded positions"), History.GetInterpolatedLocation(22.025).Equals(FVector(999, 999, 999)));
	return !HasAnyErrors();
}
#endif
