// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DynamicTerrain/LNPMovingPanel.h"
#include "DynamicTerrain/LNPPlacementMarker.h"
#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameLogic/LNPOctantSpawnSubsystem.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPMovingPanelGravityTest,
	"LootNPop.DynamicTerrain.MovingPanel.GravityRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPMovingPanelGravityTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MovingPanelGravityTest"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		const FQuat SlotRotation = ULNPOctantSpawnSubsystem::OctantRotations[Slot].Quaternion();
		const FVector A = SlotRotation.RotateVector(FVector(18000,12000,8000));
		const FVector B = SlotRotation.RotateVector(FVector(11000,19000,7000));
		const FQuat Frame = FRotationMatrix::MakeFromZY(-((A+B)*0.5).GetSafeNormal(),SlotRotation.GetAxisY()).ToQuat();
		ALNPPlacementMarker* Marker = World->SpawnActor<ALNPPlacementMarker>((A+B)*0.5,Frame.Rotator());
		Marker->PathSpeed = 600.0f;
		Marker->PathEndHoldSeconds = 3.0f;
		Marker->Path->ClearSplinePoints(false);
		Marker->Path->AddSplinePoint(A,ESplineCoordinateSpace::World,false);
		Marker->Path->AddSplinePoint(B,ESplineCoordinateSpace::World,false);
		Marker->Path->SetSplinePointType(0,ESplinePointType::Linear,false);
		Marker->Path->SetSplinePointType(1,ESplinePointType::Linear,false);
		Marker->Path->UpdateSpline();
		auto Spawn = [&]()
		{
			ALNPMovingPanel* Panel = CastChecked<ALNPMovingPanel>(ULNPDynamicTerrainSubsystem::SpawnPlacedActor(
				*World, ALNPMovingPanel::StaticClass(), Marker->GetActorTransform(), [&](AActor& Actor)
				{ CastChecked<ALNPMovingPanel>(&Actor)->InitializeFromMarker(*Marker,{static_cast<int8>(Slot),FGuid(1,0,0,1)}); }));
			Panel->DispatchBeginPlay();
			return Panel;
		};
		ALNPMovingPanel* Panel = Spawn();
		const double Start = World->GetTimeSeconds(), Travel = FVector::Distance(A,B)/600.0;
		for (int32 Step = 0; Step <= 10; ++Step)
		{
			const double Alpha = Step/10.0;
			Panel->UpdatePose(Start+Travel*Alpha);
			TestTrue(TEXT("Panel follows its original straight path"), Panel->GetActorLocation().Equals(FMath::Lerp(A,B,Alpha),0.1));
			const FVector ExpectedUp = FQuat::Slerp(FQuat::Identity,FQuat::FindBetweenNormals(-A.GetSafeNormal(),-B.GetSafeNormal()),Alpha)
				.RotateVector(-A.GetSafeNormal());
			TestTrue(TEXT("Panel tilt interpolates endpoint gravity with no extra twist"), Panel->GetActorUpVector().Equals(ExpectedUp,1e-5));
		}
		Panel->UpdatePose(Start+Travel+0.1);
		TestTrue(TEXT("Arrival is level and stops both velocities"), Panel->GetActorUpVector().Equals(-B.GetSafeNormal(),1e-5)
			&& Panel->GetLinearVelocity().IsNearlyZero() && Panel->GetAngularVelocity().IsNearlyZero());
		Panel->UpdatePose(Start+Travel*0.5);
		const FTransform Midpoint = Panel->GetActorTransform();
		const FVector ForwardAngular = Panel->GetAngularVelocity();
		TestTrue(TEXT("Rotating base publishes angular velocity to Mover physics"), !ForwardAngular.IsNearlyZero()
			&& Panel->GetPanelMesh()->GetPhysicsAngularVelocityInRadians().Equals(ForwardAngular,1e-4));
		// float 스플라인 키의 차분 오차를 줄여 가장자리 접촉점 속도와 v + omega cross r을 비교한다.
		const FVector LocalContact(100,100,15);
		const FVector Contact = Midpoint.TransformPosition(LocalContact);
		const FVector Velocity = Panel->GetLinearVelocity()+FVector::CrossProduct(ForwardAngular,Contact-Midpoint.GetLocation());
		Panel->UpdatePose(Start+Travel*0.5-0.01);
		const FVector BeforeContact = Panel->GetActorTransform().TransformPosition(LocalContact);
		Panel->UpdatePose(Start+Travel*0.5+0.01);
		const FVector MeasuredVelocity = (Panel->GetActorTransform().TransformPosition(LocalContact)-BeforeContact)/0.02;
		TestTrue(FString::Printf(TEXT("Edge contact velocity includes rotation (error=%.4fcm/s)"),
			FVector::Distance(MeasuredVelocity,Velocity)),MeasuredVelocity.Equals(Velocity,0.1));
		Panel->UpdatePose(Start+Travel+3.0+Travel*0.5);
		TestTrue(TEXT("Return reproduces midpoint and reverses angular velocity"), Panel->GetActorTransform().Equals(Midpoint,0.01)
			&& Panel->GetAngularVelocity().Equals(-ForwardAngular,1e-5));
		ALNPMovingPanel* Late = Spawn();
		Late->UpdatePose(Start+Travel*0.5);
		TestTrue(TEXT("Fresh path receiver reconstructs midpoint without integration history"), Late->GetActorTransform().Equals(Midpoint,0.01));
		Panel->UpdatePose(Start+Travel*2.0+3.1);
		TestTrue(TEXT("Departure is level and stops both velocities"), Panel->GetActorUpVector().Equals(-A.GetSafeNormal(),1e-5)
			&& Panel->GetLinearVelocity().IsNearlyZero() && Panel->GetAngularVelocity().IsNearlyZero());
		Panel->Destroy();
		Late->Destroy();
		Marker->Destroy();
	}
	return !HasAnyErrors();
}

#endif
