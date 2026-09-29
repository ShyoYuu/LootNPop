// Copyright (c) 2026 LootNPop. All rights reserved.

#if WITH_AUTOMATION_TESTS

#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPSurfaceDataSubsystem.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPEnemySurfaceHandleIdentityTest,
	"LootNPop.SurfaceNavigation.EnemyMovement.SurfaceHandleIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPEnemyCachedGroundDecisionTest,
	"LootNPop.SurfaceNavigation.EnemyMovement.CachedGroundDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPEnemyDynamicSupportContactTest,
	"LootNPop.SurfaceNavigation.EnemyMovement.DynamicSupportContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPEnemyActorHandoffTest,
	"LootNPop.SurfaceNavigation.EnemyMovement.ActorHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPEnemySurfaceHandleIdentityTest::RunTest(const FString& Parameters)
{
	FLNPExactHitIdentity Identity;
	Identity.Lifetime = ELNPExactSourceLifetime::Static;
	Identity.Roles = ELNPExactSourceRole::Support | ELNPExactSourceRole::Blocker;
	Identity.Slot = 3;
	Identity.LocalLayerId = 7;
	Identity.SurfaceDataGeneration = 42;

	FLNPSurfaceHandle Handle;
	TestTrue(TEXT("Known static Support identity resolves"), LNPEnemySurfaceMovement::MakeSurfaceHandle(Identity, Handle));
	TestEqual(TEXT("Resolved slot"), Handle.OctantSlot, static_cast<uint16>(3));
	TestEqual(TEXT("Resolved layer"), Handle.LocalLayerId, static_cast<uint16>(7));
	TestEqual(TEXT("Resolved generation"), Handle.Generation, static_cast<uint64>(42));

	FLNPSurfaceHandle AirborneHandle = Handle;
	LNPEnemySurfaceMovement::UpdateSurfaceHandleAfterAirborne(false, Identity, AirborneHandle);
	TestFalse(TEXT("Airborne movement invalidates the previous handle"), AirborneHandle.IsValid());
	LNPEnemySurfaceMovement::UpdateSurfaceHandleAfterAirborne(true, Identity, AirborneHandle);
	TestTrue(TEXT("Static landing reacquires a handle"), AirborneHandle.IsValid());
	TestEqual(TEXT("Landing reacquires the exact Layer"), AirborneHandle.LocalLayerId, static_cast<uint16>(7));

	Identity.Roles = ELNPExactSourceRole::Blocker;
	TestFalse(TEXT("Blocker-only identity does not resolve"), LNPEnemySurfaceMovement::MakeSurfaceHandle(Identity, Handle));
	TestFalse(TEXT("Rejected handle is invalid"), Handle.IsValid());
	LNPEnemySurfaceMovement::UpdateSurfaceHandleAfterAirborne(true, Identity, AirborneHandle);
	TestFalse(TEXT("Blocker landing cannot acquire a handle"), AirborneHandle.IsValid());

	Identity.Roles = ELNPExactSourceRole::Support;
	Identity.LocalLayerId = LNPSupportLayers::NoLayer;
	TestFalse(TEXT("Support without a face Layer does not resolve"), LNPEnemySurfaceMovement::MakeSurfaceHandle(Identity, Handle));

	Identity.LocalLayerId = 0;
	Identity.SurfaceDataGeneration = 0;
	TestFalse(TEXT("Unbound SurfaceData generation does not resolve"), LNPEnemySurfaceMovement::MakeSurfaceHandle(Identity, Handle));
	return true;
}

bool FLNPEnemyCachedGroundDecisionTest::RunTest(const FString& Parameters)
{
	constexpr double SurfaceRadius = 30000.0;
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
			Crust.Samples[Index].Radius = SurfaceRadius;
			Crust.Samples[Index].Normal = FVector3f(-Direction);
			Crust.Samples[Index].Flags = ELNPSupportSampleFlags::Valid | ELNPSupportSampleFlags::Walkable;
		}
	}

	FLNPSupportAtlasSource Source;
	Source.Key = TEXT("CrustActor.CrustComponent");
	Source.FaceMap.UniformLayer = 0;
	TArray<FLNPSupportAtlasSource> Sources = {MoveTemp(Source)};
	FLNPSupportCodecSettings Codec;
	Codec.BaseRadius = SurfaceRadius;
	TArray<uint8> Payload;
	FString Error;
	if (!TestTrue(TEXT("Synthetic cache payload encodes"),
		LNPSupportAtlas::Encode(MakeArrayView(&Crust, 1), Sources, Codec, Payload, Error)))
	{
		AddError(Error);
		return false;
	}

	TSharedPtr<FLNPSupportAtlas, ESPMode::ThreadSafe> Atlas = MakeShared<FLNPSupportAtlas, ESPMode::ThreadSafe>();
	if (!TestTrue(TEXT("Synthetic cache payload decodes"), LNPSupportAtlas::Decode(Payload, *Atlas, Error)))
	{
		AddError(Error);
		return false;
	}

	FLNPSurfaceDataSnapshot Snapshot;
	Snapshot.Generation = 23;
	Snapshot.Slots.SetNum(8);
	Snapshot.Slots[0].Support = Atlas;

	LNPEnemyExactMovement::FParams Params;
	Params.CapsuleHalfHeight = 88.f;
	const FVector3d Direction = FVector3d(1.0, 1.0, 1.0).GetSafeNormal();
	const FVector Target = FVector(Direction * (SurfaceRadius - Params.CapsuleHalfHeight + 10.0));
	FLNPSurfaceHandle Handle;
	Handle.OctantSlot = 0;
	Handle.LocalLayerId = 0;
	Handle.Generation = Snapshot.Generation;
	FVector GroundedLocation;
	TestTrue(TEXT("High-confidence cache resolves grounded location"),
		LNPEnemySurfaceMovement::TryResolveCachedGround(Snapshot, Params, Target, Handle, GroundedLocation));
	TestTrue(TEXT("Cache result returns a valid handle"), Handle.IsValid());
	TestEqual(TEXT("Cache handle uses snapshot generation"), Handle.Generation, static_cast<uint64>(23));
	TestEqual(TEXT("Cache handle uses selected slot"), Handle.OctantSlot, static_cast<uint16>(0));
	TestEqual(TEXT("Cache handle uses crust Layer"), Handle.LocalLayerId, static_cast<uint16>(0));
	TestTrue(TEXT("Capsule center is placed half-height inside the surface"),
		FMath::Abs(GroundedLocation.Size() - (SurfaceRadius - Params.CapsuleHalfHeight)) <= 0.5);

	Handle = FLNPSurfaceHandle();
	TestFalse(TEXT("Missing current Layer identity requires exact fallback"),
		LNPEnemySurfaceMovement::TryResolveCachedGround(Snapshot, Params, Target, Handle, GroundedLocation));
	Handle.OctantSlot = 0;
	Handle.LocalLayerId = 0;
	Handle.Generation = Snapshot.Generation - 1;
	TestFalse(TEXT("Stale Layer identity requires exact fallback"),
		LNPEnemySurfaceMovement::TryResolveCachedGround(Snapshot, Params, Target, Handle, GroundedLocation));
	Handle.OctantSlot = 0;
	Handle.LocalLayerId = 0;
	Handle.Generation = Snapshot.Generation;

	for (uint8& Flags : Atlas->Layers[0].Flags)
	{
		Flags |= static_cast<uint8>(ELNPSupportSampleFlags::NeedsExact);
	}
	TestFalse(TEXT("Risk samples request exact fallback"),
		LNPEnemySurfaceMovement::TryResolveCachedGround(Snapshot, Params, Target, Handle, GroundedLocation));
	return true;
}

bool FLNPEnemyDynamicSupportContactTest::RunTest(const FString& Parameters)
{
	const FGuid MarkerId(1, 2, 3, 4);
	FLNPExactHitIdentity Identity;
	Identity.Lifetime = ELNPExactSourceLifetime::Dynamic;
	Identity.Roles = ELNPExactSourceRole::Support | ELNPExactSourceRole::Blocker;
	Identity.Slot = 5;
	Identity.MarkerId = MarkerId;

	FLNPDynamicSupportFrame Frame;
	FLNPDynamicSupportSnapshot& Support = Frame.Supports.AddDefaulted_GetRef();
	Support.Id.Slot = 5;
	Support.Id.MarkerId = MarkerId;
	Support.PreviousTransform = FTransform(FQuat::Identity, FVector(100.f, 0.f, 0.f));
	Support.CurrentTransform = FTransform(FQuat::Identity, FVector(130.f, 20.f, 0.f));
	Support.LinearVelocity = FVector(300.f, 200.f, 0.f);
	Support.bWalkable = true;

	const FVector InitialCenter(140.f, 30.f, 50.f);
	FLNPEnemyDynamicSupportContact Contact;
	TestTrue(TEXT("Dynamic Support identity creates a contact"),
		LNPEnemySurfaceMovement::MakeDynamicSupportContact(Identity, Frame, InitialCenter, Contact));
	TestTrue(TEXT("Dynamic contact is valid"), Contact.IsValid());
	TestTrue(TEXT("Dynamic contact stores exact placement id"), Contact.SupportId == Support.Id);
	TestTrue(TEXT("Dynamic contact stores panel velocity"), Contact.LastLinearVelocity.Equals(Support.LinearVelocity));

	// 다음 프레임에 패널이 +40 X 이동하고 90도 회전한 것으로 만든다. 저장한 로컬 중심과 회전 delta를 모두 적용해야 한다.
	Support.PreviousTransform = Support.CurrentTransform;
	Support.CurrentTransform = FTransform(FQuat(FVector::UpVector, HALF_PI), FVector(170.f, 20.f, 0.f));
	Support.LinearVelocity = FVector(400.f, 0.f, 0.f);
	FTransform EnemyTransform(FQuat::Identity, InitialCenter);
	TestTrue(TEXT("Current panel pose carries the contact"),
		LNPEnemySurfaceMovement::ApplyDynamicSupportDelta(Frame, Contact, EnemyTransform));
	const FVector ExpectedCenter = Support.CurrentTransform.TransformPositionNoScale(Contact.LocalCapsuleCenter);
	TestTrue(TEXT("Carried capsule center follows full panel transform"), EnemyTransform.GetLocation().Equals(ExpectedCenter));
	const FVector ExpectedForward = FVector::YAxisVector;
	TestTrue(TEXT("Carried orientation follows panel rotation delta"),
		EnemyTransform.GetRotation().GetForwardVector().Equals(ExpectedForward, 0.001f));
	TestTrue(TEXT("Departure velocity tracks the latest panel frame"), Contact.LastLinearVelocity.Equals(Support.LinearVelocity));

	Frame.Supports.Reset();
	TestFalse(TEXT("Missing panel rejects contact carry"),
		LNPEnemySurfaceMovement::ApplyDynamicSupportDelta(Frame, Contact, EnemyTransform));
	TestTrue(TEXT("Rejected carry preserves departure velocity"), Contact.LastLinearVelocity.Equals(FVector(400.f, 0.f, 0.f)));

	Identity.Lifetime = ELNPExactSourceLifetime::Static;
	TestFalse(TEXT("Static identity cannot create a dynamic contact"),
		LNPEnemySurfaceMovement::MakeDynamicSupportContact(Identity, Frame, InitialCenter, Contact));
	return true;
}

bool FLNPEnemyActorHandoffTest::RunTest(const FString& Parameters)
{
	FLNPDynamicSupportFrame Frame;
	FLNPDynamicSupportSnapshot& Support = Frame.Supports.AddDefaulted_GetRef();
	Support.Id.Slot = 2;
	Support.Id.MarkerId = FGuid(4, 3, 2, 1);
	Support.CurrentTransform = FTransform(FVector(500.f, 0.f, 0.f));
	Support.PreviousTransform = Support.CurrentTransform;
	Support.LinearVelocity = FVector(120.f, 30.f, 0.f);
	Support.bWalkable = true;

	FLNPSurfaceHandle Surface;
	Surface.OctantSlot = 1;
	Surface.LocalLayerId = 6;
	Surface.Generation = 9;
	FLNPEnemyDynamicSupportContact Contact;
	FVector Velocity = FVector::ZeroVector;

	// Mover가 아직 floor blackboard를 게시하지 않은 활성화 첫 프레임에는 Entity의 정적 정체성을 유지한다.
	LNPEnemySurfaceMovement::UpdateActorHandoff(
		false, FVector::ZeroVector, nullptr, Frame, FVector::ZeroVector, Velocity, Surface, Contact);
	TestTrue(TEXT("Grounded handoff without a floor hit preserves the previous handle"), Surface.IsValid());
	TestTrue(TEXT("Grounded actor exports zero physical velocity"), Velocity.IsNearlyZero());

	FLNPExactHitIdentity StaticFloor;
	StaticFloor.Lifetime = ELNPExactSourceLifetime::Static;
	StaticFloor.Roles = ELNPExactSourceRole::Support;
	StaticFloor.Slot = 3;
	StaticFloor.LocalLayerId = 7;
	StaticFloor.SurfaceDataGeneration = 11;
	LNPEnemySurfaceMovement::UpdateActorHandoff(
		false, FVector::ZeroVector, &StaticFloor, Frame, FVector::ZeroVector, Velocity, Surface, Contact);
	TestEqual(TEXT("Static Mover floor replaces the slot"), Surface.OctantSlot, static_cast<uint16>(3));
	TestEqual(TEXT("Static Mover floor replaces the Layer"), Surface.LocalLayerId, static_cast<uint16>(7));
	TestFalse(TEXT("Static Mover floor clears dynamic contact"), Contact.IsValid());

	FLNPExactHitIdentity DynamicFloor;
	DynamicFloor.Lifetime = ELNPExactSourceLifetime::Dynamic;
	DynamicFloor.Roles = ELNPExactSourceRole::Support;
	DynamicFloor.Slot = Support.Id.Slot;
	DynamicFloor.MarkerId = Support.Id.MarkerId;
	const FVector CapsuleCenter(510.f, 20.f, 80.f);
	LNPEnemySurfaceMovement::UpdateActorHandoff(
		false, FVector::ZeroVector, &DynamicFloor, Frame, CapsuleCenter, Velocity, Surface, Contact);
	TestFalse(TEXT("Dynamic Mover floor clears the static handle"), Surface.IsValid());
	TestTrue(TEXT("Dynamic Mover floor creates a PureEntity contact"), Contact.IsValid());

	const FVector AirborneVelocity(25.f, -50.f, 75.f);
	LNPEnemySurfaceMovement::UpdateActorHandoff(
		true, AirborneVelocity, nullptr, Frame, CapsuleCenter, Velocity, Surface, Contact);
	TestTrue(TEXT("Airborne Mover velocity transfers exactly"), Velocity.Equals(AirborneVelocity));
	TestFalse(TEXT("Airborne handoff invalidates static handle"), Surface.IsValid());
	TestFalse(TEXT("Airborne handoff invalidates dynamic contact"), Contact.IsValid());
	return true;
}

#endif
