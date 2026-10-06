// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Replication/LNPMassReplicator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceCrawlNormalCodecTest,
	"LootNPop.SurfaceNavigation.SurfaceCrawl.Replication.NormalCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPSurfaceCrawlPoseDirtyTest,
	"LootNPop.SurfaceNavigation.SurfaceCrawl.Replication.PoseDirty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	/** octahedral 8+8bit의 최악 오차 상한(도). 실측 최댓값보다 약간 크게 잡는다. */
	constexpr double MaxNormalErrorDegrees = 1.5;

	double AngleDegrees(const FVector& A, const FVector& B)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(A.GetSafeNormal() | B.GetSafeNormal(), -1.0, 1.0)));
	}

	/** 벽보행 자세: 회전 Z = 표면 법선, 전방은 접평면 위 임의 방향. */
	FTransform CrawlPose(const FVector& Location, const FVector& Normal, const FVector& ForwardHint)
	{
		return FTransform(FRotationMatrix::MakeFromZX(Normal, ForwardHint).ToQuat(), Location);
	}

	FLNPReplicatedAgent Roundtrip(const FTransform& Pose, const bool bCrawl)
	{
		FLNPMassFastArrayItem Item;
		FLNPEnemyFragment Enemy;
		LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, nullptr, {}, 10.0, 0.f, true, bCrawl);
		return Item.Agent;
	}
}

bool FLNPSurfaceCrawlNormalCodecTest::RunTest(const FString& Parameters)
{
	using namespace LNP::Replication;

	TestTrue(TEXT("Zero code decodes to no normal"), DecodeOctNormal(0).IsZero());
	TestEqual(TEXT("Zero vector encodes to no normal"), EncodeOctNormal(FVector::ZeroVector), uint16(0));

	// 축 6방향과 octahedral 접힘 경계(Z 부호 전환)를 포함한 구면 격자.
	double MaxError = 0.0;
	int32 ZeroCodes = 0;
	for (int32 Lat = 0; Lat <= 36; ++Lat)
	{
		for (int32 Lon = 0; Lon < 72; ++Lon)
		{
			const double Theta = UE_DOUBLE_PI * Lat / 36.0;
			const double Phi = 2.0 * UE_DOUBLE_PI * Lon / 72.0;
			const FVector N(FMath::Sin(Theta) * FMath::Cos(Phi), FMath::Sin(Theta) * FMath::Sin(Phi), FMath::Cos(Theta));
			const uint16 Code = EncodeOctNormal(N);
			ZeroCodes += Code == 0;
			MaxError = FMath::Max(MaxError, AngleDegrees(N, DecodeOctNormal(Code)));
		}
	}
	AddInfo(FString::Printf(TEXT("Octahedral normal max error %.3f deg"), MaxError));
	TestEqual(TEXT("Valid normals never use the reserved zero code"), ZeroCodes, 0);
	TestTrue(TEXT("Normal error stays under the bound"), MaxError < MaxNormalErrorDegrees);

	// 벽·천장·바닥·극점 법선의 자세 복원 — Up과 전방 모두 법선 해상도 안에서 일치해야 한다.
	const FVector Location(12000.0, -8000.0, 15000.0);
	const TPair<FVector, FVector> Poses[] = {
		{ FVector(0.6, -0.8, 0.0), FVector(0.0, 0.0, 1.0) },         // 벽
		{ -Location.GetSafeNormal(), FVector(1.0, 0.0, 0.0) },         // 지면(구 중심 방향)
		{ Location.GetSafeNormal(), FVector(0.0, 1.0, 0.0) },          // 천장(섬 밑면)
		{ FVector(0.0, 0.0, 1.0), FVector(0.3, 0.7, 0.0) },            // MakeFromZ 극점 전환
		{ FVector(0.0, 0.0, -1.0), FVector(-0.5, 0.2, 0.0) },
		{ FVector(0.01, 0.0, 0.99995).GetSafeNormal(), FVector(0.0, 1.0, 0.0) },
	};
	for (const auto& [Normal, Forward] : Poses)
	{
		const FTransform Pose = CrawlPose(Location, Normal, Forward);
		const FLNPReplicatedAgent Agent = Roundtrip(Pose, true);
		const FQuat Decoded = Agent.DecodeRotation();
		TestTrue(TEXT("Crawl agent carries a normal"), Agent.GetSurfaceNormal() != 0);
		const double UpError = AngleDegrees(Decoded.GetAxisZ(), Pose.GetRotation().GetAxisZ());
		const double ForwardError = AngleDegrees(Decoded.GetAxisX(), Pose.GetRotation().GetAxisX());
		TestTrue(FString::Printf(TEXT("Up restored (%.3f deg)"), UpError), UpError < MaxNormalErrorDegrees);
		TestTrue(FString::Printf(TEXT("Forward restored (%.3f deg)"), ForwardError), ForwardError < MaxNormalErrorDegrees);
	}

	// 지상·비행 적은 법선을 싣지 않고 기존 구 접평면 디코드와 같은 자세를 복원한다.
	const FTransform GroundPose = CrawlPose(Location, -Location.GetSafeNormal(), FVector(1.0, 0.0, 0.0));
	const FLNPReplicatedAgent Ground = Roundtrip(GroundPose, false);
	TestEqual(TEXT("Ground agent keeps the reserved zero normal"), Ground.GetSurfaceNormal(), uint16(0));
	TestTrue(TEXT("Ground decode equals legacy sphere decode"), Ground.DecodeRotation().Equals(
		DecodeSphereRotation(Ground.GetReplicatedPositionYawData().GetPosition(), Ground.GetReplicatedPositionYawData().GetYaw()), 1.e-6));
	return !HasAnyErrors();
}

bool FLNPSurfaceCrawlPoseDirtyTest::RunTest(const FString& Parameters)
{
	const FVector Location(12000.0, -8000.0, 15000.0);
	const FVector Wall(0.6, -0.8, 0.0);
	FLNPEnemyFragment Enemy;
	FLNPEnemyActionFragment Action;

	// 지상: 법선 필드가 Dirty를 만들지 않는다 — 정지한 개체는 주기가 지나도 갱신이 없다.
	{
		FLNPMassFastArrayItem Item;
		const FTransform Pose = CrawlPose(Location, -Location.GetSafeNormal(), FVector(1.0, 0.0, 0.0));
		TestTrue(TEXT("Ground first update"), LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, {}, 10.0, 0.3f));
		TestFalse(TEXT("Ground still pose has no dirty"), LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, {}, 11.0, 0.3f));
		TestEqual(TEXT("Ground normal stays zero"), Item.Agent.GetSurfaceNormal(), uint16(0));
	}

	FLNPMassFastArrayItem Item;
	FTransform Pose = CrawlPose(Location, Wall, FVector(0.0, 0.0, 1.0));
	TestTrue(TEXT("Crawl first update"), LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, {}, 10.0, 0.3f, true, true));
	const uint16 WallCode = Item.Agent.GetSurfaceNormal();
	TestTrue(TEXT("Crawl still pose has no dirty"), !LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, {}, 11.0, 0.3f, true, true));

	// 같은 벽에서 전방만 돌면 법선 코드는 그대로다(평소 델타 압축 1비트).
	Pose.SetRotation(FQuat(Wall, FMath::DegreesToRadians(30.0)) * Pose.GetRotation());
	TestTrue(TEXT("Turning on the same wall dirties yaw"), LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, {}, 12.0, 0.3f, true, true));
	TestEqual(TEXT("Turning keeps the normal code"), Item.Agent.GetSurfaceNormal(), WallCode);

	// 표면을 갈아타면 위치가 그대로여도 법선 변경만으로 갱신된다. 주기 게이트는 지킨다.
	const FVector Ceiling = Location.GetSafeNormal();
	Pose = CrawlPose(Location, Ceiling, FVector(0.0, 0.0, 1.0));
	TestFalse(TEXT("Normal change respects the period gate"), LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, {}, 12.1, 0.3f, true, true));
	TestTrue(TEXT("Normal change dirties after the gate"), LNP::Replication::UpdateEnemyAgent(Item, Pose, Enemy, &Action, {}, 12.4, 0.3f, true, true));
	TestTrue(TEXT("Normal code switched"), Item.Agent.GetSurfaceNormal() != WallCode);
	TestTrue(TEXT("Decoded Up follows the new surface"), AngleDegrees(Item.Agent.DecodeRotation().GetAxisZ(), Ceiling) < MaxNormalErrorDegrees);
	return !HasAnyErrors();
}
#endif // WITH_DEV_AUTOMATION_TESTS
