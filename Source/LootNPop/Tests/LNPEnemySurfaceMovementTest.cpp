// Copyright (c) 2026 LootNPop. All rights reserved.

#if WITH_AUTOMATION_TESTS

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

	Identity.Roles = ELNPExactSourceRole::Blocker;
	TestFalse(TEXT("Blocker-only identity does not resolve"), LNPEnemySurfaceMovement::MakeSurfaceHandle(Identity, Handle));
	TestFalse(TEXT("Rejected handle is invalid"), Handle.IsValid());

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

#endif
