// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPExactMovementGroundTest,
	"LootNPop.SurfaceNavigation.ExactMovement.GroundAndCliff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPExactMovementBlockTest,
	"LootNPop.SurfaceNavigation.ExactMovement.WallAndSlope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPExactMovementAirborneTest,
	"LootNPop.SurfaceNavigation.ExactMovement.IslandBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPExactMovementCaveTest,
	"LootNPop.SurfaceNavigation.ExactMovement.CaveAndUnknown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	using namespace LNPEnemyExactMovement;

	constexpr float TestDeltaTime = 1.f / 60.f;
	constexpr float TestHalfHeight = 88.f;
	constexpr float TestRadius = 35.f;

	/**
	 * 평평한 테스트 월드. 구 중심을 +Z 멀리 두어 Up이 사실상 +Z인 내부형 구를 흉내 낸다.
	 * 판은 기본 Cube(100cm)를 Scale로 늘린다.
	 */
	struct FExactMovementWorld
	{
		UWorld* World = nullptr;
		ULNPHitIdentitySubsystem* HitIdentity = nullptr;
		ULNPMassWorldCollisionSubsystem* Collision = nullptr;
		FParams Params;

		explicit FExactMovementWorld(const TCHAR* Name)
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, Name);
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);
			HitIdentity = World->GetSubsystem<ULNPHitIdentitySubsystem>();
			Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();

			Params.GravityOrigin = FVector(0, 0, 1.0e7);
			Params.CapsuleRadius = TestRadius;
			Params.CapsuleHalfHeight = TestHalfHeight;
		}

		~FExactMovementWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		UStaticMeshComponent* Slab(const FVector& Location, const FVector& Scale, const FName Profile,
			const FRotator& Rotation = FRotator::ZeroRotator, const bool bRegister = true)
		{
			AActor* Owner = World->SpawnActor<AActor>();
			UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Owner);
			Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Mesh->SetMobility(EComponentMobility::Static);
			Mesh->SetCollisionProfileName(Profile);
			Mesh->SetWorldLocationAndRotation(Location, Rotation);
			Mesh->SetWorldScale3D(Scale);
			Owner->SetRootComponent(Mesh);
			Mesh->RegisterComponent();
			if (bRegister)
			{
				HitIdentity->RegisterRuntimeSource(Mesh);
			}
			return Mesh;
		}

		/** 판을 모두 놓은 뒤 identity snapshot을 게시한다. */
		void Publish() const { HitIdentity->Tick(0.f); }
	};

	/** 이동 프로세서와 같은 규약(속도 0 = 접지)으로 프레임을 돌린 결과. */
	struct FTrace
	{
		FVector Location = FVector::ZeroVector;
		FVector PhysVelocity = FVector::ZeroVector;
		int32 LostSupportFrame = INDEX_NONE;
		FVector LostSupportLocation = FVector::ZeroVector;
		int32 Landings = 0;
		int32 Rejections = 0;
		double MaxZ = -UE_BIG_NUMBER;
		double MinZ = UE_BIG_NUMBER;
	};

	FTrace Simulate(const FExactMovementWorld& Fixture, const FVector& Start, const FVector& StartPhysVelocity,
		const FVector& WalkVelocity, const int32 Frames)
	{
		FTrace Trace;
		Trace.Location = Start;
		Trace.PhysVelocity = StartPhysVelocity;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			FVector Next;
			if (!Trace.PhysVelocity.IsNearlyZero())
			{
				Trace.Landings += StepAirborne(*Fixture.Collision, Fixture.Params, Trace.Location, Trace.PhysVelocity, TestDeltaTime, Next) ? 1 : 0;
			}
			else
			{
				const EGroundResult Result = StepGrounded(*Fixture.Collision, Fixture.Params, Trace.Location, WalkVelocity, TestDeltaTime, Next, Trace.PhysVelocity);
				if (Result == EGroundResult::LostSupport && Trace.LostSupportFrame == INDEX_NONE)
				{
					Trace.LostSupportFrame = Frame;
					Trace.LostSupportLocation = Next;
				}
				Trace.Rejections += Result == EGroundResult::Rejected ? 1 : 0;
			}
			Trace.Location = Next;
			Trace.MaxZ = FMath::Max(Trace.MaxZ, Next.Z);
			Trace.MinZ = FMath::Min(Trace.MinZ, Next.Z);
		}
		return Trace;
	}
}

bool FLNPExactMovementGroundTest::RunTest(const FString& Parameters)
{
	FExactMovementWorld Fixture(TEXT("LNPExactMovementGroundTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 윗판 top z=10, x∈[-500,500]. 아랫판 top z=-490.
	Fixture.Slab(FVector(0, 0, 0), FVector(10, 10, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Slab(FVector(0, 0, -500), FVector(30, 30, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Publish();

	// 1. 지각 접지: 30cm 떠 있어도(하강 한도 60cm 안) 첫 프레임에 지면 위 캡슐 중심으로 붙는다.
	{
		FVector Next;
		FVector PhysVelocity;
		const EGroundResult Result = StepGrounded(*Fixture.Collision, Fixture.Params, FVector(0, 0, 10 + TestHalfHeight + 30), FVector::ZeroVector,
			TestDeltaTime, Next, PhysVelocity);
		TestTrue(TEXT("Hovering entity is grounded"), Result == EGroundResult::Grounded);
		TestEqual(TEXT("Grounded capsule center z"), Next.Z, 10.0 + TestHalfHeight, 1.0);
		TestTrue(TEXT("Grounded velocity is zero"), PhysVelocity.IsZero());
	}

	// 2. 절벽 낙하: +X로 걷다가 가장자리를 벗어나면 공중이 되고 아랫판에 착지한다.
	{
		// 약 25프레임 뒤 가장자리, 약 42프레임 낙하. 착지 뒤 아랫판 끝(x=1500)까지 걸어가지 않도록 100프레임만 돈다.
		const FTrace Trace = Simulate(Fixture, FVector(300, 0, 10 + TestHalfHeight), FVector::ZeroVector, FVector(600, 0, 0), 100);
		TestTrue(TEXT("Walking off the edge loses support"), Trace.LostSupportFrame != INDEX_NONE);
		TestTrue(TEXT("Support is lost only after the whole capsule clears the edge"), Trace.LostSupportLocation.X >= 500.0 + TestRadius - 1.0);
		TestEqual(TEXT("Fallen entity lands on the lower floor"), Trace.Location.Z, -490.0 + TestHalfHeight, 1.0);
		TestTrue(TEXT("Landed entity is grounded"), Trace.PhysVelocity.IsNearlyZero());
		TestEqual(TEXT("Lands once"), Trace.Landings, 1);
	}

	// 3. 묻힌 채 시작: 캡슐 중심이 지면 높이(반높이만큼 묻힘)에 있어도 서 있는 채로 몇 프레임 안에 지면 위로 올라온다.
	//    발밑 점에 스폰하던 결함(Phase03c 로그)에서 probe가 시작 겹침을 Rejected로 두어 영영 묻혀 있었다.
	{
		const FTrace Trace = Simulate(Fixture, FVector(0, 0, 10), FVector::ZeroVector, FVector::ZeroVector, 10);
		TestEqual(TEXT("Buried standing entity rises to the ground"), Trace.Location.Z, 10.0 + TestHalfHeight, 1.0);
		TestTrue(TEXT("Buried entity stays grounded"), Trace.PhysVelocity.IsNearlyZero());
	}
	return true;
}

bool FLNPExactMovementBlockTest::RunTest(const FString& Parameters)
{
	FExactMovementWorld Fixture(TEXT("LNPExactMovementBlockTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 바닥 top z=10, x·y∈[-2000,2000]. 벽 x∈[290,310], y∈[-500,500], 높이 300.
	Fixture.Slab(FVector(0, 0, 0), FVector(40, 40, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Slab(FVector(300, 0, 160), FVector(0.2, 10, 3), TEXT("LNPStaticBlocker"));
	// 25도 경사로(y=1500 줄)와 60도 경사(y=-1500 줄). Pitch는 +X 끝을 올린다.
	Fixture.Slab(FVector(900, 1500, 200), FVector(10, 3, 0.2), TEXT("LNPStaticTerrain"), FRotator(25, 0, 0));
	Fixture.Slab(FVector(700, -1500, 300), FVector(10, 3, 0.2), TEXT("LNPStaticTerrain"), FRotator(60, 0, 0));
	Fixture.Publish();

	const double GroundZ = 10.0 + TestHalfHeight;

	// 1. 벽 정면: 벽을 뚫지 않고 벽 앞에 선다.
	{
		const FTrace Trace = Simulate(Fixture, FVector(0, 0, GroundZ), FVector::ZeroVector, FVector(600, 0, 0), 120);
		TestTrue(TEXT("Entity stops in front of the wall"), Trace.Location.X <= 290.0 - TestRadius + 1.0);
		TestTrue(TEXT("Entity stays grounded at the wall"), Trace.PhysVelocity.IsNearlyZero());
	}

	// 2. 벽 사선: 벽 법선 성분만 지우고 벽을 따라 미끄러진다.
	{
		const FTrace Trace = Simulate(Fixture, FVector(0, -300, GroundZ), FVector::ZeroVector, FVector(424, 424, 0), 90);
		TestTrue(TEXT("Diagonal walk does not pass the wall"), Trace.Location.X <= 290.0 - TestRadius + 1.0);
		TestTrue(TEXT("Diagonal walk slides along the wall"), Trace.Location.Y > 200.0);
	}

	// 3. 25도 경사로: 걸어서 오른다.
	{
		const FTrace Trace = Simulate(Fixture, FVector(0, 1500, GroundZ), FVector::ZeroVector, FVector(400, 0, 0), 150);
		TestTrue(TEXT("Entity climbs the 25 degree ramp"), Trace.Location.Z > GroundZ + 150.0);
		TestTrue(TEXT("Entity is grounded on the ramp"), Trace.PhysVelocity.IsNearlyZero());
		TestEqual(TEXT("Never leaves the ramp surface"), Trace.LostSupportFrame, static_cast<int32>(INDEX_NONE));
	}

	// 4. 60도 경사: 한 단차 이상 오르지 못한다.
	{
		const FTrace Trace = Simulate(Fixture, FVector(0, -1500, GroundZ), FVector::ZeroVector, FVector(400, 0, 0), 150);
		TestTrue(TEXT("Entity cannot climb the 60 degree slope"), Trace.MaxZ < GroundZ + Fixture.Params.MaxStepUp + 1.0);
		TestTrue(TEXT("Entity is still grounded below the slope"), Trace.PhysVelocity.IsNearlyZero());
	}
	return true;
}

bool FLNPExactMovementAirborneTest::RunTest(const FString& Parameters)
{
	FExactMovementWorld Fixture(TEXT("LNPExactMovementAirborneTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 바닥 top z=10. 섬 몸체(Blocker 전용) z∈[350,450], x·y∈[-200,200]. 섬 윗면(Support) top z=470.
	Fixture.Slab(FVector(0, 0, 0), FVector(40, 40, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Slab(FVector(0, 0, 400), FVector(4, 4, 1), TEXT("LNPStaticBlocker"));
	Fixture.Slab(FVector(0, 0, 460), FVector(4, 4, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Publish();

	const double GroundZ = 10.0 + TestHalfHeight;

	// 1. 섬 밑면: 위로 튀어 오른 개체는 밑면에 막히고 착지하지 않으며 다시 바닥으로 떨어진다.
	{
		const FTrace Trace = Simulate(Fixture, FVector(0, 0, GroundZ), FVector(0, 0, 1500), FVector::ZeroVector, 180);
		TestTrue(TEXT("Underside blocks the capsule top"), Trace.MaxZ <= 350.0 - TestHalfHeight + 1.0);
		TestEqual(TEXT("Falls back to the floor"), Trace.Location.Z, GroundZ, 1.0);
		TestEqual(TEXT("Lands only on the floor"), Trace.Landings, 1);
	}

	// 2. 섬 측벽: 옆으로 날아간 개체는 측벽을 따라 미끄러져 떨어진다.
	{
		const FTrace Trace = Simulate(Fixture, FVector(-400, 0, 400), FVector(1000, 0, 0), FVector::ZeroVector, 180);
		TestTrue(TEXT("Side wall is not passed"), Trace.Location.X <= -200.0 - TestRadius + 1.0);
		TestEqual(TEXT("Slides down to the floor"), Trace.Location.Z, GroundZ, 1.0);
		TestTrue(TEXT("Grounded after sliding down"), Trace.PhysVelocity.IsNearlyZero());
	}

	// 3. 섬 윗면: 넉백으로 윗면 밖으로 밀려나면 아래 바닥에 착지한다.
	{
		const FTrace Trace = Simulate(Fixture, FVector(150, 0, 470 + TestHalfHeight), FVector(600, 0, 300), FVector::ZeroVector, 180);
		TestEqual(TEXT("Knocked off the island top lands on the floor below"), Trace.Location.Z, GroundZ, 1.0);
		TestTrue(TEXT("Lands beyond the island edge"), Trace.Location.X > 200.0);
	}

	// 4. 배회 목표 재투영: 엔티티와 같은 층의 지면을 고르고, 섬 가장자리 밖이면 실패한다.
	//    Reach 200: 섬 아래에서 위로 찍는 구가 섬 밑면(z=350)에 닿지 않는 거리.
	{
		auto DirectionTo = [&Fixture](const FVector& Point) { return (Point - Fixture.Params.GravityOrigin).GetSafeNormal(); };
		const double IslandTopZ = 470.0 + TestHalfHeight;
		FVector Center;

		TestTrue(TEXT("Under-island wander target is found"),
			ProjectToSameLayer(*Fixture.Collision, Fixture.Params, FVector(0, 0, GroundZ), DirectionTo(FVector(100, 0, GroundZ)), 200.f, Center));
		TestEqual(TEXT("Under-island wander target stays on the floor"), Center.Z, GroundZ, 1.0);

		TestTrue(TEXT("Island-top wander target is found"),
			ProjectToSameLayer(*Fixture.Collision, Fixture.Params, FVector(150, 0, IslandTopZ), DirectionTo(FVector(100, 0, IslandTopZ)), 200.f, Center));
		TestEqual(TEXT("Island-top wander target stays on the island"), Center.Z, IslandTopZ, 1.0);

		TestFalse(TEXT("Wander target beyond the island edge is rejected"),
			ProjectToSameLayer(*Fixture.Collision, Fixture.Params, FVector(150, 0, IslandTopZ), DirectionTo(FVector(300, 0, IslandTopZ)), 200.f, Center));
	}
	return true;
}

bool FLNPExactMovementCaveTest::RunTest(const FString& Parameters)
{
	FExactMovementWorld Fixture(TEXT("LNPExactMovementCaveTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 동굴 바닥 top z=10, 천장 밑면 z=410(바닥 +400, Meadow_00 관통 터널과 같은 높이).
	// 윗층 top z=1110, x=[-100,100]에 폭 200cm 수직 구멍.
	Fixture.Slab(FVector(0, 0, 0), FVector(40, 40, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Slab(FVector(0, 1500, 420), FVector(20, 5, 0.2), TEXT("LNPStaticBlocker"));
	Fixture.Slab(FVector(-550, 0, 1100), FVector(9, 5, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Slab(FVector(550, 0, 1100), FVector(9, 5, 0.2), TEXT("LNPStaticTerrain"));
	// 미등록 바닥(Unknown). 다른 판과 떨어져 있다.
	Fixture.Slab(FVector(5000, 5000, 0), FVector(5, 5, 0.2), TEXT("LNPStaticTerrain"), FRotator::ZeroRotator, /*bRegister=*/false);
	Fixture.Publish();

	const double GroundZ = 10.0 + TestHalfHeight;

	// 1. 천장 아래 보행: 높이가 바뀌지 않고 접지를 유지한다.
	{
		const FTrace Trace = Simulate(Fixture, FVector(-900, 1500, GroundZ), FVector::ZeroVector, FVector(600, 0, 0), 180);
		TestTrue(TEXT("Walks through the tunnel"), Trace.Location.X > 800.0);
		TestEqual(TEXT("Tunnel walk keeps the floor height (max)"), Trace.MaxZ, GroundZ, 1.0);
		TestEqual(TEXT("Tunnel walk keeps the floor height (min)"), Trace.MinZ, GroundZ, 1.0);
		TestEqual(TEXT("Tunnel walk never loses support"), Trace.LostSupportFrame, static_cast<int32>(INDEX_NONE));
	}

	// 2. 구멍 낙하: 윗층에서 구멍으로 걸어 들어가 아래층에 착지한다.
	{
		const FTrace Trace = Simulate(Fixture, FVector(-400, 0, 1110 + TestHalfHeight), FVector::ZeroVector, FVector(300, 0, 0), 240);
		TestTrue(TEXT("Loses support over the hole"), Trace.LostSupportFrame != INDEX_NONE);
		TestEqual(TEXT("Lands on the cave floor"), Trace.Location.Z, GroundZ, 1.0);
		TestTrue(TEXT("Grounded on the cave floor"), Trace.PhysVelocity.IsNearlyZero());
	}

	// 3. Unknown 미착지: 등록되지 않은 판 위로 떨어지면 뚫지 않지만 착지하지도 않는다.
	{
		Fixture.Collision->ResetStats();
		const FTrace Trace = Simulate(Fixture, FVector(5000, 5000, 400), FVector(0, 0, -100), FVector::ZeroVector, 120);
		TestEqual(TEXT("Never lands on an unknown surface"), Trace.Landings, 0);
		TestFalse(TEXT("Stays airborne on an unknown surface"), Trace.PhysVelocity.IsNearlyZero());
		TestTrue(TEXT("Does not fall through the unknown surface"), Trace.MinZ >= GroundZ - 1.0);
		TestTrue(TEXT("Unknown hits are counted"), Fixture.Collision->GetUnknownHitCount() > 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
