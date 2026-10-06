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
#include "Enemy/LNPEnemyMassTypes.h"
#include "Enemy/LNPEnemySurfaceMovement.h"
#include "DynamicTerrain/LNPDynamicTerrainSubsystem.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceCrawlDetourTest,
	"LootNPop.SurfaceNavigation.SurfaceCrawl.Detour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceCrawlHomeApproachTest,
	"LootNPop.SurfaceNavigation.SurfaceCrawl.HomeApproach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceCrawlCreviceTest,
	"LootNPop.SurfaceNavigation.SurfaceCrawl.CreviceContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceCrawlDynamicCarryTest,
	"LootNPop.SurfaceNavigation.SurfaceCrawl.DynamicSupportCarry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceCrawlWanderSegmentTest,
	"LootNPop.SurfaceNavigation.SurfaceCrawl.WanderSegment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	using namespace LNPSurfaceCrawl;

	constexpr float CrawlTestDeltaTime = 1.f / 60.f;
	constexpr float CrawlTestRadius = 40.f;
	constexpr float CrawlTestSpeed = 300.f;

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

			Params.BodyRadius = CrawlTestRadius;
			// 벽보행 몸은 구다 — 낙하도 반높이 = 반지름인 캡슐(= 구)로 sweep한다.
			FallParams.GravityOrigin = FVector(0, 0, 1.0e7);
			FallParams.CapsuleRadius = CrawlTestRadius;
			FallParams.CapsuleHalfHeight = CrawlTestRadius;
			FallParams.bLandOnAnyKnownSolid = true;
		}

		~FCrawlWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		UStaticMeshComponent* Box(const FVector& Location, const FVector& Scale, const FName Profile, const bool bRegister = true,
			const FRotator& Rotation = FRotator::ZeroRotator, const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube"))
		{
			AActor* Owner = World->SpawnActor<AActor>();
			UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Owner);
			Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
			Mesh->SetMobility(EComponentMobility::Static);
			Mesh->SetCollisionProfileName(Profile);
			Mesh->SetWorldLocation(Location);
			Mesh->SetWorldRotation(Rotation);
			Mesh->SetWorldScale3D(Scale);
			Owner->SetRootComponent(Mesh);
			Mesh->RegisterComponent();
			if (bRegister)
			{
				HitIdentity->RegisterRuntimeSource(Mesh);
			}
			return Mesh;
		}

		void Publish() const { HitIdentity->Tick(0.f); }

		/** 몸 구가 지형에 파고든 깊이(cm). 겹치지 않으면 0. 부하 harness 관통 검사와 같은 시작 겹침 깊이다. */
		float PenetrationDepth(const FVector& Location, const FVector& Normal) const
		{
			FLNPWorldHit Hit;
			return Collision->SweepSphereWorld(Location, Location + Normal * 0.1f, CrawlTestRadius,
				FLNPWorldQueryParams(ELNPWorldQueryClass::DebugValidation), Hit) && Hit.bStartPenetrating ? Hit.PenetrationDepth : 0.f;
		}

		/** 몸 구가 지형과 겹쳐 있는가. 측정용 query라 SurfaceCrawl 분류 수에 섞이지 않게 DebugValidation으로 잰다. */
		bool IsPenetrating(const FVector& Location, const FVector& Normal) const
		{
			FLNPWorldHit Hit;
			return Collision->SweepSphereWorld(Location, Location + Normal * 0.1f, CrawlTestRadius,
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
		int32 Crushes = 0;
		double MinNormalZ = 1.0;

		/** bDetour면 이동 프로세서의 교전 추격처럼 막힘 우회(UpdateDetour)를 쓴다. */
		bool bDetour = false;
		FDetourState Detour;
		int32 DetourStarts = 0;

		/** 목표에 도착 판정 거리(GoalDistanceSq) Tolerance 안으로 들어오면 true. */
		bool CrawlTo(const FCrawlWorld& Fixture, const FVector& Goal, const int32 MaxFrames, const float Tolerance = 30.f)
		{
			FDetourParams DetourParams;
			DetourParams.StallRadius = CrawlTestRadius;
			for (int32 Frame = 0; Frame < MaxFrames; ++Frame)
			{
				if (GoalDistanceSq(Location, Normal, Goal, CrawlTestRadius) < FMath::Square(Tolerance) && PhysVelocity.IsZero())
				{
					return true;
				}
				const FVector GravityUp = (Fixture.FallParams.GravityOrigin - Location).GetSafeNormal();
				FVector MoveDir = SteerDirection(Location, Normal, Goal, GravityUp, CrawlTestRadius);
				if (bDetour && PhysVelocity.IsZero())
				{
					const float Before = Detour.DetourTime;
					MoveDir = UpdateDetour(Detour, DetourParams, Location, Normal, MoveDir, Goal - Location, GravityUp,
						FVector::RightVector, 1, CrawlTestDeltaTime);
					DetourStarts += Detour.DetourTime > Before ? 1 : 0;
				}
				Tick(Fixture, MoveDir * CrawlTestSpeed);
			}
			return false;
		}

		void Tick(const FCrawlWorld& Fixture, const FVector& Velocity)
		{
			FVector Next = Location;
			if (!PhysVelocity.IsZero())
			{
				FVector LandingNormal = (Fixture.FallParams.GravityOrigin - Location).GetSafeNormal();
				if (LNPEnemyExactMovement::StepAirborne(*Fixture.Collision, Fixture.FallParams, Location, PhysVelocity, CrawlTestDeltaTime, Next,
					nullptr, &LandingNormal))
				{
					++Landings;
					FVector Attached, AttachedNormal;
					Normal = LandingNormal;
					if (Attach(*Fixture.Collision, Fixture.Params, Next, -LandingNormal, Fixture.Params.SurfaceGap + 1.f, Attached, AttachedNormal))
					{
						Next = Attached;
						Normal = AttachedNormal;
					}
					else
					{
						// 이동 프로세서와 같다: 착지 자리가 겹쳐 있으면 풀고 한 번 더 붙는다.
						FVector Free;
						Depenetrate(*Fixture.Collision, Fixture.Params, Next, Free);
						Next = Free;
						if (Attach(*Fixture.Collision, Fixture.Params, Free, -LandingNormal, Fixture.Params.SurfaceGap + 1.f, Attached, AttachedNormal))
						{
							Next = Attached;
							Normal = AttachedNormal;
						}
					}
				}
			}
			else
			{
				FVector NextNormal;
				const EStepResult Result = Step(*Fixture.Collision, Fixture.Params, Location, Normal, Velocity, CrawlTestDeltaTime, Next, NextNormal);
				Normal = NextNormal;
				Blocked += Result == EStepResult::Blocked ? 1 : 0;
				Crushes += Result == EStepResult::Crushed ? 1 : 0;
				if (Result == EStepResult::Lost)
				{
					++Lost;
					const FVector GravityUp = (Fixture.FallParams.GravityOrigin - Next).GetSafeNormal();
					PhysVelocity = FVector::VectorPlaneProject(Velocity, Normal) - GravityUp * Fixture.FallParams.GravityStrength * CrawlTestDeltaTime;
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
	const double FloorZ = 10.0 + CrawlTestRadius + Fixture.Params.SurfaceGap;
	const double CeilingZ = 590.0 - CrawlTestRadius - Fixture.Params.SurfaceGap;

	// 1. 평지: 붙은 채 높이와 법선을 유지한다. 이동 프레임마다 query는 접평면 sweep + probe 2회다.
	{
		FCrawler Crawler;
		Crawler.Location = FVector(0, 0, FloorZ);
		const uint64 Before = Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::SurfaceCrawl);
		for (int32 Frame = 0; Frame < 60; ++Frame)
			Crawler.Tick(Fixture, FVector(CrawlTestSpeed, 0, 0));
		TestEqual(TEXT("Flat crawl uses two queries per moving frame"),
			Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::SurfaceCrawl) - Before, static_cast<uint64>(120));
		TestEqual(TEXT("Flat crawl keeps height"), Crawler.Location.Z, FloorZ, 0.5);
		TestEqual(TEXT("Flat crawl advances"), Crawler.Location.X, static_cast<double>(CrawlTestSpeed), 1.0);
		TestTrue(TEXT("Flat crawl keeps floor normal"), Crawler.Normal.Equals(FVector::UpVector, 1e-3));
		TestEqual(TEXT("Flat crawl never loses the surface"), Crawler.Lost, 0);
		TestEqual(TEXT("Flat crawl never penetrates"), Crawler.Penetrations, 0);
	}

	// 2. 정지: query 없이 제자리다.
	{
		const uint64 Before = Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::SurfaceCrawl);
		FVector Next, NextNormal;
		const EStepResult Result = Step(*Fixture.Collision, Fixture.Params, FVector(0, 0, FloorZ), FVector::UpVector, FVector::ZeroVector,
			CrawlTestDeltaTime, Next, NextNormal);
		TestTrue(TEXT("Stationary crawler stays attached"), Result == EStepResult::Attached);
		TestEqual(TEXT("Stationary crawler issues no query"), Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::SurfaceCrawl) - Before, static_cast<uint64>(0));
	}

	// 3. 천장: 중력과 무관하게 붙은 채 기어간다.
	{
		FCrawler Crawler;
		Crawler.Location = FVector(0, 0, CeilingZ);
		Crawler.Normal = -FVector::UpVector;
		for (int32 Frame = 0; Frame < 60; ++Frame)
			Crawler.Tick(Fixture, FVector(0, CrawlTestSpeed, 0));
		TestEqual(TEXT("Ceiling crawl keeps height"), Crawler.Location.Z, CeilingZ, 0.5);
		TestEqual(TEXT("Ceiling crawl advances"), Crawler.Location.Y, static_cast<double>(CrawlTestSpeed), 1.0);
		TestTrue(TEXT("Ceiling crawl keeps ceiling normal"), Crawler.Normal.Equals(-FVector::UpVector, 1e-3));
		TestEqual(TEXT("Ceiling crawl never falls"), Crawler.Lost, 0);
		TestEqual(TEXT("Ceiling crawl never penetrates"), Crawler.Penetrations, 0);
	}

	// 4. 허공: 붙을 표면이 없으면 표면을 잃는다.
	{
		FVector Next, NextNormal;
		const EStepResult Result = Step(*Fixture.Collision, Fixture.Params, FVector(0, 0, 300), FVector::UpVector, FVector(CrawlTestSpeed, 0, 0),
			CrawlTestDeltaTime, Next, NextNormal);
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
	const double FloorZ = 10.0 + CrawlTestRadius + Gap;
	const double RoofZ = 400.0 + CrawlTestRadius + Gap;

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
	const double FloorZ = 10.0 + CrawlTestRadius + Gap;

	// 1. 천장에서 넉백을 받으면 구 중심 중력으로 떨어져 바닥에 착지하고 바닥 법선으로 다시 붙는다.
	{
		FCrawler Crawler;
		Crawler.Location = FVector(-1000, 0, 590.0 - CrawlTestRadius - Gap);
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
			Crawler.Tick(Fixture, FVector(CrawlTestSpeed, 0, 0));
		TestTrue(TEXT("Unknown wall blocks the crawler"), Crawler.Blocked > 0);
		TestTrue(TEXT("Crawler stops in front of the unknown wall"), Crawler.Location.X < 290.0 - CrawlTestRadius + 1.0);
		TestTrue(TEXT("Crawler does not climb the unknown wall"), Crawler.Normal.Equals(FVector::UpVector, 1e-3));
		TestEqual(TEXT("Unknown wall never penetrated"), Crawler.Penetrations, 0);
	}
	return true;
}

bool FLNPSurfaceCrawlDetourTest::RunTest(const FString& Parameters)
{
	FCrawlWorld Fixture(TEXT("LNPSurfaceCrawlDetourTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 바닥 top z=10.
	// 돌출부: 벽 x∈[500,700], y∈[-1000,1000], z∈[0,800]. 벽 앞면에 붙은 판 x∈[200,500], y∈[-300,300], z∈[400,440](밑면 = 천장).
	// 테이블: 기둥 x,y∈[-2050,-1950]×[-50,50], z∈[0,400] 위에 판 x∈[-2300,-1700], y∈[-300,300], z∈[400,440].
	Fixture.Box(FVector(0, 0, 0), FVector(80, 80, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Box(FVector(600, 0, 400), FVector(2, 20, 8), TEXT("LNPStaticBlocker"));
	Fixture.Box(FVector(350, 0, 420), FVector(3, 6, 0.4), TEXT("LNPStaticBlocker"));
	Fixture.Box(FVector(-2000, 0, 200), FVector(1, 1, 4), TEXT("LNPStaticBlocker"));
	Fixture.Box(FVector(-2000, 0, 420), FVector(6, 6, 0.4), TEXT("LNPStaticBlocker"));
	Fixture.Publish();
	const float Gap = Fixture.Params.SurfaceGap;
	const double FloorZ = 10.0 + CrawlTestRadius + Gap;

	// 1. 돌출부: 조향만으로는 벽 ↔ 돌출부 밑면을 오가며 벽 꼭대기에 닿지 못한다. 우회하면 닿는다.
	const FVector WallTop(600, 0, 800.0 + CrawlTestRadius + Gap);
	{
		FCrawler Plain;
		Plain.Location = FVector(0, 0, FloorZ);
		TestFalse(TEXT("Plain steering stays trapped under the ledge"), Plain.CrawlTo(Fixture, WallTop, 900));

		FCrawler Crawler;
		Crawler.bDetour = true;
		Crawler.Location = FVector(0, 0, FloorZ);
		TestTrue(TEXT("Detour climbs past the ledge to the wall top"), Crawler.CrawlTo(Fixture, WallTop, 3600));
		TestTrue(TEXT("Ledge escape needed a detour"), Crawler.DetourStarts > 0);
		TestEqual(TEXT("Ledge detour never penetrates"), Crawler.Penetrations, 0);
		AddInfo(FString::Printf(TEXT("Ledge detours=%d lost=%d"), Crawler.DetourStarts, Crawler.Lost));
	}

	// 2. 테이블: 기둥을 타고 판 밑면에 붙은 뒤, 판 가장자리를 돌아 윗면 목표에 닿는다.
	const FVector TableTop(-2000, 0, 440.0 + CrawlTestRadius + Gap);
	{
		FCrawler Plain;
		Plain.Location = FVector(-2800, 0, FloorZ);
		TestFalse(TEXT("Plain steering stays trapped under the table"), Plain.CrawlTo(Fixture, TableTop, 900));

		FCrawler Crawler;
		Crawler.bDetour = true;
		Crawler.Location = FVector(-2800, 0, FloorZ);
		TestTrue(TEXT("Detour reaches the table top"), Crawler.CrawlTo(Fixture, TableTop, 3600));
		TestTrue(TEXT("Table escape needed a detour"), Crawler.DetourStarts > 0);
		TestEqual(TEXT("Table detour never penetrates"), Crawler.Penetrations, 0);
		AddInfo(FString::Printf(TEXT("Table detours=%d lost=%d"), Crawler.DetourStarts, Crawler.Lost));
	}

	// 3. 평지 직진 추격에는 우회가 끼어들지 않는다.
	{
		FCrawler Crawler;
		Crawler.bDetour = true;
		Crawler.Location = FVector(-1000, 2000, FloorZ);
		TestTrue(TEXT("Flat chase arrives"), Crawler.CrawlTo(Fixture, FVector(1000, 2000, FloorZ), 600));
		TestEqual(TEXT("Flat chase never detours"), Crawler.DetourStarts, 0);
	}
	return true;
}

bool FLNPSurfaceCrawlHomeApproachTest::RunTest(const FString& Parameters)
{
	FHomeApproachParams Params;
	Params.FarDistance = 2580.f;
	Params.ProgressMargin = 80.f;
	Params.Timeout = 10.f;
	constexpr float Dt = 1.f / 60.f;

	// Home까지 몇 초 동안 계측해 처음으로 재귀속이 요청된 시각(없으면 -1).
	const auto Run = [&](const TFunctionRef<float(float)> DistanceAt, const float Seconds)
	{
		FHomeApproachState State;
		for (int32 Frame = 0; Frame * Dt < Seconds; ++Frame)
		{
			if (UpdateHomeApproach(State, Params, DistanceAt(Frame * Dt), Dt))
				return Frame * Dt;
		}
		return -1.f;
	};

	// 배회 속도(180cm/s)로 다가오면 30초 동안 재귀속하지 않는다.
	TestEqual(TEXT("Approaching home never rehomes"), Run([](float T) { return 8000.f - 180.f * T; }, 30.f), -1.f);
	// 배회 거리 안에서는 멈춰 있어도 재지 않는다.
	TestEqual(TEXT("Inside wander range never rehomes"), Run([](float) { return 2000.f; }, 30.f), -1.f);
	// 다른 섬 밑에 걸려 멈추면 10초 뒤 재귀속한다.
	const float StallTime = Run([](float) { return 4000.f; }, 30.f);
	TestTrue(FString::Printf(TEXT("Stalled far from home rehomes after timeout (%.2fs)"), StallTime),
		StallTime >= Params.Timeout - Dt && StallTime <= Params.Timeout + 2.f * Dt);
	// 몸 반지름보다 작은 떨림은 진전이 아니다.
	const float JitterTime = Run([](float T) { return 4000.f - 50.f * FMath::Abs(FMath::Sin(T * 3.f)); }, 30.f);
	TestTrue(FString::Printf(TEXT("Jitter below margin still rehomes (%.2fs)"), JitterTime),
		JitterTime >= Params.Timeout - Dt && JitterTime <= Params.Timeout + 0.5f);
	// 5초 다가간 뒤 막히면 마지막 진전부터 10초 뒤다.
	const float LateTime = Run([](float T) { return 5000.f - 180.f * FMath::Min(T, 5.f); }, 30.f);
	TestTrue(FString::Printf(TEXT("Stall after progress rehomes from last progress (%.2fs)"), LateTime),
		LateTime >= 5.f + Params.Timeout - 1.f && LateTime <= 5.f + Params.Timeout + 0.5f);

	// 후보 선택: 직선거리 최소, 같은 거리면 앞 인덱스, 후보 없음.
	const TArray<FVector> Pods = { FVector(1000, 0, 0), FVector(0, 300, 0), FVector(0, -300, 0) };
	TestEqual(TEXT("Nearest pod by straight distance"), SelectNearestPod(FVector::ZeroVector, Pods), 1);
	TestEqual(TEXT("Nearest pod near the far one"), SelectNearestPod(FVector(900, 0, 0), Pods), 0);
	TestEqual(TEXT("No pod candidates"), SelectNearestPod(FVector::ZeroVector, TArray<FVector>()), static_cast<int32>(INDEX_NONE));
	// 이미 다가가지 못한 Pod는 더 가까워도 건너뛴다. 모두 빠지면 후보가 없다.
	TestEqual(TEXT("Failed pods are skipped"), SelectNearestPod(FVector::ZeroVector, Pods, TArray<bool>{ false, true, false }), 2);
	TestEqual(TEXT("All pods failed leaves no candidate"),
		SelectNearestPod(FVector::ZeroVector, Pods, TArray<bool>{ true, true, true }), static_cast<int32>(INDEX_NONE));
	return true;
}

bool FLNPSurfaceCrawlCreviceTest::RunTest(const FString& Parameters)
{
	// 부하 harness가 실측한 얕은 관통(0.11~0.28cm, Blocker 프랍의 붙은 면이 아닌 면)의 두 경로를 재현한다(Phase 14 단위 6b).
	// harness와 같이 깊이 0.1cm 초과만 관통으로 센다 — 이하는 옆면 접촉이다.
	constexpr float PenetrationTolerance = 0.1f;
	FCrawlWorld Fixture(TEXT("LNPSurfaceCrawlCreviceTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 1. 붙은 채 V자 틈: 벽 A(+X 면 x=0, y≤0)와 벽 B(법선 (-0.5,-0.866,0), 원점에서 A와 60°로 만난다). 중력과 무관하게 수직 벽만 쓴다.
	const FVector NormalB(-0.5, -0.8660254, 0);
	const FVector TangentB(0.8660254, -0.5, 0);
	Fixture.Box(FVector(-10, -490, 200), FVector(0.2, 10, 4), TEXT("LNPStaticBlocker"));
	Fixture.Box(TangentB * 490.0 - NormalB * 10.0 + FVector(0, 0, 200), FVector(0.2, 10, 4), TEXT("LNPStaticBlocker"), true, FRotator(0, -120, 0));
	// 2. 공중 미끄럼: 바닥 top z=10(x∈[1000,3000])과 급경사 Blocker 면(법선·Up 내적 0.44, 걸을 수 없음)이 만드는 V자 골.
	Fixture.Box(FVector(2000, 0, 0), FVector(20, 20, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Box(FVector(2000, 0, 300), FVector(0.2, 10, 8), TEXT("LNPStaticBlocker"), true, FRotator(-26, 0, 0));
	Fixture.Publish();

	{
		FCrawler Crawler;
		Crawler.Location = FVector(CrawlTestRadius + Fixture.Params.SurfaceGap, -400, 200);
		Crawler.Normal = FVector(1, 0, 0);
		int32 Deep = 0;
		float MaxDepth = 0.f;
		for (int32 Frame = 0; Frame < 180; ++Frame)
		{
			Crawler.Tick(Fixture, FVector(0, CrawlTestSpeed, 0));
			const float Depth = Fixture.PenetrationDepth(Crawler.Location, Crawler.Normal);
			Deep += Depth > PenetrationTolerance ? 1 : 0;
			MaxDepth = FMath::Max(MaxDepth, Depth);
		}
		// 틈에 박힌 채 멈춘다(공격·정지). 이동량 0이면 Step은 query를 하지 않는다.
		const float RestDepth = Fixture.PenetrationDepth(Crawler.Location, Crawler.Normal);
		AddInfo(FString::Printf(TEXT("Crevice attached: deepFrames=%d maxDepth=%.4f restDepth=%.4f normal=%s"),
			Deep, MaxDepth, RestDepth, *Crawler.Normal.ToString()));
		TestEqual(TEXT("Crawling into a V crevice never penetrates"), Deep, 0);
		TestTrue(TEXT("Crawler resting in a V crevice is not embedded"), RestDepth <= PenetrationTolerance);
	}

	{
		// 경사면 위에서 넉백으로 떨어져 경사면을 따라 골로 미끄러진다.
		FCrawler Crawler;
		// 경사면(-X 면은 z=500에서 x≈2086)에서 10cm 떨어진 공중. 경사면 쪽으로 밀려 붙은 뒤 골(x≈1848, z=10)로 미끄러진다.
		Crawler.Location = FVector(2030, 0, 500);
		Crawler.Normal = FVector::UpVector;
		Crawler.PhysVelocity = FVector(100, 0, 0);
		int32 Deep = 0;
		int32 AirFrames = 0;
		float MaxDepth = 0.f;
		for (int32 Frame = 0; Frame < 240 && Crawler.Landings == 0; ++Frame)
		{
			Crawler.Tick(Fixture, FVector::ZeroVector);
			++AirFrames;
			const float Depth = Fixture.PenetrationDepth(Crawler.Location, FVector::UpVector);
			Deep += Depth > PenetrationTolerance ? 1 : 0;
			MaxDepth = FMath::Max(MaxDepth, Depth);
		}
		AddInfo(FString::Printf(TEXT("Crevice airborne: airFrames=%d landings=%d deepFrames=%d maxDepth=%.4f location=%s"),
			AirFrames, Crawler.Landings, Deep, MaxDepth, *Crawler.Location.ToString()));
		TestEqual(TEXT("Sliding down a steep blocker never penetrates"), Deep, 0);
		TestEqual(TEXT("Sliding crawler eventually lands"), Crawler.Landings, 1);
	}

	// 3·4. 현장 관통 source와 같은 엔진 기본 도형 프랍(볼록 hull collision). 바닥 top z=10(x∈[-5000,-3000]).
	// 원뿔(높이 200, 밑면 반지름 100, 옆면 법선·Up 내적 0.447)과 원기둥(높이 200, 반지름 100).
	Fixture.Box(FVector(-4000, 0, 0), FVector(20, 20, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Box(FVector(-4500, 0, 110), FVector(2, 2, 2), TEXT("LNPStaticBlocker"), true, FRotator::ZeroRotator, TEXT("/Engine/BasicShapes/Cone.Cone"));
	Fixture.Box(FVector(-3500, 0, 110), FVector(2, 2, 2), TEXT("LNPStaticBlocker"), true, FRotator::ZeroRotator, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Fixture.Publish();
	const auto Measure = [&](FCrawler& Crawler, const FVector& Velocity, const int32 Frames, int32& OutDeep, float& OutMax)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Crawler.Tick(Fixture, Velocity);
			const FVector Up = Crawler.PhysVelocity.IsZero() ? Crawler.Normal : FVector::UpVector;
			const float Depth = Fixture.PenetrationDepth(Crawler.Location, Up);
			OutDeep += Depth > PenetrationTolerance ? 1 : 0;
			OutMax = FMath::Max(OutMax, Depth);
		}
	};
	{
		// 원뿔 옆면 여러 방위에서 떨어뜨린다. 옆면을 미끄러져 밑동 골에 내려앉는다.
		int32 Deep = 0;
		float MaxDepth = 0.f;
		int32 Landed = 0;
		for (int32 Azimuth = 0; Azimuth < 8; ++Azimuth)
		{
			const FVector Dir(FMath::Cos(Azimuth * PI / 4.0), FMath::Sin(Azimuth * PI / 4.0), 0.0);
			FCrawler Crawler;
			Crawler.Location = FVector(-4500, 0, 0) + Dir * 60.0 + FVector(0, 0, 330);
			Crawler.PhysVelocity = -Dir * 50.0;
			Measure(Crawler, FVector::ZeroVector, 180, Deep, MaxDepth);
			Landed += Crawler.Landings > 0 ? 1 : 0;
		}
		AddInfo(FString::Printf(TEXT("Cone airborne: deepFrames=%d maxDepth=%.4f landed=%d/8"), Deep, MaxDepth, Landed));
		TestEqual(TEXT("Sliding down a cone never penetrates"), Deep, 0);
	}
	{
		// 원기둥을 향해 기어가 옆면에 오르고, 옆면을 돌아 기어간 뒤 멈춘다.
		int32 Deep = 0;
		float MaxDepth = 0.f;
		const double FloorZ = 10.0 + CrawlTestRadius + Fixture.Params.SurfaceGap;
		for (int32 Azimuth = 0; Azimuth < 8; ++Azimuth)
		{
			const FVector Dir(FMath::Cos(Azimuth * PI / 4.0 + 0.1), FMath::Sin(Azimuth * PI / 4.0 + 0.1), 0.0);
			FCrawler Crawler;
			Crawler.Location = FVector(-3500, 0, FloorZ) + Dir * 400.0;
			Measure(Crawler, -Dir * CrawlTestSpeed, 120, Deep, MaxDepth);
			Measure(Crawler, FVector::CrossProduct(FVector::UpVector, Dir) * CrawlTestSpeed, 120, Deep, MaxDepth);
			Measure(Crawler, FVector::ZeroVector, 10, Deep, MaxDepth);
		}
		AddInfo(FString::Printf(TEXT("Cylinder attached: deepFrames=%d maxDepth=%.4f"), Deep, MaxDepth));
		TestEqual(TEXT("Crawling around a cylinder never penetrates"), Deep, 0);
	}

	// 5·6. 현장 재현 지점의 모양: 인접한 프랍 둘이 만드는 주머니. 바닥 top z=10(x∈[-8000,-6000]).
	// 원뿔 둘(밑면이 맞닿음): 경사면 골의 폭이 몸 지름보다 좁아 바닥에 닿지 못한다. 원기둥 둘(사이 60cm): 바닥과 세 면 구석을 만든다.
	Fixture.Box(FVector(-7000, 0, 0), FVector(20, 20, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Box(FVector(-7600, 0, 110), FVector(2, 2, 2), TEXT("LNPStaticBlocker"), true, FRotator::ZeroRotator, TEXT("/Engine/BasicShapes/Cone.Cone"));
	Fixture.Box(FVector(-7400, 0, 110), FVector(2, 2, 2), TEXT("LNPStaticBlocker"), true, FRotator::ZeroRotator, TEXT("/Engine/BasicShapes/Cone.Cone"));
	Fixture.Box(FVector(-6630, 0, 110), FVector(2, 2, 2), TEXT("LNPStaticBlocker"), true, FRotator::ZeroRotator, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Fixture.Box(FVector(-6370, 0, 110), FVector(2, 2, 2), TEXT("LNPStaticBlocker"), true, FRotator::ZeroRotator, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Fixture.Publish();
	{
		FCrawler Crawler;
		Crawler.Location = FVector(-7500, 0, 300);
		Crawler.PhysVelocity = FVector(0, 5, 0);
		int32 Deep = 0;
		float MaxDepth = 0.f;
		Measure(Crawler, FVector::ZeroVector, 240, Deep, MaxDepth);
		AddInfo(FString::Printf(TEXT("Cone pocket: deepFrames=%d maxDepth=%.4f landings=%d airborne=%d location=%s"),
			Deep, MaxDepth, Crawler.Landings, Crawler.PhysVelocity.IsZero() ? 0 : 1, *Crawler.Location.ToString()));
		TestEqual(TEXT("Falling into a pocket between cones never penetrates"), Deep, 0);
		TestTrue(TEXT("Crawler falling into a pocket between cones ends up attached"), Crawler.PhysVelocity.IsZero());
	}
	{
		// 두 원기둥 사이 틈으로 계속 밀고 들어간다(교전 접근처럼 목표가 틈 너머에 있다).
		const double FloorZ = 10.0 + CrawlTestRadius + Fixture.Params.SurfaceGap;
		FCrawler Crawler;
		Crawler.Location = FVector(-6500, -400, FloorZ);
		int32 Deep = 0;
		float MaxDepth = 0.f;
		Measure(Crawler, FVector(0, CrawlTestSpeed, 0), 240, Deep, MaxDepth);
		Measure(Crawler, FVector::ZeroVector, 10, Deep, MaxDepth);
		AddInfo(FString::Printf(TEXT("Cylinder pocket: deepFrames=%d maxDepth=%.4f location=%s normal=%s"),
			Deep, MaxDepth, *Crawler.Location.ToString(), *Crawler.Normal.ToString()));
		TestEqual(TEXT("Pushing into a pocket between cylinders never penetrates"), Deep, 0);
	}
	{
		// 7. 반복 겹침 해소: 두 원기둥(사이 60cm)과 바닥에 동시에 파고든 몸을 밀어낸다. 한 면 법선으로 한 번만 밀면 다른 면에 남는다.
		// 틈 입구(y=-40): 두 원기둥에 약 4cm, 바닥에 5cm 겹친다. 틈 안쪽은 몸 지름보다 좁아 바깥으로만 빠져나갈 수 있다.
		const FVector Embedded(-6500, -40, 10.0 + CrawlTestRadius - 5.0);
		TestTrue(TEXT("Embedded body starts penetrating"), Fixture.PenetrationDepth(Embedded, FVector::UpVector) > PenetrationTolerance);
		FVector Free;
		TestTrue(TEXT("Depenetrate frees a body wedged between two cylinders and the floor"),
			Depenetrate(*Fixture.Collision, Fixture.Params, Embedded, Free));
		TestEqual(TEXT("Depenetrated body is clear"), Fixture.PenetrationDepth(Free, FVector::UpVector), 0.f);
		AddInfo(FString::Printf(TEXT("Depenetrate: from=%s to=%s"), *Embedded.ToString(), *Free.ToString()));
	}
	{
		// 8. 내려오는 패널에 깔림(현장 핫스팟 = 이동 패널 마커). 바닥 위 개체 위로 Dynamic 판(LNPDynamicTerrain)이 틱당 5cm씩
		// 내려온다. 겹침 해소가 개체를 바닥 안으로 밀어 넣으면 안 되고, 빈 공간이 없어지면 압사(Crushed)로 끝나야 한다.
		// 이동 중(Step)과 정지 중(ResolveStationaryOverlap — 이동 프로세서가 패널 근처 정지 개체에 부른다) 두 경우를 본다.
		const double FloorZ = 10.0 + CrawlTestRadius + Fixture.Params.SurfaceGap;
		for (int32 Case = 0; Case < 2; ++Case)
		{
			const bool bMoving = Case == 0;
			// 3·4번 바닥(x∈[-5000,-3000], y∈[-1000,1000]) 위, 원뿔·원기둥(y=0)에서 떨어진 자리.
			const FVector Start(-4000 + Case * 600, 600, FloorZ);
			UStaticMeshComponent* Slab = Fixture.Box(Start + FVector(0, 0, CrawlTestRadius + 30), FVector(3, 3, 0.2), TEXT("LNPDynamicTerrain"));
			Slab->SetMobility(EComponentMobility::Movable);
			Fixture.Publish();
			FCrawler Crawler;
			Crawler.Location = Start;
			float MaxFloorDepth = 0.f;
			int32 Frame = 0;
			for (; Frame < 60 && Crawler.Crushes == 0; ++Frame)
			{
				Slab->SetWorldLocation(Slab->GetComponentLocation() - FVector(0, 0, 5));
				if (bMoving)
				{
					Crawler.Tick(Fixture, FVector(CrawlTestSpeed * 0.2f, 0, 0));
				}
				else
				{
					FVector Next;
					Crawler.Crushes += ResolveStationaryOverlap(*Fixture.Collision, Fixture.Params, Crawler.Location, Crawler.Normal, Next)
						== EStepResult::Crushed ? 1 : 0;
					Crawler.Location = Next;
				}
				// 바닥(top z=10)에 파고든 깊이는 위치로 바로 잰다.
				MaxFloorDepth = FMath::Max(MaxFloorDepth, static_cast<float>(10.0 + CrawlTestRadius - Crawler.Location.Z));
			}
			AddInfo(FString::Printf(TEXT("Descending panel %s: frames=%d crushes=%d lost=%d landings=%d blocked=%d maxFloorDepth=%.3f final=%s"),
				bMoving ? TEXT("moving") : TEXT("stationary"), Frame, Crawler.Crushes, Crawler.Lost, Crawler.Landings, Crawler.Blocked,
				MaxFloorDepth, *Crawler.Location.ToString()));
			TestEqual(TEXT("Descending panel crushes the trapped crawler"), Crawler.Crushes, 1);
			TestTrue(TEXT("Descending panel never pushes the crawler into the floor"), MaxFloorDepth <= PenetrationTolerance);
			Slab->DestroyComponent();
			Fixture.Publish();
		}
	}
	return true;
}

bool FLNPSurfaceCrawlDynamicCarryTest::RunTest(const FString& Parameters)
{
	// 1. Step·Attach가 새로 붙은 면의 identity를 돌려준다. 정적 면은 동적 운반 접촉을 만들지 않는다.
	{
		FCrawlWorld Fixture(TEXT("LNPSurfaceCrawlDynamicCarryTest"));
		if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
			return false;
		Fixture.Box(FVector(0, 0, 0), FVector(40, 40, 0.2), TEXT("LNPStaticTerrain"));
		Fixture.Publish();
		const double FloorZ = 10.0 + CrawlTestRadius + Fixture.Params.SurfaceGap;

		FVector Next, NextNormal;
		FLNPExactHitIdentity Identity;
		Step(*Fixture.Collision, Fixture.Params, FVector(0, 0, FloorZ), FVector::UpVector, FVector(CrawlTestSpeed, 0, 0), CrawlTestDeltaTime,
			Next, NextNormal, &Identity);
		TestTrue(TEXT("Moving step reports the surface it settled on"), Identity.IsKnown());
		TestTrue(TEXT("Floor identity is static"), Identity.Lifetime == ELNPExactSourceLifetime::Static);

		FLNPExactHitIdentity Unchanged;
		Step(*Fixture.Collision, Fixture.Params, FVector(0, 0, FloorZ), FVector::UpVector, FVector::ZeroVector, CrawlTestDeltaTime,
			Next, NextNormal, &Unchanged);
		TestFalse(TEXT("Stationary step finds no new surface"), Unchanged.IsKnown());

		FLNPDynamicSupportFrame EmptyFrame;
		FLNPEnemyDynamicSupportContact Contact;
		TestFalse(TEXT("Static surface makes no dynamic contact"),
			LNPEnemySurfaceMovement::MakeDynamicSupportContact(Identity, EmptyFrame, Next, Contact));
	}

	// 2. 패널 운반: 패널 밑면에 붙은 개체(법선 -Z)가 패널 이동·회전을 따라가고 법선도 같이 돈다.
	const FGuid MarkerId(7, 7, 7, 7);
	FLNPExactHitIdentity PanelIdentity;
	PanelIdentity.Lifetime = ELNPExactSourceLifetime::Dynamic;
	PanelIdentity.Roles = ELNPExactSourceRole::Support | ELNPExactSourceRole::Blocker;
	PanelIdentity.Slot = 3;
	PanelIdentity.MarkerId = MarkerId;

	FLNPDynamicSupportFrame Frame;
	FLNPDynamicSupportSnapshot& Panel = Frame.Supports.AddDefaulted_GetRef();
	Panel.Id.Slot = 3;
	Panel.Id.MarkerId = MarkerId;
	Panel.CurrentTransform = FTransform(FQuat::Identity, FVector(0, 0, 500));
	Panel.PreviousTransform = Panel.CurrentTransform;
	Panel.LinearVelocity = FVector(100, 0, 0);
	Panel.bWalkable = true;

	const FVector UnderPanel(50, 0, 500 - 10 - CrawlTestRadius - 1);
	FLNPEnemyDynamicSupportContact Contact;
	TestTrue(TEXT("Panel underside makes a dynamic contact"),
		LNPEnemySurfaceMovement::MakeDynamicSupportContact(PanelIdentity, Frame, UnderPanel, Contact));

	// 다음 프레임: 패널이 +X 30 이동하고 X축(롤)으로 90도 돈다. 밑면 법선 -Z는 롤 90도 뒤 +Y가 된다.
	Panel.PreviousTransform = Panel.CurrentTransform;
	Panel.CurrentTransform = FTransform(FQuat(FVector::XAxisVector, HALF_PI), FVector(30, 0, 500));
	FTransform Crawler(FRotationMatrix::MakeFromZX(-FVector::UpVector, FVector::XAxisVector).ToQuat(), UnderPanel);
	FVector Normal = -FVector::UpVector;
	TestTrue(TEXT("Panel pose carries the crawler"), CarryOnDynamicSupport(Frame, Contact, Crawler, Normal));
	TestTrue(TEXT("Carried location follows the panel transform"),
		Crawler.GetLocation().Equals(Panel.CurrentTransform.TransformPositionNoScale(Contact.LocalCapsuleCenter), 0.01));
	TestTrue(TEXT("Carried surface normal rotates with the panel"), Normal.Equals(FVector::YAxisVector, 1e-3));
	TestTrue(TEXT("Carried body Up stays on the surface normal"), Crawler.GetRotation().GetUpVector().Equals(Normal, 1e-3));

	// 3. 패널이 사라지면 운반을 거부하고 마지막 패널 속도를 남긴다(호출자가 공중으로 넘긴다).
	Frame.Supports.Reset();
	TestFalse(TEXT("Missing panel stops the carry"), CarryOnDynamicSupport(Frame, Contact, Crawler, Normal));
	TestTrue(TEXT("Departure velocity is the last panel velocity"), Contact.LastLinearVelocity.Equals(FVector(100, 0, 0)));
	return true;
}

bool FLNPSurfaceCrawlWanderSegmentTest::RunTest(const FString& Parameters)
{
	FCrawlWorld Fixture(TEXT("LNPSurfaceCrawlWanderSegmentTest"));
	if (!TestNotNull(TEXT("World collision subsystem exists"), Fixture.Collision))
		return false;

	// 바닥 top z=10(x -2000~2000). 벽 x 290~310, 높이 600.
	Fixture.Box(FVector(0, 0, 0), FVector(40, 40, 0.2), TEXT("LNPStaticTerrain"));
	Fixture.Box(FVector(300, 0, 300), FVector(0.2, 40, 6), TEXT("LNPStaticTerrain"));
	Fixture.Publish();
	const double FloorZ = 10.0 + CrawlTestRadius + Fixture.Params.SurfaceGap;
	const double WallX = 290.0 - CrawlTestRadius - Fixture.Params.SurfaceGap;

	// 1. 평지 직선: 수락. query는 sweep 1 + 반지름 간격 probe(180 / 40 → 5)다.
	const uint64 Before = Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::SurfaceCrawl);
	TestTrue(TEXT("Flat segment is accepted"),
		ValidateWanderSegment(*Fixture.Collision, Fixture.Params, FVector(0, 0, FloorZ), FVector::UpVector, FVector(180, 0, FloorZ)));
	TestEqual(TEXT("Flat segment uses sweep plus probes"),
		Fixture.Collision->GetQueryCount(ELNPWorldQueryClass::SurfaceCrawl) - Before, static_cast<uint64>(6));

	// 2. 벽으로 들어가는 직선: 같은 높이 sweep이 벽에 막혀 거부(오목 모서리를 건너는 구간).
	TestFalse(TEXT("Segment into a wall is rejected"),
		ValidateWanderSegment(*Fixture.Collision, Fixture.Params, FVector(200, 0, FloorZ), FVector::UpVector, FVector(380, -100, FloorZ)));

	// 3. 절벽 밖으로 나가는 직선: 끝의 probe가 같은 면을 찾지 못해 거부.
	TestFalse(TEXT("Segment off a cliff edge is rejected"),
		ValidateWanderSegment(*Fixture.Collision, Fixture.Params, FVector(1900, -1000, FloorZ), FVector::UpVector, FVector(2080, -1000, FloorZ)));

	// 4. 벽면 위 직선: 법선 -X 평면 안이면 중력과 무관하게 수락.
	TestTrue(TEXT("Segment on a wall face is accepted"),
		ValidateWanderSegment(*Fixture.Collision, Fixture.Params, FVector(WallX, 0, 300), -FVector::ForwardVector, FVector(WallX, 150, 380)));

	// 5. 벽면에서 바닥으로 내려가는 직선: 볼록·오목 모서리를 건너므로 거부.
	TestFalse(TEXT("Segment from wall down to the floor is rejected"),
		ValidateWanderSegment(*Fixture.Collision, Fixture.Params, FVector(WallX, 0, 120), -FVector::ForwardVector, FVector(WallX, 0, 20)));
	return true;
}

#endif
