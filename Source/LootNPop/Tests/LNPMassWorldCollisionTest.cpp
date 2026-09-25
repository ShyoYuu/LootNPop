// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Async/ParallelFor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "HitDetection/LNPHitDetectionShared.h"
#include "HitDetection/LNPProjectileMotion.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPMassWorldCollisionApiTest,
	"LootNPop.SurfaceNavigation.WorldCollision.Api",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPDynamicMarkerHitTest,
	"LootNPop.SurfaceNavigation.WorldCollision.DynamicMarkerHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPProjectileExactArcTest,
	"LootNPop.SurfaceNavigation.WorldCollision.ProjectileArc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPProjectileEarliestHitTest,
	"LootNPop.SurfaceNavigation.WorldCollision.EarliestHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPRegressionMapExactTest,
	"LootNPop.SurfaceNavigation.WorldCollision.RegressionMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
#endif

namespace
{
	/** 기본 Cube(100cm)를 Scale로 늘린 판을 Actor 하나에 붙인다. */
	UStaticMeshComponent* SpawnSlab(UWorld* World, const FVector& Location, const FVector& Scale, const FName Profile,
		const EComponentMobility::Type Mobility = EComponentMobility::Static)
	{
		AActor* Owner = World->SpawnActor<AActor>();
		UStaticMeshComponent* Slab = NewObject<UStaticMeshComponent>(Owner);
		Slab->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Slab->SetMobility(Mobility);
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

bool FLNPDynamicMarkerHitTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("LNPDynamicMarkerHitTest"));
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

	// 마커 요소 패널(x=0)과 마커 없이 등록한 Dynamic 판(x=2000). 움직이는 패널과 같은 Movable·LNPDynamicTerrain이다.
	FLNPPlacementId Placement;
	Placement.Slot = 3;
	Placement.MarkerId = FGuid::NewGuid();
	UStaticMeshComponent* Panel = SpawnSlab(World, FVector(0, 0, 0), FVector(4, 4, 0.3), TEXT("LNPDynamicTerrain"), EComponentMobility::Movable);
	UStaticMeshComponent* Orphan = SpawnSlab(World, FVector(2000, 0, 0), FVector(4, 4, 0.3), TEXT("LNPDynamicTerrain"), EComponentMobility::Movable);
	HitIdentity->RegisterRuntimeSource(Panel, Placement);
	HitIdentity->RegisterRuntimeSource(Orphan);
	HitIdentity->Tick(0.f);

	const FLNPWorldQueryParams Mandatory(ELNPWorldQueryClass::ProjectileMandatory);
	Collision->ResetStats();

	// 1. 패널 hit는 Dynamic이고 (slot, MarkerId)를 싣는다.
	{
		FLNPWorldHit Hit;
		TestTrue(TEXT("Raycast hits the panel"), Collision->RaycastWorld(FVector(0, 0, 300), FVector(0, 0, -300), Mandatory, Hit));
		TestTrue(TEXT("Panel hit is Dynamic"), Hit.Identity.Lifetime == ELNPExactSourceLifetime::Dynamic);
		TestEqual(TEXT("Panel hit carries the marker slot"), static_cast<int32>(Hit.Identity.Slot), 3);
		TestTrue(TEXT("Panel hit carries the MarkerId"), Hit.Identity.MarkerId == Placement.MarkerId);
		TestEqual(TEXT("Panel hit roles are Support+Blocker"), static_cast<int32>(Hit.Identity.Roles),
			static_cast<int32>(ELNPExactSourceRole::Support | ELNPExactSourceRole::Blocker));
	}

	// 2. 패널이 움직여도 identity는 자세와 무관하다. 게임 스레드가 옮긴 자세를 query가 바로 본다(D-027).
	Panel->SetWorldLocation(FVector(0, 1000, 0));
	{
		FLNPWorldHit Hit;
		TestFalse(TEXT("Old panel pose is empty"), Collision->RaycastWorld(FVector(0, 0, 300), FVector(0, 0, -300), Mandatory, Hit));
		TestTrue(TEXT("Moved panel is hit"), Collision->RaycastWorld(FVector(0, 1000, 300), FVector(0, 1000, -300), Mandatory, Hit));
		TestTrue(TEXT("Moved panel keeps its MarkerId"), Hit.Identity.MarkerId == Placement.MarkerId);
	}

	// 3. 마커 없는 Dynamic은 분류 오류 counter로 따로 센다.
	{
		FLNPWorldHit Hit;
		TestTrue(TEXT("Raycast hits the orphan"), Collision->RaycastWorld(FVector(2000, 0, 300), FVector(2000, 0, -300), Mandatory, Hit));
		TestTrue(TEXT("Orphan hit is Dynamic"), Hit.Identity.Lifetime == ELNPExactSourceLifetime::Dynamic);
		TestFalse(TEXT("Orphan hit has no MarkerId"), Hit.Identity.MarkerId.IsValid());
	}
	TestEqual(TEXT("Dynamic hit count"), Collision->GetLifetimeHitCount(ELNPExactSourceLifetime::Dynamic), static_cast<uint64>(3));
	TestEqual(TEXT("Dynamic marker hit count"), Collision->GetDynamicMarkerHitCount(), static_cast<uint64>(2));
	TestEqual(TEXT("Dynamic without marker count"), Collision->GetDynamicWithoutMarkerCount(), static_cast<uint64>(1));
	TestEqual(TEXT("No static hits"), Collision->GetLifetimeHitCount(ELNPExactSourceLifetime::Static), static_cast<uint64>(0));

	// 4. 해제는 generation을 올린 새 snapshot으로 게시된다. worker가 쥐고 있던 옛 snapshot은 그대로 읽힌다.
	const TSharedRef<const FLNPHitIdentitySnapshot, ESPMode::ThreadSafe> OldSnapshot = HitIdentity->GetSnapshot();
	FHitResult PanelHit;
	PanelHit.Component = Panel;
	HitIdentity->UnregisterRuntimeSource(Panel);
	HitIdentity->Tick(0.f);
	TestTrue(TEXT("Unregister publishes a newer generation"), HitIdentity->GetSnapshot()->Generation > OldSnapshot->Generation);
	TestFalse(TEXT("New snapshot no longer knows the panel"), ULNPHitIdentitySubsystem::ResolveHit(*HitIdentity->GetSnapshot(), PanelHit).IsKnown());

	// 소유 Actor까지 파괴해도 옛 snapshot 조회는 index·serial 비교만 하므로 역참조하지 않는다.
	Panel->GetOwner()->Destroy();
	const FLNPExactHitIdentity Stale = ULNPHitIdentitySubsystem::ResolveHit(*OldSnapshot, PanelHit);
	TestTrue(TEXT("Old snapshot still resolves the destroyed panel"), Stale.MarkerId == Placement.MarkerId);
	TestEqual(TEXT("Stale result reports the old generation"), Stale.RegistryGeneration, OldSnapshot->Generation);

	return true;
}

bool FLNPProjectileEarliestHitTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("LNPProjectileEarliestHitTest"));
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

	// 벽 앞면 x=990. 적 캡슐 하나는 벽 앞(x=500), 하나는 벽 뒤(x=1500). 한 프레임 선분이 둘 다 지난다.
	UStaticMeshComponent* Wall = SpawnSlab(World, FVector(1000, 0, 0), FVector(0.2, 10, 10), TEXT("LNPStaticBlocker"));
	HitIdentity->RegisterRuntimeSource(Wall);
	HitIdentity->Tick(0.f);

	const FVector From(0, 0, 0);
	const FVector To(2000, 0, 0);
	const FVector Up = FVector::UpVector;
	constexpr float HalfHeight = 90.f;
	constexpr float Radius = 40.f;
	const FVector FrontEnemy(500, 0, 0);
	const FVector BehindEnemy(1500, 0, 0);

	FLNPWorldHit WorldHit;
	const FVector SegmentEnd = LNPProjectileMotion::ClipSegmentToWorld(Collision, From, To, WorldHit);
	TestTrue(TEXT("Segment hits the wall"), WorldHit.bBlockingHit);
	TestEqual(TEXT("Clipped segment ends on the wall face"), SegmentEnd.X, 990.0, 0.5);

	// 1. 벽보다 앞의 적은 잘린 선분 위에 있어 캐릭터 hit가 earliest hit다.
	FVector HitPoint;
	TestTrue(TEXT("Enemy in front of the wall is hit"),
		LNPHitDetection::SegmentHitsCapsule(From, SegmentEnd, FrontEnemy, Up, HalfHeight, Radius, HitPoint));

	// 2. 벽 뒤의 적은 잘린 선분 밖이라 맞지 않고 월드 hit가 earliest hit다.
	TestFalse(TEXT("Enemy behind the wall is not hit"),
		LNPHitDetection::SegmentHitsCapsule(From, SegmentEnd, BehindEnemy, Up, HalfHeight, Radius, HitPoint));

	// 3. 대조군: 월드 판정이 없으면 선분이 잘리지 않아 벽 뒤 적도 맞는다 — 2의 miss는 벽 때문이다.
	FLNPWorldHit NoWorldHit;
	const FVector Unclipped = LNPProjectileMotion::ClipSegmentToWorld(nullptr, From, To, NoWorldHit);
	TestTrue(TEXT("Without world collision the segment is not clipped"), Unclipped.Equals(To));
	TestTrue(TEXT("Without world collision the enemy behind is hit"),
		LNPHitDetection::SegmentHitsCapsule(From, Unclipped, BehindEnemy, Up, HalfHeight, Radius, HitPoint));

	// 4. 월드 hit가 없는 선분은 그대로 To에서 끝난다.
	FLNPWorldHit MissHit;
	const FVector MissEnd = LNPProjectileMotion::ClipSegmentToWorld(Collision, FVector(0, 5000, 0), FVector(2000, 5000, 0), MissHit);
	TestFalse(TEXT("Segment beside the wall misses"), MissHit.bBlockingHit);
	TestTrue(TEXT("Missed segment keeps its end"), MissEnd.Equals(FVector(2000, 5000, 0)));

	return true;
}

#if WITH_EDITOR
namespace
{
	/** 회귀 맵 사례 각도의 방사·접선 축(Scripts/GenerateSurfaceRegressionMap.py radial_basis). */
	struct FRegressionFrame
	{
		FVector Radial;
		FVector Tangent;

		explicit FRegressionFrame(const double AngleDegrees)
		{
			const double Radians = FMath::DegreesToRadians(AngleDegrees);
			Radial = FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.0);
			Tangent = FVector(-FMath::Sin(Radians), FMath::Cos(Radians), 0.0);
		}

		FVector At(const double Radius, const double TangentOffset = 0.0) const { return Radial * Radius + Tangent * TangentOffset; }
		double RadiusOf(const FVector& Point) const { return FVector::DotProduct(Point, Radial); }
		double TangentOf(const FVector& Point) const { return FVector::DotProduct(Point, Tangent); }
	};
}

/**
 * design/RegressionMap.md §3의 exact 기대를 회귀 맵 fixture로 확인한다.
 * 맵은 일반 레벨이라 slot 등록 경로를 타지 않는다. fixture component의 mesh·transform·profile·mobility를 테스트 월드로
 * 복제하고 런타임 source로 등록한다. 맵 에셋이 입력의 원본이고 좌표는 생성 스크립트의 치수에서 나온다.
 */
bool FLNPRegressionMapExactTest::RunTest(const FString& Parameters)
{
	UPackage* MapPackage = LoadPackage(nullptr, TEXT("/Game/Maps/SurfaceNavigation/L_SurfaceRegression"), LOAD_None);
	UWorld* MapWorld = MapPackage ? UWorld::FindWorldInPackage(MapPackage) : nullptr;
	if (!TestNotNull(TEXT("Regression map loads"), MapWorld) || !TestNotNull(TEXT("Regression map has a level"), MapWorld->PersistentLevel.Get()))
		return false;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("LNPRegressionMapExactTest"));
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

	int32 FixtureCount = 0;
	int32 RegisteredCount = 0;
	for (const AActor* MapActor : MapWorld->PersistentLevel->Actors)
	{
		const AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(MapActor);
		if (!MeshActor || !MeshActor->Tags.Contains(TEXT("LNP.Regression.Fixture")))
			continue;

		const UStaticMeshComponent* Source = MeshActor->GetStaticMeshComponent();
		AActor* Owner = World->SpawnActor<AActor>();
		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(Owner);
		Copy->SetStaticMesh(Source->GetStaticMesh());
		Copy->SetMobility(Source->Mobility);
		Copy->SetCollisionProfileName(Source->GetCollisionProfileName());
		Copy->SetWorldTransform(Source->GetRelativeTransform());
		Owner->SetRootComponent(Copy);
		Copy->RegisterComponent();
		++FixtureCount;

		// Decoration은 LNPWorldExact를 무시하므로 registry 대상이 아니다. 복제만 해 두고 miss를 확인한다.
		ELNPExactSourceLifetime Lifetime;
		uint8 Roles;
		if (ULNPHitIdentitySubsystem::ClassifyProfile(Copy->GetCollisionProfileName(), Lifetime, Roles))
		{
			HitIdentity->RegisterRuntimeSource(Copy);
			++RegisteredCount;
		}
	}
	TestEqual(TEXT("Fixture actor count"), FixtureCount, 37);
	TestEqual(TEXT("Exact source count (fixtures minus 12 decorations)"), RegisteredCount, 25);

	// Pawn 제외: 정적 프랍 사례의 지면 앞에 Pawn profile 판을 둔다. 등록하지 않으므로 맞으면 Unknown counter가 오른다.
	const FRegressionFrame Props(45.0);
	SpawnSlab(World, Props.At(24500.0, -700.0), FVector(1, 1, 1), TEXT("Pawn"), EComponentMobility::Movable);
	HitIdentity->Tick(0.f);
	Collision->ResetStats();

	using namespace ELNPExactSourceRole;
	const FLNPWorldQueryParams Query(ELNPWorldQueryClass::DebugValidation);
	constexpr double Tolerance = 1.0;

	// 1. 기본 지각: 바깥 ray가 지각 안쪽 면(r=25000)에 맞는다.
	{
		const FRegressionFrame F(5.0);
		FLNPWorldHit Hit;
		TestTrue(TEXT("BasicCrust: outward ray hits"), Collision->RaycastWorld(F.At(24000.0), F.At(26000.0), Query, Hit));
		TestEqual(TEXT("BasicCrust: hit radius"), F.RadiusOf(Hit.ImpactPoint), 25000.0, Tolerance);
		TestTrue(TEXT("BasicCrust: normal faces the center"), FVector::DotProduct(Hit.ImpactNormal, -F.Radial) > 0.99);
		TestTrue(TEXT("BasicCrust: static support"), Hit.Identity.Lifetime == ELNPExactSourceLifetime::Static && (Hit.Identity.Roles & Support) != 0);
	}

	// 2. 부유섬 하나: 섬 윗면(23000)이 먼저, 섬을 지난 ray는 지각(25000)에 맞는다.
	{
		const FRegressionFrame F(13.0);
		FLNPWorldHit Hit;
		TestTrue(TEXT("IslandOne: island hit"), Collision->RaycastWorld(F.At(22000.0), F.At(26000.0), Query, Hit));
		TestEqual(TEXT("IslandOne: island top radius"), F.RadiusOf(Hit.ImpactPoint), 23000.0, Tolerance);
		TestTrue(TEXT("IslandOne: crust behind island"), Collision->RaycastWorld(F.At(23150.0), F.At(26000.0), Query, Hit));
		TestEqual(TEXT("IslandOne: crust radius"), F.RadiusOf(Hit.ImpactPoint), 25000.0, Tolerance);
	}

	// 3. 부유섬 둘: 안쪽 섬 → 바깥 섬 → 지각 순서로 구분된다.
	{
		const FRegressionFrame F(21.0);
		const double Starts[] = { 20000.0, 21550.0, 23350.0 };
		const double Expected[] = { 21400.0, 23200.0, 25000.0 };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Starts); ++Index)
		{
			FLNPWorldHit Hit;
			TestTrue(FString::Printf(TEXT("IslandTwo: layer %d hit"), Index), Collision->RaycastWorld(F.At(Starts[Index]), F.At(26000.0), Query, Hit));
			TestEqual(FString::Printf(TEXT("IslandTwo: layer %d radius"), Index), F.RadiusOf(Hit.ImpactPoint), Expected[Index], Tolerance);
		}
	}

	// 4·5. 섬 가장자리: 안쪽(접선 550)은 윗면 support, 바깥(750)은 support miss, 측벽(접선 600)은 sweep hit.
	{
		const FRegressionFrame F(29.0);
		FLNPSupportProbeQuery Probe;
		Probe.Up = -F.Radial;
		FLNPSupportProbeResult Result;

		Probe.Position = F.At(22950.0, 550.0);
		TestTrue(TEXT("IslandEdge inside: supported"), Collision->ProbeSupport(Probe, Query, Result));
		TestEqual(TEXT("IslandEdge inside: top radius"), F.RadiusOf(Result.Hit.ImpactPoint), 23000.0, Tolerance);

		Probe.Position = F.At(22950.0, 750.0);
		TestFalse(TEXT("IslandEdge outside: not supported"), Collision->ProbeSupport(Probe, Query, Result));
		TestFalse(TEXT("IslandEdge outside: nothing below"), Result.Hit.bBlockingHit);

		FLNPWorldHit Hit;
		TestTrue(TEXT("IslandEdge side wall: sweep hits"), Collision->SweepSphereWorld(F.At(23050.0, 900.0), F.At(23050.0, 0.0), 20.f, Query, Hit));
		TestEqual(TEXT("IslandEdge side wall: tangent"), F.TangentOf(Hit.ImpactPoint), 600.0, Tolerance);
		TestTrue(TEXT("IslandEdge side wall: normal faces outward"), FVector::DotProduct(Hit.ImpactNormal, F.Tangent) > 0.99);
	}

	// 6. 단순 동굴: 바깥은 floor(Support), 안쪽은 ceiling, 옆은 벽(둘 다 Blocker만).
	{
		const FRegressionFrame F(37.0);
		const FVector Inside = F.At(24600.0);
		FLNPWorldHit Hit;
		TestTrue(TEXT("Cave: floor hit"), Collision->RaycastWorld(Inside, F.At(26000.0), Query, Hit));
		TestEqual(TEXT("Cave: floor radius"), F.RadiusOf(Hit.ImpactPoint), 25000.0, Tolerance);
		TestTrue(TEXT("Cave: floor is support"), (Hit.Identity.Roles & Support) != 0);

		TestTrue(TEXT("Cave: ceiling hit"), Collision->RaycastWorld(Inside, F.At(23000.0), Query, Hit));
		TestEqual(TEXT("Cave: ceiling radius"), F.RadiusOf(Hit.ImpactPoint), 24200.0, Tolerance);
		TestEqual(TEXT("Cave: ceiling is blocker only"), static_cast<int32>(Hit.Identity.Roles), static_cast<int32>(Blocker));

		for (const double Sign : { 1.0, -1.0 })
		{
			TestTrue(TEXT("Cave: wall hit"), Collision->RaycastWorld(Inside, F.At(24600.0, Sign * 2000.0), Query, Hit));
			TestEqual(TEXT("Cave: wall tangent"), F.TangentOf(Hit.ImpactPoint), Sign * 800.0, Tolerance);
			TestEqual(TEXT("Cave: wall is blocker only"), static_cast<int32>(Hit.Identity.Roles), static_cast<int32>(Blocker));
		}
	}

	// 7. 정적 프랍: 나무·바위는 hit(Blocker만), 장식과 Pawn은 통과해 지면에 맞는다.
	{
		const FRegressionFrame& F = Props;
		FLNPWorldHit Hit;
		TestTrue(TEXT("Props: tree hit"), Collision->RaycastWorld(F.At(24700.0), F.At(24700.0, -1000.0), Query, Hit));
		TestEqual(TEXT("Props: tree surface tangent"), F.TangentOf(Hit.ImpactPoint), -260.0, 5.0);
		TestEqual(TEXT("Props: tree is blocker only"), static_cast<int32>(Hit.Identity.Roles), static_cast<int32>(Blocker));

		// 바위는 비균등 scale 구라 표면 위치 대신 바위 영역(접선 125~350) 안에서 맞았는지만 본다.
		TestTrue(TEXT("Props: rock hit"), Collision->RaycastWorld(F.At(24800.0), F.At(24800.0, 1000.0), Query, Hit));
		TestTrue(TEXT("Props: rock surface is before the rock center"), F.TangentOf(Hit.ImpactPoint) > 100.0 && F.TangentOf(Hit.ImpactPoint) < 350.0);
		TestEqual(TEXT("Props: rock is blocker only"), static_cast<int32>(Hit.Identity.Roles), static_cast<int32>(Blocker));

		TestTrue(TEXT("Props: ray through decoration hits ground"), Collision->RaycastWorld(F.At(24000.0, 700.0), F.At(26000.0, 700.0), Query, Hit));
		TestEqual(TEXT("Props: decoration is missed"), F.RadiusOf(Hit.ImpactPoint), 25000.0, Tolerance);

		TestTrue(TEXT("Props: ray through pawn hits ground"), Collision->RaycastWorld(F.At(24000.0, -700.0), F.At(26000.0, -700.0), Query, Hit));
		TestEqual(TEXT("Props: pawn is excluded"), F.RadiusOf(Hit.ImpactPoint), 25000.0, Tolerance);
	}

	// 8. 상태형 기둥: 기둥 자체는 항상 hit(Dynamic, Blocker만).
	{
		const FRegressionFrame F(53.0);
		FLNPWorldHit Hit;
		TestTrue(TEXT("Pillar: hit"), Collision->RaycastWorld(F.At(24500.0, -500.0), F.At(24500.0, 500.0), Query, Hit));
		TestEqual(TEXT("Pillar: surface tangent"), F.TangentOf(Hit.ImpactPoint), -90.0, 5.0);
		TestTrue(TEXT("Pillar: dynamic blocker"), Hit.Identity.Lifetime == ELNPExactSourceLifetime::Dynamic && Hit.Identity.Roles == Blocker);
	}

	// 9. 움직이는 패널: 현재 transform에서 hit(Dynamic Support), 패널을 지난 gap은 miss.
	{
		const FRegressionFrame F(61.0);
		FLNPWorldHit Hit;
		TestTrue(TEXT("Panel: hit"), Collision->RaycastWorld(F.At(23800.0), F.At(26000.0), Query, Hit));
		TestEqual(TEXT("Panel: radius"), F.RadiusOf(Hit.ImpactPoint), 24200.0, Tolerance);
		TestTrue(TEXT("Panel: dynamic support"), Hit.Identity.Lifetime == ELNPExactSourceLifetime::Dynamic && (Hit.Identity.Roles & Support) != 0);
		TestFalse(TEXT("Panel: gap below the panel misses"), Collision->RaycastWorld(F.At(24350.0), F.At(26000.0), Query, Hit));
	}

	// 10. 파괴 바닥: 파괴 전에는 hit(Destructible).
	{
		const FRegressionFrame F(69.0);
		FLNPWorldHit Hit;
		TestTrue(TEXT("Destructible: hit"), Collision->RaycastWorld(F.At(24500.0), F.At(26000.0), Query, Hit));
		TestEqual(TEXT("Destructible: radius"), F.RadiusOf(Hit.ImpactPoint), 25000.0, Tolerance);
		TestTrue(TEXT("Destructible: lifetime"), Hit.Identity.Lifetime == ELNPExactSourceLifetime::Destructible);
	}

	// 11. 옥탄트 seam: 경계 양쪽에서 같은 높이로 hit, 경계를 가로지르는 sweep에 턱이 없다.
	{
		const FRegressionFrame F(90.0);
		for (const double Offset : { -5.0, 5.0 })
		{
			FLNPWorldHit Hit;
			TestTrue(TEXT("Seam: hit"), Collision->RaycastWorld(F.At(24000.0, Offset), F.At(26000.0, Offset), Query, Hit));
			TestEqual(TEXT("Seam: radius"), F.RadiusOf(Hit.ImpactPoint), 25000.0, Tolerance);
		}
		FLNPWorldHit Hit;
		TestFalse(TEXT("Seam: sweep along the surface has no step"),
			Collision->SweepSphereWorld(F.At(24900.0, -500.0), F.At(24900.0, 500.0), 50.f, Query, Hit));
	}

	TestEqual(TEXT("No unknown hits"), Collision->GetUnknownHitCount(), static_cast<uint64>(0));
	return true;
}
#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
