// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "Enemy/LNPSurfaceCrawl.h"
#include "SurfaceNavigation/LNPHitIdentityRegistry.h"
#include "SurfaceNavigation/LNPMassWorldCollision.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceCrawlFlatTest,
	"LootNPop.SurfaceNavigation.SurfaceCrawl.FlatAndCeiling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceCrawlClimbTest,
	"LootNPop.SurfaceNavigation.SurfaceCrawl.ClimbAndDescend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceCrawlFallTest,
	"LootNPop.SurfaceNavigation.SurfaceCrawl.FallAndUnknown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	using namespace LNPSurfaceCrawl;

	constexpr float TestDeltaTime = 1.f / 60.f;
	constexpr float TestRadius = 40.f;
	constexpr float TestSpeed = 300.f;

	/** 평평한 테스트 월드. 구 중심을 +Z 멀리 두어 중력 Up이 사실상 +Z다. 판은 기본 Cube(100cm)를 Scale로 늘린다. */
	struct FCrawlWorld
	{
		UWorld* World = nullptr;
		ULNPHitIdentitySubsystem* HitIdentity = nullptr;
		ULNPMassWorldCollisionSubsystem* Collision = nullptr;
		FParams Params;
		LNPEnemyExactMovement::FParams FallParams;

		explicit FCrawlWorld(const TCHAR* Name)
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, Name);
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);
			HitIdentity = World->GetSubsystem<ULNPHitIdentitySubsystem>();
			Collision = World->GetSubsystem<ULNPMassWorldCollisionSubsystem>();

			Params.BodyRadius = TestRadius;
			// 벽보행 몸은 구다 — 낙하도 반높이 = 반지름인 캡슐(= 구)로 sweep한다.
			FallParams.GravityOrigin = FVector(0, 0, 1.0e7);
			FallParams.CapsuleRadius = TestRadius;
			FallParams.CapsuleHalfHeight = TestRadius;
		}

		~FCrawlWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		void Box(const FVector& Location, const FVector& Scale, const FName Profile, const bool bRegister = true)
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

		/** 몸 구가 지형과 겹쳐 있는가. 측정용 query라 SurfaceCrawl 분류 수에 섞이지 않게 DebugValidation으로 잰다. */
		bool IsPenetrating(const FVector& Location, const FVector& Normal) const
		{
			FLNPWorldHit Hit;
			return Collision->SweepSphereWorld(Location, Location + Normal * 0.1f, TestRadius,
				FLNPWorldQueryParams(ELNPWorldQueryClass::DebugValidation), Hit) && Hit.bStartPenetrating;
		}
	};

	/** 벽보행 이동 프로세서와 같은 규약(속도 0 = 붙음)으로 목표점을 향해 기어간 결과. */
	struct FCrawler
	{
		FVector Location = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		FVector PhysVelocity = FVector::ZeroVector;
		int32 Penetrations = 0;
		int32 Lost = 0;
		int32 Blocked = 0;
		int32 Landings = 0;
		double MinNormalZ = 1.0;

		/** 목표에 도착 판정 거리(GoalDistanceSq) Tolerance 안으로 들어오면 true. */
		bool CrawlTo(const FCrawlWorld& Fixture, const FVector& Goal, const int32 MaxFrames, const float Tolerance = 30.f)
		{
			for (int32 Frame = 0; Frame < MaxFrames; ++Frame)
			{
				if (GoalDistanceSq(Location, Normal, Goal, TestRadius) < FMath::Square(Tolerance) && PhysVelocity.IsZero())
				{
					return true;
				}
				const FVector GravityUp = (Fixture.FallParams.GravityOrigin - Location).GetSafeNormal();
				Tick(Fixture, SteerDirection(Location, Normal, Goal, GravityUp, TestRadius) * TestSpeed);
			}
			return false;
		}

		void Tick(const FCrawlWorld& Fixture, const FVector& Velocity)
		{
			FVector Next = Location;
			if (!PhysVelocity.IsZero())
			{
				if (LNPEnemyExactMovement::StepAirborne(*Fixture.Collision, Fixture.FallParams, Location, PhysVelocity, TestDeltaTime, Next))
				{
					++Landings;
					const FVector GravityUp = (Fixture.FallParams.GravityOrigin - Next).GetSafeNormal();
					FVector Attached, AttachedNormal;
					Normal = GravityUp;
					if (Attach(*Fixture.Collision, Fixture.Params, Next, -GravityUp, Fixture.Params.SurfaceGap + 1.f, Attached, AttachedNormal))
					{
						Next = Attached;
						Normal = AttachedNormal;
					}
				}
			}
			else
			{
				FVector NextNormal;
				const EStepResult Result = Step(*Fixture.Collision, Fixture.Params, Location, Normal, Velocity, TestDeltaTime, Next, NextNormal);
				Normal = NextNormal;
				Blocked += Result == EStepResult::Blocked ? 1 : 0;
				if (Result == EStepResult::Lost)
				{
					++Lost;
					const FVector GravityUp = (Fixture.FallParams.GravityOrigin - Next).GetSafeNormal();
					PhysVelocity = FVector::VectorPlaneProject(Velocity, Normal) - GravityUp * Fixture.FallParams.GravityStrength * TestDeltaTime;
				}
			}
			Location = Next;
			Penetrations += Fixture.IsPenetrating(Location, Normal) ? 1 : 0;
			MinNormalZ = FMath::Min(MinNormalZ, Normal.Z);
		}
	};
}

bool FLNPSurfaceCrawlFlatTest::RunTest(const FString& Parameters)
{
	FCrawlWorld Fixture(TEXT("LNPSurfaceCrawlFlatTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 바닥 top z=10. 천장 bottom z=590.
	Fixture.Box(FVector(0, 0, 0), FVector(40, 40, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Box(FVector(0, 0, 600), FVector(40, 40, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Publish();
	const double FloorZ = 10.0 + TestRadius + Fixture.Params.SurfaceGap;
	const double CeilingZ = 590.0 - TestRadius - Fixture.Params.SurfaceGap;

	// 1. 평지: 붙은 채 높이와 법선을 유지한다. 이동 프레임마다 query는 접평면 sweep + probe 2회다.
	{
		FCrawler Crawler;
		Crawler.Location = FVector(0, 0, FloorZ);
		const uint64 Before = Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::SurfaceCrawl);
		for (int32 Frame = 0; Frame < 60; ++Frame)
			Crawler.Tick(Fixture, FVector(TestSpeed, 0, 0));
		TestEqual(TEXT("Flat crawl uses two queries per moving frame"),
			Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::SurfaceCrawl) - Before, static_cast<uint64>(120));
		TestEqual(TEXT("Flat crawl keeps height"), Crawler.Location.Z, FloorZ, 0.5);
		TestEqual(TEXT("Flat crawl advances"), Crawler.Location.X, static_cast<double>(TestSpeed), 1.0);
		TestTrue(TEXT("Flat crawl keeps floor normal"), Crawler.Normal.Equals(FVector::UpVector, 1e-3));
		TestEqual(TEXT("Flat crawl never loses the surface"), Crawler.Lost, 0);
		TestEqual(TEXT("Flat crawl never penetrates"), Crawler.Penetrations, 0);
	}

	// 2. 정지: query 없이 제자리다.
	{
		const uint64 Before = Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::SurfaceCrawl);
		FVector Next, NextNormal;
		const EStepResult Result = Step(*Fixture.Collision, Fixture.Params, FVector(0, 0, FloorZ), FVector::UpVector, FVector::ZeroVector,
			TestDeltaTime, Next, NextNormal);
		TestTrue(TEXT("Stationary crawler stays attached"), Result == EStepResult::Attached);
		TestEqual(TEXT("Stationary crawler issues no query"), Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::SurfaceCrawl) - Before, static_cast<uint64>(0));
	}

	// 3. 천장: 중력과 무관하게 붙은 채 기어간다.
	{
		FCrawler Crawler;
		Crawler.Location = FVector(0, 0, CeilingZ);
		Crawler.Normal = -FVector::UpVector;
		for (int32 Frame = 0; Frame < 60; ++Frame)
			Crawler.Tick(Fixture, FVector(0, TestSpeed, 0));
		TestEqual(TEXT("Ceiling crawl keeps height"), Crawler.Location.Z, CeilingZ, 0.5);
		TestEqual(TEXT("Ceiling crawl advances"), Crawler.Location.Y, static_cast<double>(TestSpeed), 1.0);
		TestTrue(TEXT("Ceiling crawl keeps ceiling normal"), Crawler.Normal.Equals(-FVector::UpVector, 1e-3));
		TestEqual(TEXT("Ceiling crawl never falls"), Crawler.Lost, 0);
		TestEqual(TEXT("Ceiling crawl never penetrates"), Crawler.Penetrations, 0);
	}

	// 4. 허공: 붙을 표면이 없으면 표면을 잃는다.
	{
		FVector Next, NextNormal;
		const EStepResult Result = Step(*Fixture.Collision, Fixture.Params, FVector(0, 0, 300), FVector::UpVector, FVector(TestSpeed, 0, 0),
			TestDeltaTime, Next, NextNormal);
		TestTrue(TEXT("Crawler in mid air loses the surface"), Result == EStepResult::Lost);
	}
	return true;
}

bool FLNPSurfaceCrawlClimbTest::RunTest(const FString& Parameters)
{
	FCrawlWorld Fixture(TEXT("LNPSurfaceCrawlClimbTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 바닥 top z=10. 블록 x∈[500,700], y∈[-500,500], z∈[0,400] — 앞면(-X)·지붕·뒷면(+X)이 각각 오목·볼록 모서리를 만든다.
	Fixture.Box(FVector(0, 0, 0), FVector(40, 40, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Box(FVector(600, 0, 200), FVector(2, 10, 4), TEXT("LNPStaticBlocker"));
	Fixture.Publish();
	const float Gap = Fixture.Params.SurfaceGap;
	const double FloorZ = 10.0 + TestRadius + Gap;
	const double RoofZ = 400.0 + TestRadius + Gap;

	FCrawler Crawler;
	Crawler.Location = FVector(0, 0, FloorZ);

	// 1. 바닥 → 앞면(오목) → 지붕(볼록). 지붕 한가운데 목표로 기어 올라간다.
	const bool bReachedRoof = Crawler.CrawlTo(Fixture, FVector(600, 0, RoofZ), 600);
	TestTrue(TEXT("Crawler climbs onto the roof"), bReachedRoof);
	TestEqual(TEXT("Crawler rests on the roof"), Crawler.Location.Z, RoofZ, 1.0);
	TestTrue(TEXT("Crawler adopts the roof normal"), Crawler.Normal.Equals(FVector::UpVector, 1e-2));
	TestTrue(TEXT("Crawler turned onto the wall on the way"), Crawler.MinNormalZ < 0.1);

	// 2. 지붕 → 뒷면(볼록) → 바닥(오목). 블록 뒤 바닥 목표로 기어 내려간다.
	Crawler.MinNormalZ = 1.0;
	const bool bReachedFloor = Crawler.CrawlTo(Fixture, FVector(1100, 0, FloorZ), 600);
	TestTrue(TEXT("Crawler descends back to the floor"), bReachedFloor);
	TestEqual(TEXT("Crawler rests on the floor"), Crawler.Location.Z, FloorZ, 1.0);
	TestTrue(TEXT("Crawler adopts the floor normal"), Crawler.Normal.Equals(FVector::UpVector, 1e-2));
	TestTrue(TEXT("Crawler turned onto the back wall on the way"), Crawler.MinNormalZ < 0.1);

	TestEqual(TEXT("Climbing never loses the surface"), Crawler.Lost, 0);
	TestEqual(TEXT("Climbing never penetrates"), Crawler.Penetrations, 0);
	return true;
}

bool FLNPSurfaceCrawlFallTest::RunTest(const FString& Parameters)
{
	FCrawlWorld Fixture(TEXT("LNPSurfaceCrawlFallTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 바닥 top z=10. 천장 bottom z=590. 등록하지 않은(Unknown) 벽 x∈[290,310].
	Fixture.Box(FVector(0, 0, 0), FVector(40, 40, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Box(FVector(-1000, 0, 600), FVector(10, 10, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Box(FVector(300, 0, 160), FVector(0.2, 10, 3), TEXT("LNPStaticBlocker"), /*bRegister=*/false);
	Fixture.Publish();
	const float Gap = Fixture.Params.SurfaceGap;
	const double FloorZ = 10.0 + TestRadius + Gap;

	// 1. 천장에서 넉백을 받으면 구 중심 중력으로 떨어져 바닥에 착지하고 바닥 법선으로 다시 붙는다.
	{
		FCrawler Crawler;
		Crawler.Location = FVector(-1000, 0, 590.0 - TestRadius - Gap);
		Crawler.Normal = -FVector::UpVector;
		Crawler.PhysVelocity = FVector(0, 200, 0);
		for (int32 Frame = 0; Frame < 120 && Crawler.Landings == 0; ++Frame)
			Crawler.Tick(Fixture, FVector::ZeroVector);
		TestEqual(TEXT("Knocked crawler lands once"), Crawler.Landings, 1);
		TestTrue(TEXT("Landed crawler is attached"), Crawler.PhysVelocity.IsZero());
		TestEqual(TEXT("Landed crawler rests on the floor"), Crawler.Location.Z, FloorZ, 1.0);
		TestTrue(TEXT("Landed crawler adopts the floor normal"), Crawler.Normal.Equals(FVector::UpVector, 1e-2));
		TestEqual(TEXT("Falling never penetrates"), Crawler.Penetrations, 0);
	}

	// 2. Unknown 벽에는 붙지 않는다(D-037). 벽 앞에서 서고 바닥 법선을 유지한다.
	{
		FCrawler Crawler;
		Crawler.Location = FVector(0, 0, FloorZ);
		for (int32 Frame = 0; Frame < 120; ++Frame)
			Crawler.Tick(Fixture, FVector(TestSpeed, 0, 0));
		TestTrue(TEXT("Unknown wall blocks the crawler"), Crawler.Blocked > 0);
		TestTrue(TEXT("Crawler stops in front of the unknown wall"), Crawler.Location.X < 290.0 - TestRadius + 1.0);
		TestTrue(TEXT("Crawler does not climb the unknown wall"), Crawler.Normal.Equals(FVector::UpVector, 1e-3));
		TestEqual(TEXT("Unknown wall never penetrated"), Crawler.Penetrations, 0);
	}
	return true;
}

#endif
