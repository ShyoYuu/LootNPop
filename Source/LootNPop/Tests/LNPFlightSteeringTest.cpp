// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Enemy/LNPFlightSteering.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPFlightSteeringLookaheadTest,
	"LootNPop.SurfaceNavigation.FlightSteering.LookaheadStop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	using namespace LNPFlightSteering;

	constexpr float TestDeltaTime = 1.f / 60.f;
	constexpr float TestSpeed = 900.f;

	/** 비행 steering 테스트 월드. 판은 기본 Cube(100cm)를 Scale로 늘린다. */
	struct FFlightWorld
	{
		UWorld* World = nullptr;
		ULNPHitIdentitySubsystem* HitIdentity = nullptr;
		ULNPMassWorldCollisionSubsystem* Collision = nullptr;
		FParams Params;

		explicit FFlightWorld(const TCHAR* Name)
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, Name);
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);
			HitIdentity = World->GetSubsystem<ULNPHitIdentitySubsystem>();
			Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();

			Params.BodyRadius = 80.f;
			Params.Clearance = 50.f;
			Params.LookaheadTime = 1.f;
		}

		~FFlightWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		void Slab(const FVector& Location, const FVector& Scale, const FName Profile, const bool bRegister = true)
		{
			AActor* Owner = World->SpawnActor<AActor>();
			UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Owner);
			Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Mesh->SetMobility(EComponentMobility::Static);
			Mesh->SetCollisionProfileName(Profile);
			Mesh->SetWorldLocation(Location);
			Mesh->SetWorldScale3D(Scale);
			Owner->SetRootComponent(Mesh);
			Mesh->RegisterComponent();
			if (bRegister)
			{
				HitIdentity->RegisterRuntimeSource(Mesh);
			}
		}

		void Publish() const { HitIdentity->Tick(0.f); }

		float SweepRadius() const { return Params.BodyRadius + Params.Clearance; }
	};

	struct FFlightTrace
	{
		FVector Location = FVector::ZeroVector;
		double MaxX = -UE_BIG_NUMBER;
		int32 BlockedFrames = 0;
	};

	/** 비행 이동 프로세서와 같은 흐름(도착 속도 → Step)으로 목표점을 향해 난다. */
	FFlightTrace FlyToward(const FFlightWorld& Fixture, const FVector& Start, const FVector& Goal, const int32 Frames)
	{
		FFlightTrace Trace;
		Trace.Location = Start;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			const FVector Desired = ComputeArrivalVelocity(Trace.Location, Goal, TestSpeed, TestDeltaTime);
			FVector Next;
			Trace.BlockedFrames += Step(*Fixture.Collision, Fixture.Params, Trace.Location, Desired, TestDeltaTime, Next) == EStepResult::Blocked ? 1 : 0;
			Trace.Location = Next;
			Trace.MaxX = FMath::Max(Trace.MaxX, Next.X);
		}
		return Trace;
	}
}

bool FLNPFlightSteeringLookaheadTest::RunTest(const FString& Parameters)
{
	FFlightWorld Fixture(TEXT("LNPFlightSteeringLookaheadTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 바닥 top z=10. 벽 x∈[990,1010], y∈[-500,500], z∈[10,1010]. 등록하지 않은(Unknown) 벽은 y=3000 줄.
	Fixture.Slab(FVector(0, 0, 0), FVector(60, 60, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Slab(FVector(1000, 0, 510), FVector(0.2, 10, 10), TEXT("LNPStaticBlocker"));
	Fixture.Slab(FVector(1000, 3000, 510), FVector(0.2, 10, 10), TEXT("LNPStaticBlocker"), /*bRegister=*/false);
	Fixture.Publish();

	const double WallFace = 990.0;
	const double StopX = WallFace - Fixture.SweepRadius();

	// 1. 빈 공간: 목표에 넘치지 않고 도착한다. 막힘은 한 번도 없다.
	{
		const FFlightTrace Trace = FlyToward(Fixture, FVector(0, -2000, 500), FVector(800, -2000, 700), 120);
		TestTrue(TEXT("Reaches the goal in open space"), FVector::Dist(Trace.Location, FVector(800, -2000, 700)) < 1.0);
		TestEqual(TEXT("Open space is never blocked"), Trace.BlockedFrames, 0);
	}

	// 2. 벽 정면: 여유(Clearance)를 남기고 벽 앞에 선다. 어느 프레임에도 여유 구가 벽을 넘지 않는다.
	{
		const FFlightTrace Trace = FlyToward(Fixture, FVector(0, 0, 500), FVector(2000, 0, 500), 180);
		TestTrue(TEXT("Lookahead reports the wall"), Trace.BlockedFrames > 0);
		TestTrue(TEXT("Never crosses the clearance line"), Trace.MaxX <= StopX + 0.1);
		TestTrue(TEXT("Stops close to the clearance line"), Trace.Location.X >= StopX - 5.0);
	}

	// 3. 여유 안에서 시작: 전진하지 않고 벽 반대쪽으로 풀려난다.
	{
		const FVector Start(StopX + 20.0, 0, 500);
		FVector Next;
		const EStepResult Result = Step(*Fixture.Collision, Fixture.Params, Start, FVector(TestSpeed, 0, 0), TestDeltaTime, Next);
		TestTrue(TEXT("Starting inside the clearance is blocked"), Result == EStepResult::Blocked);
		TestTrue(TEXT("Depenetrates away from the wall"), Next.X < Start.X);
	}

	// 4. Unknown 벽도 막힘이다(D-037).
	{
		const FFlightTrace Trace = FlyToward(Fixture, FVector(0, 3000, 500), FVector(2000, 3000, 500), 180);
		TestTrue(TEXT("Unknown wall blocks flight"), Trace.MaxX <= StopX + 0.1);
	}

	// 5. 호버는 query를 쓰지 않는다.
	{
		const uint64 Before = Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::FlightSteering);
		FVector Next;
		const EStepResult Result = Step(*Fixture.Collision, Fixture.Params, FVector(0, 0, 500), FVector::ZeroVector, TestDeltaTime, Next);
		TestTrue(TEXT("Zero velocity hovers"), Result == EStepResult::Hover);
		TestEqual(TEXT("Hover issues no query"), Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::FlightSteering), Before);
		TestTrue(TEXT("Hover stays in place"), Next.Equals(FVector(0, 0, 500)));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
