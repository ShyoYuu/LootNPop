// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Graphics/LNPGraphicsBenchmark.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/EngineTypes.h"
#include "Engine/LocalPlayer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPGraphicsPathTest, "LootNPop.Graphics.Benchmark.Viewpoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPGraphicsPathTest::RunTest(const FString& Parameters)
{
	FString Json;
	if (!TestTrue(TEXT("Viewpoint artifact exists"), FFileHelper::LoadFileToString(Json,
		*(FPaths::ProjectDir() / TEXT("Scripts/Profiling/Graphics/RidgeViewpoints.json"))))) return false;
	TArray<FTransform> Views;
	float FOV = 0, Aspect = 0;
	if (!TestTrue(TEXT("Captured artifact is valid"), ULNPGraphicsBenchmarkSubsystem::ReadViewpoints(Json, Views, FOV, Aspect))) return false;
	const FTransform Slot(ULNPOctantSpawnSubsystem::OctantRotations[1]);
	const auto WorldView = [&](double Phase) { return ULNPGraphicsBenchmarkSubsystem::EvaluateView(Views, Phase) * Slot; };
	TestTrue(TEXT("First point reproduces the manually captured world camera"),
		WorldView(0).GetLocation().Equals(FVector(-11981.177118997784, 14126.49257945583, 25578.3334521097), 0.001));
	TestTrue(TEXT("First point reproduces captured roll under spherical gravity"),
		WorldView(0).GetRotation().Equals(FRotator(-48.679422263353544, 145.7622689387656, 170.8183138073181).Quaternion(), 1.e-6));
	const FVector CapturedTablelandCamera(-15446.863503344848, 11128.920084385212, 20202.150679161663);
	TestTrue(TEXT("Tableland viewpoint adds 50cm spherical clearance to the captured camera"),
		WorldView(1.0 / 3.0).GetLocation().Equals(
			CapturedTablelandCamera - CapturedTablelandCamera.GetSafeNormal() * 50.0, 0.001));
	TestTrue(TEXT("Valley viewpoint reproduces the manually captured world camera"),
		WorldView(2.0 / 3.0).GetLocation().Equals(FVector(-15391.090894585082, 13900.145215414032, 24604.96972345391), 0.001));
	TestTrue(TEXT("Tableland viewpoint preserves captured rotation"),
		WorldView(1.0 / 3.0).GetRotation().Equals(
			FRotator(-4.408533680505686, 97.23797620202804, -146.54353526453198).Quaternion(), 1.e-6));
	TestTrue(TEXT("Valley viewpoint preserves captured rotation"),
		WorldView(2.0 / 3.0).GetRotation().Equals(
			FRotator(-39.710038500192205, 99.76550123823205, -156.3474421332819).Quaternion(), 1.e-6));
	TestTrue(TEXT("Loop returns to its initial pose"), WorldView(1).Equals(WorldView(0), 1.e-6));
	TestTrue(TEXT("Interpolated orientation remains normalized"), WorldView(0.17).GetRotation().IsNormalized());
	TestTrue(TEXT("Captured FOV is preserved"), FMath::IsNearlyEqual(FOV, 80.f, 0.001f));
	TestTrue(TEXT("Captured reference aspect ratio is preserved"), FMath::IsNearlyEqual(Aspect, 1.3331547975540161f, 1.e-6f));
	TestEqual(TEXT("Default local player uses the benchmark vertical FOV constraint"),
		GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint.GetValue(), AspectRatio_MaintainYFOV);
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	FString Invalid;
	Root->SetNumberField(TEXT("target_slot"), 6);
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Invalid));
	TestFalse(TEXT("Previous benchmark slot cannot be silently reused"),
		ULNPGraphicsBenchmarkSubsystem::ReadViewpoints(Invalid, Views, FOV, Aspect));
	Root->SetNumberField(TEXT("target_slot"), 1);
	Root->SetStringField(TEXT("octant_asset"), TEXT("/Game/Maps/Acropolis_00/LVI_Octant_Acropolis_00"));
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Invalid));
	TestFalse(TEXT("Previous benchmark octant cannot be silently reused"),
		ULNPGraphicsBenchmarkSubsystem::ReadViewpoints(Invalid, Views, FOV, Aspect));
	Root->SetStringField(TEXT("octant_asset"), TEXT("/Game/Maps/Ridge_00/LVI_Octant_Ridge_00"));
	Root->SetStringField(TEXT("coordinate_space"), TEXT("world"));
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Invalid));
	TestFalse(TEXT("World coordinates cannot be silently used as local coordinates"),
		ULNPGraphicsBenchmarkSubsystem::ReadViewpoints(Invalid, Views, FOV, Aspect));
	Root->SetStringField(TEXT("coordinate_space"), TEXT("octant_local"));
	Root->GetArrayField(TEXT("points"))[0]->AsObject()->GetObjectField(TEXT("camera"))->SetArrayField(TEXT("rotation_quaternion_xyzw"),
		{MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0)});
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Invalid));
	TestFalse(TEXT("Invalid quaternion is rejected before camera creation"),
		ULNPGraphicsBenchmarkSubsystem::ReadViewpoints(Invalid, Views, FOV, Aspect));
	return !HasAnyErrors();
}

#endif
