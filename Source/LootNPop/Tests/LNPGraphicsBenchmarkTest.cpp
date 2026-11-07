// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Graphics/LNPGraphicsBenchmark.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "Dom/JsonObject.h"
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
		*(FPaths::ProjectDir() / TEXT("Scripts/Profiling/Graphics/AcropolisViewpoints.json"))))) return false;
	TArray<FTransform> Views;
	float FOV = 0, Aspect = 0;
	if (!TestTrue(TEXT("Captured artifact is valid"), ULNPGraphicsBenchmarkSubsystem::ReadViewpoints(Json, Views, FOV, Aspect))) return false;
	const FTransform Slot(ULNPOctantSpawnSubsystem::OctantRotations[6]);
	const auto WorldView = [&](double Phase) { return ULNPGraphicsBenchmarkSubsystem::EvaluateView(Views, Phase) * Slot; };
	TestTrue(TEXT("First point reproduces the manually captured world camera"),
		WorldView(0).GetLocation().Equals(FVector(18151.953409770336, -18394.83929964723, -18413.831631335015), 0.001));
	TestTrue(TEXT("First point reproduces captured roll under spherical gravity"),
		WorldView(0).GetRotation().Equals(FRotator(34.83026360721428, 62.27101613823391, 66.71982922796639).Quaternion(), 1.e-6));
	TestTrue(TEXT("Upper viewpoint uses player origin plus 50cm forward, remapped from slot 2 to slot 6"),
		WorldView(1.0 / 3.0).GetLocation().Equals(FVector(10513.431709385082, -7587.938127701423, -10842.515507489785), 0.001));
	TestTrue(TEXT("Close viewpoint is remapped to fixed slot 6"),
		WorldView(2.0 / 3.0).GetLocation().Equals(FVector(7664.860253690804, -15426.92606323785, -12466.216059413013), 0.001));
	const FTransform CaptureSlot(FRotator(0, 180, 0));
	TestTrue(TEXT("Upper viewpoint preserves captured rotation after player-origin position adjustment"),
		WorldView(1.0 / 3.0).GetRotation().Equals(Slot.TransformRotation(CaptureSlot.InverseTransformRotation(
			FRotator(5.8777411958635835, -70.3747504280153, 129.91180080236575).Quaternion())), 1.e-6));
	TestTrue(TEXT("Close viewpoint preserves captured rotation"),
		WorldView(2.0 / 3.0).GetRotation().Equals(Slot.TransformRotation(CaptureSlot.InverseTransformRotation(
			FRotator(59.92685263138905, 15.701050913785787, 141.92552668350538).Quaternion())), 1.e-6));
	TestTrue(TEXT("Loop returns to its initial pose"), WorldView(1).Equals(WorldView(0), 1.e-6));
	TestTrue(TEXT("Interpolated orientation remains normalized"), WorldView(0.17).GetRotation().IsNormalized());
	TestTrue(TEXT("Captured FOV is preserved"), FMath::IsNearlyEqual(FOV, 80.f, 0.001f));
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	Root->SetStringField(TEXT("coordinate_space"), TEXT("world"));
	FString Invalid;
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
