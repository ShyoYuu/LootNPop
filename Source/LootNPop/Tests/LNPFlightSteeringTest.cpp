// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Enemy/LNPFlightSteering.h"
#include "Enemy/LNPEnemyLineOfSight.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPFlightSteeringLookaheadTest,
	"LootNPop.SurfaceNavigation.FlightSteering.LookaheadStop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPEnemyLineOfSightGateTest,
	"LootNPop.SurfaceNavigation.EnemyLineOfSight.Gate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPFlightSteeringDetourTest,
	"LootNPop.SurfaceNavigation.FlightSteering.DetourAndStuck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	// 유니티 빌드에서 다른 테스트 파일의 익명 네임스페이스와 합쳐진다. using-directive를 두지 않고 LNPFlightSteering을 한정해 쓴다.

	constexpr float FlightTestDeltaTime = 1.f / 60.f;
	constexpr float TestSpeed = 900.f;

	/** 비행 steering 테스트 월드. 판은 기본 Cube(100cm)를 Scale로 늘린다. */
	struct FFlightWorld
	{
		UWorld* World = nullptr;
		ULNPHitIdentitySubsystem* HitIdentity = nullptr;
		ULNPMassWorldCollisionSubsystem* Collision = nullptr;
		LNPFlightSteering::FParams Params;

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
			const FVector Desired = LNPFlightSteering::ComputeArrivalVelocity(Trace.Location, Goal, TestSpeed, FlightTestDeltaTime);
			FVector Next;
			Trace.BlockedFrames += LNPFlightSteering::Step(*Fixture.Collision, Fixture.Params, Trace.Location, Desired, FlightTestDeltaTime, Next) == LNPFlightSteering::EStepResult::Blocked ? 1 : 0;
			Trace.Location = Next;
			Trace.MaxX = FMath::Max(Trace.MaxX, Next.X);
		}
		return Trace;
	}

	/** 점과 축 정렬 상자 사이 거리. 상자 안이면 0. */
	double DistanceToBox(const FVector& Point, const FBox& Box)
	{
		return FMath::Sqrt(Box.ComputeSquaredDistanceToPoint(Point));
	}

	struct FSteerTrace
	{
		FVector Location = FVector::ZeroVector;
		double MinBoxDistance = UE_BIG_NUMBER;
		int32 StuckFrame = INDEX_NONE;
		int32 ArrivedFrame = INDEX_NONE;
	};

	/** 비행 이동 프로세서와 같은 흐름(Steer, 분리력 없음)으로 목표점을 향해 난다. Stuck이면 멈춘다. */
	FSteerTrace SteerToward(const FFlightWorld& Fixture, const FVector& Start, const FVector& Goal, const FBox& Obstacle, const int32 Frames)
	{
		FSteerTrace Trace;
		Trace.Location = Start;
		LNPFlightSteering::FSteeringState State;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			FVector Next;
			const LNPFlightSteering::EStepResult Result = LNPFlightSteering::Steer(*Fixture.Collision, Fixture.Params, Trace.Location, FVector::UpVector, Goal, TestSpeed,
				FVector::ZeroVector, FlightTestDeltaTime, State, Next);
			Trace.Location = Next;
			Trace.MinBoxDistance = FMath::Min(Trace.MinBoxDistance, DistanceToBox(Next, Obstacle));
			if (Result == LNPFlightSteering::EStepResult::Stuck)
			{
				Trace.StuckFrame = Frame;
				break;
			}
			if (Trace.ArrivedFrame == INDEX_NONE && FVector::Dist(Next, Goal) < 1.0)
			{
				Trace.ArrivedFrame = Frame;
				break;
			}
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
		const LNPFlightSteering::EStepResult Result = LNPFlightSteering::Step(*Fixture.Collision, Fixture.Params, Start, FVector(TestSpeed, 0, 0), FlightTestDeltaTime, Next);
		TestTrue(TEXT("Starting inside the clearance is blocked"), Result == LNPFlightSteering::EStepResult::Blocked);
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
		const LNPFlightSteering::EStepResult Result = LNPFlightSteering::Step(*Fixture.Collision, Fixture.Params, FVector(0, 0, 500), FVector::ZeroVector, FlightTestDeltaTime, Next);
		TestTrue(TEXT("Zero velocity hovers"), Result == LNPFlightSteering::EStepResult::Hover);
		TestEqual(TEXT("Hover issues no query"), Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::FlightSteering), Before);
		TestTrue(TEXT("Hover stays in place"), Next.Equals(FVector(0, 0, 500)));
	}
	return true;
}

bool FLNPFlightSteeringDetourTest::RunTest(const FString& Parameters)
{
	FFlightWorld Fixture(TEXT("LNPFlightSteeringDetourTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 바닥 top z=10. 섬 크기 블록 x·y∈[-600,600], z∈[300,700] — 비행 고도 500이 블록 한가운데를 지난다.
	// 두 번째 블록은 y=5000 줄, 목표가 그 안에 있는 닿을 수 없는 배치.
	Fixture.Slab(FVector(0, 0, 0), FVector(120, 120, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Slab(FVector(0, 0, 500), FVector(12, 12, 4), TEXT("LNPStaticTerrain"));
	Fixture.Slab(FVector(0, 5000, 500), FVector(12, 12, 4), TEXT("LNPStaticTerrain"));
	Fixture.Publish();

	const FBox Island(FVector(-600, -600, 300), FVector(600, 600, 700));
	const FBox Solid(FVector(-600, 4400, 300), FVector(600, 5600, 700));

	// 1. 우회: 블록을 돌아 반대편 목표에 도착하고, 몸이 블록에 닿지 않는다.
	{
		const FSteerTrace Trace = SteerToward(Fixture, FVector(-2500, 0, 500), FVector(2500, 0, 500), Island, 60 * 30);
		TestTrue(TEXT("Detours around the island block and arrives"), Trace.ArrivedFrame != INDEX_NONE);
		TestTrue(TEXT("Never touches the island block"), Trace.MinBoxDistance >= Fixture.Params.BodyRadius - 1.0);
		TestEqual(TEXT("Detour is not reported as stuck"), Trace.StuckFrame, static_cast<int32>(INDEX_NONE));
	}

	// 2. 포기: 목표가 블록 속이면 교착 복구 끝에 Stuck을 돌려주고, 그동안 블록을 관통하지 않는다.
	{
		const FSteerTrace Trace = SteerToward(Fixture, FVector(-2500, 5000, 500), FVector(0, 5000, 500), Solid, 60 * 30);
		TestTrue(TEXT("Unreachable goal ends in Stuck"), Trace.StuckFrame != INDEX_NONE);
		TestTrue(TEXT("Never enters the solid block"), Trace.MinBoxDistance >= Fixture.Params.BodyRadius - 1.0);
	}
	return true;
}

bool FLNPEnemyLineOfSightGateTest::RunTest(const FString& Parameters)
{
	FFlightWorld Fixture(TEXT("LNPEnemyLineOfSightGateTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 섬 밑면처럼 가로놓인 판 z∈[480,520], x·y∈[-500,500]. 등록하지 않은(Unknown) 판은 y=3000 줄.
	Fixture.Slab(FVector(0, 0, 500), FVector(10, 10, 0.4), TEXT("LNPStaticTerrain"));
	Fixture.Slab(FVector(0, 3000, 500), FVector(10, 10, 0.4), TEXT("LNPStaticTerrain"), /*bRegister=*/false);
	Fixture.Publish();

	const uint64 Before = Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::EnemyLineOfSight);
	TestFalse(TEXT("Shot from under the island to its top is blocked"),
		LNPEnemyLineOfSight::HasClearShot(*Fixture.Collision, FVector(0, 0, 200), FVector(0, 0, 800)));
	TestTrue(TEXT("Shot beside the island is clear"),
		LNPEnemyLineOfSight::HasClearShot(*Fixture.Collision, FVector(800, 0, 200), FVector(800, 0, 800)));
	TestFalse(TEXT("Unknown geometry also blocks the shot"),
		LNPEnemyLineOfSight::HasClearShot(*Fixture.Collision, FVector(0, 3000, 200), FVector(0, 3000, 800)));
	TestEqual(TEXT("Each gate check is one EnemyLineOfSight query"),
		Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::EnemyLineOfSight) - Before, static_cast<uint64>(3));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
