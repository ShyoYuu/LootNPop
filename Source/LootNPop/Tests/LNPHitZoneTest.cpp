// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HitDetection/LNPHitDetectionShared.h"
#include "HitDetection/LNPWeaponTraceMassTypes.h"
#include "GAS/LNPDamageFormula.h"
#include "Misc/DataValidation.h"

using namespace LNPHitDetection;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPHitZonePlacementTest,
	"LNP.HitDetection.HitZone.Placement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPHitZonePlacementTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Head overlaps and protrudes"), IsWeakSpotPlacementValid(96, 42, FVector(0, 0, 78), 40));
	TestTrue(TEXT("Negative Z also works"), IsWeakSpotPlacementValid(96, 42, FVector(0, 0, -78), 40));
	TestFalse(TEXT("Buried sphere"), IsWeakSpotPlacementValid(96, 42, FVector(0, 0, 54), 40));
	TestFalse(TEXT("Internal tangent"), IsWeakSpotPlacementValid(96, 42, FVector(0, 0, 56), 40));
	TestFalse(TEXT("Separated sphere"), IsWeakSpotPlacementValid(96, 42, FVector(0, 0, 140), 40));
	TestFalse(TEXT("External tangent"), IsWeakSpotPlacementValid(96, 42, FVector(0, 0, 136), 40));
	TestTrue(TEXT("Zero radius disables weak spot"), IsWeakSpotPlacementValid(96, 42, FVector(1000), 0));
	TestFalse(TEXT("Negative radius is invalid"), IsWeakSpotPlacementValid(96, 42, FVector(0, 0, 78), -1));
	TestTrue(TEXT("Drone spherical capsule"), IsWeakSpotPlacementValid(64, 64, FVector(39, 0, 0), 32));
	TestTrue(TEXT("Half height below radius also uses sphere"), IsWeakSpotPlacementValid(20, 64, FVector(39, 0, 0), 32));
	TestFalse(TEXT("Concentric contained sphere"), IsWeakSpotPlacementValid(64, 64, FVector::ZeroVector, 32));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPHitZoneClassifyTest,
	"LNP.HitDetection.HitZone.Classify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPHitZoneClassifyTest::RunTest(const FString& Parameters)
{
	const FVector Center = FVector::ZeroVector;
	const FVector WeakSpotCenter(0, 0, 78);
	for (const double Height : {78.0, 0.0, 96.0, 110.0})
	{
		const FPreparedSegment Segment(FVector(-100, 0, Height), FVector(100, 0, Height));
		FVector HitPoint(999);
		const bool bBodyHit = SegmentHitsCapsule(Segment, Center, FVector::UpVector, 96, 42, 0, HitPoint);
		TestEqual(TEXT("Only protruding sphere is a body miss"), bBodyHit, Height != 110.0);
		const FVector OriginalHitPoint = HitPoint;
		ELNPHitZone Zone = ELNPHitZone::Body;
		if (bBodyHit)
			Zone = ClassifyProjectileHitZone(Segment, WeakSpotCenter, FVector::UpVector, 40, 0);
		TestEqual(TEXT("Body gate determines final weak spot"), Zone,
			Height == 78.0 || Height == 96.0 ? ELNPHitZone::WeakSpot : ELNPHitZone::Body);
		TestEqual(TEXT("Classification leaves body hit point unchanged"), HitPoint, OriginalHitPoint);
		TestEqual(TEXT("Disabled weak spot stays body"),
			ClassifyProjectileHitZone(Segment, WeakSpotCenter, FVector::UpVector, 0, 0), ELNPHitZone::Body);
		if (!bBodyHit)
			TestEqual(TEXT("Sphere contact alone cannot authorize a hit"),
				ClassifyProjectileHitZone(Segment, WeakSpotCenter, FVector::UpVector, 40, 0), ELNPHitZone::WeakSpot);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPHitZoneFirstContactTest,
	"LNP.HitDetection.HitZone.FirstContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPHitZoneFirstContactTest::RunTest(const FString& Parameters)
{
	struct FShape { float HalfHeight; float Radius; FVector Offset; float WeakRadius; };
	const FShape Shapes[] = { {96, 42, FVector(0, 0, 78), 40}, {88, 35, FVector(0, 0, 70), 35} };
	const FVector Directions[] = { FVector::ForwardVector, FVector::BackwardVector,
		FVector::RightVector, FVector::LeftVector, FVector::UpVector };
	for (const FShape& Shape : Shapes)
	for (const FVector& Direction : Directions)
	for (const float HitRadius : {0.f, 2.f, 10.f})
	for (const double Step : {0.05, 0.5, 5.0})
	for (const double Phase : {0.1, 0.5, 0.9})
	{
		bool bFound = false;
		for (double Distance = 150.0 + Step * Phase; Distance > 0.0; Distance -= Step)
		{
			const FPreparedSegment Segment(Shape.Offset + Direction * Distance,
				Shape.Offset + Direction * (Distance - Step));
			FVector HitPoint;
			if (!SegmentHitsCapsule(Segment, FVector::ZeroVector, FVector::UpVector,
				Shape.HalfHeight, Shape.Radius, HitRadius, HitPoint))
				continue;
			TestEqual(TEXT("First body contact is already covered by head sphere"),
				ClassifyProjectileHitZone(Segment, Shape.Offset, FVector::UpVector, Shape.WeakRadius, HitRadius),
				ELNPHitZone::WeakSpot);
			bFound = true;
			break;
		}
		TestTrue(TEXT("Sampled path reaches body"), bFound);
	}
	// 드론의 센서는 정면에서 몸 구에 처음 닿는 순간을 덮는다.
	const FPreparedSegment DroneSegment(FVector(64.01, 0, 0), FVector(63.99, 0, 0));
	FVector HitPoint;
	TestTrue(TEXT("Drone front hits body"), SegmentHitsCapsule(DroneSegment, FVector::ZeroVector,
		FVector::UpVector, 64, 64, 0, HitPoint));
	TestEqual(TEXT("Drone front first contact is weak spot"),
		ClassifyProjectileHitZone(DroneSegment, FVector(39, 0, 0), FVector::UpVector, 32, 0), ELNPHitZone::WeakSpot);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPHitZoneMeleeDegenerateTest,
	"LNP.HitDetection.HitZone.MeleeDegenerate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPHitZoneMeleeDegenerateTest::RunTest(const FString& Parameters)
{
	// 서버가 호출하는 실제 함수로 폴백 선택·구 축 거리·몸 명중 게이트를 검증한다.
	for (const double Movement : {0.0, 0.5})
	for (const bool bReverse : {false, true})
	for (const double Height : {78.0, 0.0, 110.0})
	{
		FLNPWeaponTraceFragment Trace;
		Trace.SwordRootCurr = FVector(bReverse ? 100 : -100, 0, Height);
		Trace.SwordTipCurr = FVector(bReverse ? -100 : 100, 0, Height);
		Trace.SwordRootPrev = Trace.SwordRootCurr - FVector(0, Movement, 0);
		Trace.SwordTipPrev = Trace.SwordTipCurr - FVector(0, Movement, 0);
		Trace.HitRadius = 5.f;
		const float BodyDistSq = MeleeCapsuleDistSq(Trace, FVector::ZeroVector, FVector::UpVector, 96, 42);
		const float WeakDistSq = MeleeCapsuleDistSq(Trace, FVector(0, 0, 78), FVector::UpVector, 40, 40);
		TestTrue(TEXT("Sphere axis distance uses blade interior in either endpoint order"),
			FMath::IsNearlyEqual(WeakDistSq, float(FMath::Square(Height - 78.0))));
		const bool bBodyHit = BodyDistSq <= FMath::Square(Trace.HitRadius + 42.f);
		TestEqual(TEXT("Only protruding sphere is a body miss"), bBodyHit, Height != 110.0);
		const bool bSphereHit = WeakDistSq <= FMath::Square(Trace.HitRadius + 40.f);
		TestEqual(TEXT("Head is weak spot and chest stays body after body gate"),
			bBodyHit && bSphereHit, Height == 78.0);
		if (!bBodyHit)
			TestTrue(TEXT("Sphere contact alone still cannot authorize melee damage"), bSphereHit);
	}

	// 폴백 밖에서도 같은 함수가 칼날 사이를 채운 면으로 구를 검사한다.
	FLNPWeaponTraceFragment Sweep;
	Sweep.SwordRootPrev = FVector(-100, -60, 78);
	Sweep.SwordTipPrev = FVector(100, -60, 78);
	Sweep.SwordRootCurr = FVector(-100, 60, 78);
	Sweep.SwordTipCurr = FVector(100, 60, 78);
	TestTrue(TEXT("Moving swept blade reaches sphere between frames"), FMath::IsNearlyZero(
		MeleeCapsuleDistSq(Sweep, FVector(0, 0, 78), FVector::UpVector, 40, 40)));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPHitZoneOffsetRotationTest,
	"LNP.HitDetection.HitZone.OffsetRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPHitZoneOffsetRotationTest::RunTest(const FString& Parameters)
{
	const FQuat Yaw(FVector::UpVector, UE_DOUBLE_HALF_PI);
	TestTrue(TEXT("Drone sensor follows yaw"), ResolveWeakSpotOffset(Yaw, FVector(39, 0, 0)).Equals(FVector(0, 39, 0), 1.e-6));
	const FQuat Rotation = FRotationMatrix::MakeFromXZ(FVector::RightVector, FVector::ForwardVector).ToQuat();
	TestTrue(TEXT("Sensor follows forward with arbitrary up"),
		ResolveWeakSpotOffset(Rotation, FVector(39, 0, 0)).Equals(FVector(0, 39, 0), 1.e-6));
	TestTrue(TEXT("Head follows arbitrary up"),
		ResolveWeakSpotOffset(Rotation, FVector(0, 0, 78)).Equals(FVector(78, 0, 0), 1.e-6));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPHitZoneDamageScaleTest,
	"LNP.HitDetection.HitZone.DamageScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPHitZoneDamageScaleTest::RunTest(const FString& Parameters)
{
	ULNPSettings* Settings = GetMutableDefault<ULNPSettings>();
	TGuardValue<float> RestoreMultiplier(Settings->WeakSpotDamageMultiplier, 1.5f);
	TestEqual(TEXT("Body damage unchanged"), ScaleDamageForZone(80, ELNPHitZone::Body), 80.f);
	TestEqual(TEXT("Weak spot damage scaled"), ScaleDamageForZone(80, ELNPHitZone::WeakSpot), 120.f);
	for (const float Defense : {0.f, 50.f, 300.f})
		TestTrue(TEXT("Defense preserves damage ratio"), FMath::IsNearlyEqual(
			LNPDamage::ApplyDefense(ScaleDamageForZone(80, ELNPHitZone::WeakSpot), Defense),
			LNPDamage::ApplyDefense(80, Defense) * 1.5f));
	Settings->WeakSpotDamageMultiplier = 1.f;
	TestEqual(TEXT("Unit multiplier supported"), ScaleDamageForZone(80, ELNPHitZone::WeakSpot), 80.f);
	Settings->WeakSpotDamageMultiplier = 2.f;
	TestEqual(TEXT("Uses global setting"), ScaleDamageForZone(80, ELNPHitZone::WeakSpot), 160.f);
	return !HasAnyErrors();
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPHitZoneConfigAssetsTest,
	"LNP.HitDetection.HitZone.ConfigAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPHitZoneConfigAssetsTest::RunTest(const FString& Parameters)
{
	const TCHAR* EnemyPaths[] = {
		TEXT("/Game/Enemy/DA_Enemy_PureEntity_Melee01.DA_Enemy_PureEntity_Melee01"),
		TEXT("/Game/Enemy/DA_Enemy_PureEntity_Ranged01.DA_Enemy_PureEntity_Ranged01"),
		TEXT("/Game/Enemy/DA_Enemy_ActorPromoted_Melee01.DA_Enemy_ActorPromoted_Melee01"),
		TEXT("/Game/Enemy/DA_Enemy_PureEntity_Flyer01.DA_Enemy_PureEntity_Flyer01"),
		TEXT("/Game/Enemy/DA_Enemy_PureEntity_Crawler01.DA_Enemy_PureEntity_Crawler01")
	};
	for (const TCHAR* Path : EnemyPaths)
	{
		const ULNPEnemyConfig* Config = LoadObject<ULNPEnemyConfig>(nullptr, Path);
		if (!TestNotNull(Path, Config))
			continue;
		FDataValidationContext Context;
		TestEqual(Path, Config->IsDataValid(Context), EDataValidationResult::Valid);
		TestEqual(TEXT("Enemy placement errors"), Context.GetNumErrors(), 0u);
	}
	const UClass* PlayerClass = LoadClass<ALNPPlayerCharacter>(nullptr, TEXT("/Game/Pawns/BP_LNPPlayer.BP_LNPPlayer_C"));
	if (TestNotNull(TEXT("Player blueprint class"), PlayerClass))
	{
		FDataValidationContext Context;
		TestEqual(TEXT("Player CDO placement"), PlayerClass->GetDefaultObject<ALNPPlayerCharacter>()->IsDataValid(Context),
			EDataValidationResult::Valid);
		TestEqual(TEXT("Player placement errors"), Context.GetNumErrors(), 0u);
	}
	return !HasAnyErrors();
}
#endif

#endif
