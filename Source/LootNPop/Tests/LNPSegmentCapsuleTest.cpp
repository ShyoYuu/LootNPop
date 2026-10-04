// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HitDetection/LNPHitDetectionShared.h"
#include "Math/RandomStream.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPSegmentCapsuleRegressionTest,
	"LNP.HitDetection.SegmentCapsule.Regression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSegmentCapsuleRegressionTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		const TCHAR* Name;
		FVector A;
		FVector B;
		float HalfHeight;
		float Radius;
		float HitRadius;
		bool bHit;
	};
	const FCase Cases[] = {
		{ TEXT("Oblique segment crosses axis away from center"), {-60, 0, -10}, {60, 0, 110}, 100, 10, 0, true },
		{ TEXT("Flat cylinder corner is outside rounded cap"), {-20, 9, 99}, {20, 9, 99}, 100, 10, 0, false },
		{ TEXT("Upper cap tangent"), {-20, 0, 100}, {20, 0, 100}, 100, 10, 0, true },
		{ TEXT("Lower cap tangent"), {-20, 0, -100}, {20, 0, -100}, 100, 10, 0, true },
		{ TEXT("Inflation extends upper cap"), {-20, 0, 105}, {20, 0, 105}, 100, 10, 10, true },
		{ TEXT("Inflation extends lower cap"), {-20, 0, -105}, {20, 0, -105}, 100, 10, 10, true },
		{ TEXT("Inflation keeps original axis"), {-20, 15, 95}, {20, 15, 95}, 100, 10, 10, true },
		{ TEXT("Beyond inflated cap"), {-20, 0, 111}, {20, 0, 111}, 100, 10, 10, false },
		{ TEXT("Parallel segment inside"), {9, 0, -200}, {9, 0, 200}, 100, 10, 0, true },
		{ TEXT("Parallel segment outside"), {11, 0, -200}, {11, 0, 200}, 100, 10, 0, false },
		{ TEXT("Collinear disjoint segment"), {0, 0, 101}, {0, 0, 200}, 100, 10, 0, false },
		{ TEXT("Point inside cap"), {0, 0, 99}, {0, 0, 99}, 100, 10, 0, true },
		{ TEXT("Point outside cap"), {9, 0, 99}, {9, 0, 99}, 100, 10, 0, false },
		{ TEXT("Sphere capsule tangent"), {-20, 10, 0}, {20, 10, 0}, 10, 10, 0, true },
		{ TEXT("Sphere capsule point outside"), {0, 0, 11}, {0, 0, 11}, 10, 10, 0, false },
		{ TEXT("Endpoint inside capsule"), {0, 0, 0}, {100, 0, 0}, 100, 10, 0, true },
		{ TEXT("Near parallel segment crosses axis"), {-1, 0, -1000000}, {1, 0, 1000000}, 100, 0.001f, 0, true },
	};
	// 구형 월드처럼 회전·이동한 캡슐도 같은 판정을 유지한다.
	const FQuat Rotations[] = { FQuat::Identity, FQuat(FVector(1, 2, 3).GetSafeNormal(), 1.1) };
	for (const FQuat& Rotation : Rotations)
	{
		const FVector Center(12000, -23000, 7000);
		for (const FCase& Case : Cases)
		{
			// 회전 계산의 반올림이 정확한 접선의 양쪽으로 튈 수 있으므로 접선은 정수 좌표에서 검사한다.
			if (!Rotation.Equals(FQuat::Identity) && FString(Case.Name).Contains(TEXT("tangent")))
				continue;
			const FVector A = Center + Rotation.RotateVector(Case.A);
			const FVector B = Center + Rotation.RotateVector(Case.B);
			FVector HitPoint(999);
			const bool bHit = LNPHitDetection::SegmentHitsCapsule(A, B, Center,
				Rotation.RotateVector(FVector::UpVector), Case.HalfHeight, Case.Radius, Case.HitRadius, HitPoint);
			TestEqual(Case.Name, bHit, Case.bHit);
			const LNPHitDetection::FPreparedSegment Segment(A, B);
			FVector PreparedPoint(999);
			TestEqual(TEXT("Prepared segment preserves hit"), LNPHitDetection::SegmentHitsCapsule(Segment,
				Center, Rotation.RotateVector(FVector::UpVector), Case.HalfHeight, Case.Radius, Case.HitRadius, PreparedPoint), bHit);
			TestEqual(TEXT("Prepared segment preserves output"), PreparedPoint, HitPoint);
			if (bHit)
				TestTrue(TEXT("Sphere filter keeps exact hit"), LNPHitDetection::SegmentMayHitCapsule(Segment,
					Center, Case.HalfHeight, Case.Radius, Case.HitRadius));
			if (bHit)
				TestTrue(TEXT("Hit point lies on projectile segment"),
					HitPoint.Equals(FMath::ClosestPointOnSegment(HitPoint, A, B), 1.e-6));
			else
				TestEqual(TEXT("Miss leaves output unchanged"), HitPoint, FVector(999));
			FVector ReversePoint;
			TestEqual(TEXT("Reversing segment preserves hit"), LNPHitDetection::SegmentHitsCapsule(B, A,
				Center, Rotation.RotateVector(FVector::UpVector), Case.HalfHeight, Case.Radius, Case.HitRadius, ReversePoint), bHit);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPSegmentCapsuleReferenceTest,
	"LNP.HitDetection.SegmentCapsule.EngineReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSegmentCapsuleReferenceTest::RunTest(const FString& Parameters)
{
	FRandomStream Random(12004);
	for (int32 Index = 0; Index < 10000; ++Index)
	{
		const FVector Center = Random.VRand() * 30000.0;
		const FVector Up = Random.VRand();
		const float Radius = Random.FRandRange(10.f, 80.f);
		const float HalfHeight = Radius + Random.FRandRange(0.f, 150.f);
		const float HitRadius = Random.FRandRange(0.f, 50.f);
		const FVector A = Center + Random.VRand() * Random.FRandRange(0.f, 400.f);
		const FVector B = Center + Random.VRand() * Random.FRandRange(0.f, 400.f);
		FVector ReferencePoint, AxisPoint;
		FMath::SegmentDistToSegmentSafe(A, B, Center - Up * (HalfHeight - Radius),
			Center + Up * (HalfHeight - Radius), ReferencePoint, AxisPoint);
		const double ReferenceDistanceSq = FVector::DistSquared(ReferencePoint, AxisPoint);
		const double RadiusSq = FMath::Square(double(Radius) + HitRadius);
		FVector HitPoint;
		const bool bHit = LNPHitDetection::SegmentHitsCapsule(A, B, Center, Up, HalfHeight, Radius, HitRadius, HitPoint);
		if (bHit && !TestTrue(FString::Printf(TEXT("Sphere keeps reference hit %d"), Index),
			LNPHitDetection::SegmentMayHitCapsule(LNPHitDetection::FPreparedSegment(A, B), Center, HalfHeight, Radius, HitRadius)))
			return false;
		// 엔진 기준 함수는 거의 평행한 선분을 근사하므로 그 경우는 위의 명시적 사례로 검증한다.
		if (FVector::CrossProduct((B - A).GetSafeNormal(), Up).SizeSquared() < UE_KINDA_SMALL_NUMBER)
			continue;
		if (!TestEqual(FString::Printf(TEXT("Reference hit %d"), Index), bHit, ReferenceDistanceSq <= RadiusSq))
			return false;
		if (bHit && !TestTrue(FString::Printf(TEXT("Reference closest point %d"), Index),
			HitPoint.Equals(ReferencePoint, 1.e-5)))
			return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPSegmentCapsuleSphereTest,
	"LNP.HitDetection.SegmentCapsule.SphereBroadPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPSegmentCapsuleSphereTest::RunTest(const FString& Parameters)
{
	// 멀리 이동한 타겟은 현재 위치가 아니라 정밀 판정에 쓰는 되감긴 중심으로 거른다.
	const LNPHitDetection::FPreparedSegment Segment(FVector(-200, 0, 0), FVector(200, 0, 0));
	TestFalse(TEXT("Reject distant current center"), LNPHitDetection::SegmentMayHitCapsule(Segment, FVector(0, 1000, 0), 96, 42, 10));
	TestTrue(TEXT("Keep historical center"), LNPHitDetection::SegmentMayHitCapsule(Segment, FVector::ZeroVector, 96, 42, 10));
	FRandomStream Random(12304);
	for (int32 Index = 0; Index < 10000; ++Index)
	{
		const FVector Center = Random.VRand() * 30000;
		const FVector Up = Random.VRand();
		const float Radius = Random.FRandRange(0.01f, 100.f);
		const float HalfHeight = Random.FRandRange(0.f, 300.f);
		const float HitRadius = Random.FRandRange(0.f, 200.f);
		// 점 선분, 반지름보다 짧은 캡슐, 긴 축 방향 선분과 큰 패링 반경도 포함한다.
		const FVector A = Center + Random.VRand() * Random.FRandRange(0.f, 600.f);
		const FVector B = Index % 3 == 0 ? A : (Index % 3 == 1 ? A + Up * 20000 : Center + Random.VRand() * 600);
		const LNPHitDetection::FPreparedSegment Query(A, B);
		FVector ExactPoint(999), FilteredPoint(999);
		const bool bExact = LNPHitDetection::SegmentHitsCapsule(A, B, Center, Up, HalfHeight, Radius, HitRadius, ExactPoint);
		const bool bFiltered = LNPHitDetection::SegmentMayHitCapsule(Query, Center, HalfHeight, Radius, HitRadius)
			&& LNPHitDetection::SegmentHitsCapsule(Query, Center, Up, HalfHeight, Radius, HitRadius, FilteredPoint);
		if (!TestEqual(TEXT("Filtered result equals exact result"), bFiltered, bExact)
			|| !TestEqual(TEXT("Filtered output equals exact output"), FilteredPoint, ExactPoint))
			return false;
	}
	return true;
}

#endif
