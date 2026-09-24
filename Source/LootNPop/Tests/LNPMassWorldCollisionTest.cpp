// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Async/ParallelFor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HitDetection/LNPProjectileMotion.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPMassWorldCollisionApiTest,
	"LootNPop.SurfaceNavigation.WorldCollision.Api",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPProjectileExactArcTest,
	"LootNPop.SurfaceNavigation.WorldCollision.ProjectileArc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	/** 기본 Cube(100cm)를 Scale로 늘린 판을 Actor 하나에 붙인다. */
	UStaticMeshComponent* SpawnSlab(UWorld* World, const FVector& Location, const FVector& Scale, const FName Profile)
	{
		AActor* Owner = World->SpawnActor<AActor>();
		UStaticMeshComponent* Slab = NewObject<UStaticMeshComponent>(Owner);
		Slab->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Slab->SetMobility(EComponentMobility::Static);
		Slab->SetCollisionProfileName(Profile);
		Slab->SetWorldLocation(Location);
		Slab->SetWorldScale3D(Scale);
		Owner->SetRootComponent(Slab);
		Slab->RegisterComponent();
		return Slab;
	}
}

bool FLNPMassWorldCollisionApiTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("LNPMassWorldCollisionApiTest"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	ULNPHitIdentitySubsystem* HitIdentity = World->GetSubsystem<ULNPHitIdentitySubsystem>();
	ULNPMassWorldCollisionSubsystem* Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
	if (!TestNotNull(TEXT("Hit identity subsystem exists"), HitIdentity)
		|| !TestNotNull(TEXT("World collision subsystem exists"), Collision))
		return false;

	// 위 바닥(z=0), 아래 바닥(z=-500), 벽(x=1000), 미등록 바닥(x=-2000). 판 두께 20cm.
	UStaticMeshComponent* UpperFloor = SpawnSlab(World, FVector(0, 0, 0), FVector(10, 10, 0.2), TEXT("LNPStaticTerrain"));
	UStaticMeshComponent* LowerFloor = SpawnSlab(World, FVector(0, 0, -500), FVector(10, 10, 0.2), TEXT("LNPStaticTerrain"));
	UStaticMeshComponent* Wall = SpawnSlab(World, FVector(1000, 0, 200), FVector(0.2, 10, 10), TEXT("LNPStaticBlocker"));
	UStaticMeshComponent* Unregistered = SpawnSlab(World, FVector(-2000, 0, 0), FVector(5, 5, 0.2), TEXT("LNPStaticTerrain"));
	HitIdentity->RegisterRuntimeSource(UpperFloor);
	HitIdentity->RegisterRuntimeSource(LowerFloor);
	HitIdentity->RegisterRuntimeSource(Wall);
	HitIdentity->Tick(0.f);

	const FLNPWorldQueryParams Mandatory(ELNPWorldQueryClass::GroundRiskFallback);

	// 1. Raycast: 위 바닥의 윗면(z=10)에 맞고 Static Support로 해석된다.
	{
		FLNPWorldHit Hit;
		TestTrue(TEXT("Raycast hits the upper floor"), Collision->RaycastWorld(FVector(0, 0, 300), FVector(0, 0, -300), Mandatory, Hit));
		TestEqual(TEXT("Raycast impact z"), Hit.ImpactPoint.Z, 10.0, 0.5);
		TestTrue(TEXT("Raycast normal is up"), Hit.ImpactNormal.Equals(FVector::UpVector, 0.01));
		TestTrue(TEXT("Raycast identity is static"), Hit.Identity.Lifetime == ELNPExactSourceLifetime::Static);
		TestTrue(TEXT("Raycast identity has Support role"), (Hit.Identity.Roles & ELNPExactSourceRole::Support) != 0);
	}

	// 2. miss: Time=1, Location=End.
	{
		FLNPWorldHit Hit;
		TestFalse(TEXT("Raycast outside all slabs misses"), Collision->RaycastWorld(FVector(5000, 5000, 300), FVector(5000, 5000, -300), Mandatory, Hit));
		TestEqual(TEXT("Miss time"), Hit.Time, 1.f);
		TestTrue(TEXT("Miss location is End"), Hit.Location.Equals(FVector(5000, 5000, -300)));
	}

	// 3. 제외 Actor ID: 위 바닥을 무시하면 아래 바닥(윗면 z=-490)에 맞는다.
	{
		FLNPWorldQueryParams IgnoreUpper(ELNPWorldQueryClass::ProjectileMandatory);
		IgnoreUpper.IgnoredActorIds.Add(UpperFloor->GetOwner()->GetUniqueID());
		FLNPWorldHit Hit;
		TestTrue(TEXT("Ignoring the upper floor still hits something"), Collision->RaycastWorld(FVector(0, 0, 300), FVector(0, 0, -900), IgnoreUpper, Hit));
		TestEqual(TEXT("Ignored upper floor falls through to the lower floor"), Hit.ImpactPoint.Z, -490.0, 0.5);
	}

	// 4. sweep: 구 반지름만큼 먼저 멈추고 캡슐은 축 방향 반높이만큼 먼저 멈춘다.
	{
		FLNPWorldHit Hit;
		TestTrue(TEXT("Sphere sweep hits the floor"), Collision->SweepSphereWorld(FVector(0, 0, 300), FVector(0, 0, -300), 50.f, Mandatory, Hit));
		TestEqual(TEXT("Sphere center stops one radius above the floor"), Hit.Location.Z, 60.0, 1.0);

		TestTrue(TEXT("Capsule sweep hits the floor"), Collision->SweepCapsuleWorld(FVector(0, 0, 300), FVector(0, 0, -300),
			FQuat::Identity, 40.f, 90.f, Mandatory, Hit));
		TestEqual(TEXT("Upright capsule center stops one half height above the floor"), Hit.Location.Z, 100.0, 1.0);
	}

	// 5. ProbeSupport: 바닥은 지지면, 벽은 법선이 Up과 수직이라 지지면이 아니다.
	{
		FLNPSupportProbeQuery Query;
		Query.Position = FVector(0, 0, 40);
		Query.Up = FVector::UpVector;
		FLNPSupportProbeResult Result;
		TestTrue(TEXT("Floor below is supported"), Collision->ProbeSupport(Query, Mandatory, Result));
		TestTrue(TEXT("Floor normal is walkable"), Result.bWalkableNormal);

		// 벽 옆면(x=980)을 Up=-X로 탐색한다. hit은 있지만 역할이 Blocker뿐이다.
		Query.Position = FVector(900, 0, 200);
		Query.Up = FVector(-1, 0, 0);
		TestFalse(TEXT("Blocker-only wall is not supported"), Collision->ProbeSupport(Query, Mandatory, Result));
		TestTrue(TEXT("Wall probe still hits"), Result.Hit.bBlockingHit);
		TestTrue(TEXT("Wall face normal is walkable for this Up"), Result.bWalkableNormal);

		// 같은 벽을 Up=+Z로 탐색하면 아무것도 없다(벽과 거리가 멀다).
		Query.Up = FVector::UpVector;
		TestFalse(TEXT("No support below a point in the air"), Collision->ProbeSupport(Query, Mandatory, Result));
	}

	// 6. 미등록 source는 Unknown이고 counter가 오른다.
	{
		const uint64 UnknownBefore = Collision->GetUnknownHitCount();
		FLNPWorldHit Hit;
		TestTrue(TEXT("Unregistered floor is hit"), Collision->RaycastWorld(FVector(-2000, 0, 300), FVector(-2000, 0, -300), Mandatory, Hit));
		TestFalse(TEXT("Unregistered floor is unknown"), Hit.Identity.IsKnown());
		TestEqual(TEXT("Unknown counter increments"), Collision->GetUnknownHitCount(), UnknownBefore + 1);
	}

	// 7. worker 스레드: 게임 스레드와 같은 결과를 낸다.
	{
		constexpr int32 WorkerQueries = 256;
		TArray<FLNPWorldHit> Hits;
		Hits.SetNum(WorkerQueries);
		std::atomic<int32> OffGameThread = 0;
		const FLNPWorldQueryParams Optional(ELNPWorldQueryClass::DebugValidation);
		Collision->ResetStats();
		ParallelFor(WorkerQueries, [&](const int32 Index)
		{
			if (!IsInGameThread())
			{
				OffGameThread.fetch_add(1, std::memory_order_relaxed);
			}
			const double X = -400.0 + 800.0 * Index / WorkerQueries;
			Collision->RaycastWorld(FVector(X, 0, 300), FVector(X, 0, -300), Optional, Hits[Index]);
		});

		int32 Mismatches = 0;
		for (int32 Index = 0; Index < WorkerQueries; ++Index)
		{
			const FLNPWorldHit& Hit = Hits[Index];
			if (!Hit.bBlockingHit || !FMath::IsNearlyEqual(Hit.ImpactPoint.Z, 10.0, 0.5) || Hit.Identity.Lifetime != ELNPExactSourceLifetime::Static)
			{
				++Mismatches;
			}
		}
		TestEqual(TEXT("Worker raycasts all resolve to the upper floor"), Mismatches, 0);
		TestTrue(TEXT("Some raycasts ran off the game thread"), OffGameThread.load() > 0);
		TestEqual(TEXT("Optional class counter"), Collision->GetQueryCount(ELNPWorldQueryClass::DebugValidation), static_cast<uint64>(WorkerQueries));
		TestEqual(TEXT("Mandatory counters are separate"), Collision->GetQueryCount(ELNPWorldQueryClass::GroundRiskFallback), static_cast<uint64>(0));
	}

	return true;
}

bool FLNPProjectileExactArcTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("LNPProjectileExactArcTest"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	ULNPHitIdentitySubsystem* HitIdentity = World->GetSubsystem<ULNPHitIdentitySubsystem>();
	ULNPMassWorldCollisionSubsystem* Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();
	if (!TestNotNull(TEXT("Hit identity subsystem exists"), HitIdentity)
		|| !TestNotNull(TEXT("World collision subsystem exists"), Collision))
		return false;

	// 구 내벽 세계라 중력은 원점에서 바깥쪽이다. 원점에서 1km 아래에 두면 궤적 구간에서 중력이 거의 -Z로 균일하다.
	// 원점 근처에 두면 바깥쪽 성분이 커서 궤적이 옆으로 휜다. 바닥 윗면 z=Base-990, 벽 앞면 x=490.
	const FVector Base(0, 0, -100000);
	UStaticMeshComponent* Floor = SpawnSlab(World, Base + FVector(0, 0, -1000), FVector(40, 40, 0.2), TEXT("LNPStaticTerrain"));
	HitIdentity->RegisterRuntimeSource(Floor);
	HitIdentity->Tick(0.f);

	const FVector Start = Base + FVector(0, 0, -200);
	const FVector Velocity(1000, 0, 0);
	constexpr float GravityAccel = 980.f;
	constexpr float Lifetime = 5.f;

	// 1. 바닥에 닿는 궤적은 정확히 ArcPointCount개이고 마지막 점이 바닥 윗면이다.
	{
		Collision->ResetStats();
		TArray<FVector> Points;
		LNPProjectileMotion::PredictArc(*Collision, Start, Velocity, GravityAccel, Lifetime, Points);
		TestEqual(TEXT("Arc point count"), Points.Num(), LNPProjectileMotion::ArcPointCount);
		TestTrue(TEXT("First point is the muzzle"), Points[0].Equals(Start, 0.1));
		TestEqual(TEXT("Arc ends on the floor top"), Points.Last().Z, Base.Z - 990.0, 1.0);
		TestTrue(TEXT("Arc queries are ProjectileMandatory"), Collision->GetQueryCount(ELNPWorldQueryClass::ProjectileMandatory) > 0);
	}

	// 2. 벽이 궤적을 가로막으면 벽 앞면에서 끝난다.
	UStaticMeshComponent* Wall = SpawnSlab(World, Base + FVector(500, 0, -500), FVector(0.2, 10, 10), TEXT("LNPStaticBlocker"));
	HitIdentity->RegisterRuntimeSource(Wall);
	HitIdentity->Tick(0.f);
	{
		TArray<FVector> Points;
		LNPProjectileMotion::PredictArc(*Collision, Start, Velocity, GravityAccel, Lifetime, Points);
		TestEqual(TEXT("Arc ends on the wall face"), Points.Last().X, 490.0, 1.0);
		TestTrue(TEXT("Arc ends above the floor"), Points.Last().Z > Base.Z - 990.0);
	}

	// 3. 아무것도 없으면 수명 끝까지 뻗는다(월드 판정이 없는 상태).
	{
		TArray<FVector> Points;
		LNPProjectileMotion::PredictArc(*Collision, FVector(0, 0, 5000), FVector(1000, 0, 0), 0.f, 1.f, Points);
		TestTrue(TEXT("Arc without geometry runs to lifetime"), Points.Last().Equals(FVector(1000, 0, 5000), 1.0));
	}

	// 4. TraceWorld는 투사체 한 프레임 선분 판정과 같은 결과를 낸다(바닥 관통 선분).
	{
		FLNPWorldHit Hit;
		TestTrue(TEXT("Segment crossing the floor hits"), LNPProjectileMotion::TraceWorld(*Collision, Base + FVector(-500, 0, -900), Base + FVector(-500, 0, -1100), Hit));
		TestEqual(TEXT("Segment impact on floor top"), Hit.ImpactPoint.Z, Base.Z - 990.0, 0.5);
		TestTrue(TEXT("Segment impact is a known static surface"), Hit.Identity.Lifetime == ELNPExactSourceLifetime::Static);
	}

	// 5. envelope는 등록된 geometry 중 원점에서 가장 먼 점이다 — 바닥 아랫면 모서리.
	const float EnvelopeRadius = Collision->GetWorldEnvelopeRadius();
	{
		const double Expected = FVector(2000, 2000, Base.Z - 1010.0).Size();
		TestEqual(TEXT("Envelope is the farthest floor corner"), static_cast<double>(EnvelopeRadius), Expected, 1.0);
		TestFalse(TEXT("No envelope means no safety net"), LNPProjectileMotion::IsOutsideWorldEnvelope(0.f, FVector(0, 0, -1e7)));
	}

	// 6. 바닥 옆을 지나 아래(바깥쪽)로 빠진 궤적은 수명 끝이 아니라 envelope + 여유를 넘은 첫 스텝에서 끝난다.
	{
		TArray<FVector> Points;
		LNPProjectileMotion::PredictArc(*Collision, Base + FVector(3000, 0, -200), FVector(0, 0, -1000), GravityAccel, Lifetime, Points);
		const double EndRadius = Points.Last().Size();
		const double Limit = EnvelopeRadius + LNPProjectileMotion::WorldEnvelopeMargin;
		TestTrue(TEXT("Escaped arc ends outside the envelope"), EndRadius > Limit);
		TestTrue(TEXT("Escaped arc ends within one step of the envelope"), EndRadius < Limit + 100.0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
