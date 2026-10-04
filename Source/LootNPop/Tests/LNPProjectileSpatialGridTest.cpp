// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HitDetection/LNPHitDetectionShared.h"
#include "Math/RandomStream.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPSpatialGridCoverageTest,
	"LNP.HitDetection.SpatialGrid.SphereCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSpatialGridCoverageTest::RunTest(const FString& Parameters)
{
	FRandomStream Random(12305);
	TArray<FVector> Positions = { FVector::ZeroVector, FVector(30000, 0, 0), FVector(0, 0, 30000),
		FVector(0, 0, -30000), FVector(30000, -0.001, 0), FVector(30000, 0.001, 0) };
	// 셀 경계 양쪽·극 주변·고도 차를 같은 인덱스에 넣는다.
	for (double Latitude : { -89.99, -88.59375, 0.0, 88.59375, 89.99 })
		for (double Longitude : { -0.00001, 0.00001, 1.40624, 1.40626, 179.99999, 180.00001 })
		{
			const double Lat = FMath::DegreesToRadians(Latitude), Lon = FMath::DegreesToRadians(Longitude);
			Positions.Add(FVector(FMath::Cos(Lat) * FMath::Cos(Lon), FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat)) * 30000);
		}
	for (int32 Index = 0; Index < 1000; ++Index)
		Positions.Add(Random.VRand() * Random.FRandRange(10000.f, 40000.f));
	FLNPEnemySpatialGrid Grid;
	Grid.BeginRebuild(Positions.Num());
	for (const FVector& Position : Positions) Grid.AddEntity(FMassEntityHandle(), Position);
	Grid.FinishRebuild();
	TArray<int32> Candidates;
	int32 LocalQueries = 0;
	for (int32 Query = 0; Query < 1000; ++Query)
	{
		const FVector Center = Positions[1 + Query % (Positions.Num() - 1)];
		const double Radius = Query < 36 ? 50.0 : Random.FRandRange(1.f, 3000.f);
		if (!Grid.GatherSphereCandidates(Center, Radius, Candidates)) continue;
		++LocalQueries;
		TestTrue(TEXT("Unindexed origin is always included"), Candidates.Contains(0));
		for (int32 Index = 1; Index < Candidates.Num(); ++Index)
			if (!TestTrue(TEXT("Candidates are unique and preserve insertion order"), Candidates[Index - 1] < Candidates[Index])) return false;
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
			if (FVector::DistSquared(Center, Positions[Index]) <= FMath::Square(Radius)
				&& !TestTrue(TEXT("Every point in query sphere is included"), Candidates.Contains(Index))) return false;
	}
	TestTrue(TEXT("Coverage exercised local grid queries"), LocalQueries > 900);
	TestFalse(TEXT("Origin query uses full scan"), Grid.GatherSphereCandidates(FVector::ZeroVector, 100, Candidates));
	TestFalse(TEXT("Sphere containing origin uses full scan"), Grid.GatherSphereCandidates(FVector(100, 0, 0), 101, Candidates));
	TestFalse(TEXT("Wide query uses full scan"), Grid.GatherSphereCandidates(FVector(30000, 0, 0), 29000, Candidates));
	// 지연 재구축이 이전 프레임의 후보를 남기지 않으며 기존 이웃 조회는 원점을 방문하지 않는다.
	Grid.BeginRebuild(1);
	Grid.AddEntity(FMassEntityHandle(), FVector::ZeroVector);
	Grid.FinishRebuild();
	int32 LegacyNeighbors = 0;
	Grid.ForEachNeighbor(FVector(30000, 0, 0), 1000, [&](FMassEntityHandle, const FVector&) { ++LegacyNeighbors; });
	TestEqual(TEXT("Legacy separation still ignores origin entries"), LegacyNeighbors, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPProjectileGridCapsuleTest,
	"LNP.HitDetection.SpatialGrid.CapsuleCoverageAndOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPProjectileGridCapsuleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Local zero rewind can use grid"), LNPHitDetection::CanUseCurrentCapsuleGrid(false, 0));
	TestFalse(TEXT("Remote zero ping can still interpolate"), LNPHitDetection::CanUseCurrentCapsuleGrid(true, 0));
	TestFalse(TEXT("Local ping rewind uses full scan"), LNPHitDetection::CanUseCurrentCapsuleGrid(false, 0.01f));
	TestFalse(TEXT("Remote reflected projectile uses full scan"), LNPHitDetection::CanUseCurrentCapsuleGrid(true, 0.1f));
	struct FCapsule { FVector Center; FVector Up; float HalfHeight; float Radius; };
	FRandomStream Random(12306);
	TArray<FCapsule> Capsules = {
		{ FVector(30000, 120, 0), FVector::UpVector, 96, 42 },
		{ FVector::ZeroVector, FVector::UpVector, 100, 42 },
		{ FVector(30000, 0, 0), FVector::UpVector, 96, 42 } };
	for (int32 Index = 0; Index < 1000; ++Index)
		Capsules.Add({ Random.VRand() * Random.FRandRange(10000.f, 40000.f), Random.VRand(),
			float(Random.FRandRange(10.f, 180.f)), float(Random.FRandRange(5.f, 100.f)) });
	FLNPEnemySpatialGrid Grid;
	Grid.BeginRebuild(Capsules.Num());
	double MaxExtent = 0;
	for (const FCapsule& Capsule : Capsules)
	{
		Grid.AddEntity(FMassEntityHandle(), Capsule.Center);
		MaxExtent = FMath::Max(MaxExtent, double(FMath::Max(Capsule.HalfHeight, Capsule.Radius)));
	}
	Grid.FinishRebuild();
	TArray<int32> Candidates;
	for (int32 Query = 0; Query < 300; ++Query)
	{
		const FVector Center = Query == 0 ? FVector(30000, 0, 0) : Capsules[Query].Center;
		const FVector Delta = Query == 0 ? FVector(0, 200, 0) : Random.VRand() * Random.FRandRange(0.f, 1000.f);
		const LNPHitDetection::FPreparedSegment Segment(Center - Delta, Center + Delta);
		const float HitRadius = Query % 2 ? 10.f : 5.f;
		const bool bLocal = LNPHitDetection::GatherProjectileCapsuleCandidates(Grid, Segment, MaxExtent, HitRadius, Candidates);
		if (!bLocal) { Candidates.Reset(); for (int32 Index = 0; Index < Capsules.Num(); ++Index) Candidates.Add(Index); }
		int32 FirstReference = INDEX_NONE, FirstGrid = INDEX_NONE;
		FVector ReferencePoint = FVector::ZeroVector, GridPoint = FVector::ZeroVector;
		for (int32 Index = 0; Index < Capsules.Num(); ++Index)
		{
			const FCapsule& Capsule = Capsules[Index];
			FVector Point;
			if (LNPHitDetection::SegmentHitsCapsule(Segment, Capsule.Center, Capsule.Up, Capsule.HalfHeight, Capsule.Radius, HitRadius, Point))
			{
				if (!TestTrue(TEXT("Exact capsule hit is never omitted"), Candidates.Contains(Index))) return false;
				if (FirstReference == INDEX_NONE) { FirstReference = Index; ReferencePoint = Point; }
			}
		}
		for (int32 Index : Candidates)
		{
			const FCapsule& Capsule = Capsules[Index];
			if (LNPHitDetection::SegmentHitsCapsule(Segment, Capsule.Center, Capsule.Up, Capsule.HalfHeight, Capsule.Radius, HitRadius, GridPoint))
			{ FirstGrid = Index; break; }
		}
		if (!TestEqual(TEXT("First hit retains collection order"), FirstGrid, FirstReference)) return false;
		if (FirstGrid != INDEX_NONE && !TestEqual(TEXT("First hit point is unchanged"), GridPoint, ReferencePoint)) return false;
		if (Query == 0) {
			TestTrue(TEXT("Order fixture uses local grid"), bLocal);
			TestEqual(TEXT("Earlier collected target wins over geometrically nearer target"), FirstGrid, 0);
		}
	}
	const LNPHitDetection::FPreparedSegment LongSegment(FVector(30000, 0, 0), FVector(-30000, 0, 0));
	TestFalse(TEXT("Long chord keeps full scan"), LNPHitDetection::GatherProjectileCapsuleCandidates(Grid, LongSegment, MaxExtent, 5, Candidates));
	const LNPHitDetection::FPreparedSegment PointSegment(FVector(30000, 0, 0), FVector(30000, 0, 0));
	TestTrue(TEXT("Point segment keeps target"), LNPHitDetection::GatherProjectileCapsuleCandidates(Grid, PointSegment, MaxExtent, 5, Candidates) && Candidates.Contains(2));
	return true;
}

#endif
