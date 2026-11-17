// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Enemy/LNPEnemyConfig.h"

namespace LNPEnemyTerritoryTest
{
	/** Pod 출현 반경의 최댓값(authored EnemyFormations 25m). 홈 반경이 이보다 작으면 스폰 직후 귀환으로 달린다. */
	constexpr float MaxPodSpawnRadius = 2500.f;

	/** bGroundReturn: 홈 반경 밖에서 정상 속도로 귀환하는 지상 개체인가(비행·벽보행은 배회점을 원래 Pod 기준으로 잡아 귀환 이동이 없다). */
	void CheckInvariants(FAutomationTestBase& Test, const FString& Label, const FLNPEnemyTargetingConfig& Targeting,
		const FLNPEnemyMovementConfig& Movement, const bool bGroundReturn)
	{
		Test.TestTrue(Label + TEXT(": awareness < vision"), Targeting.AwarenessDistance < Targeting.VisionDistance);
		Test.TestTrue(Label + TEXT(": vision < retention"), Targeting.VisionDistance < Targeting.AlertRetentionDistance);
		Test.TestTrue(Label + TEXT(": retention <= chase radius"), Targeting.AlertRetentionDistance <= Targeting.ChaseRadius);
		// 홈 안에서 시야에 든 플레이어는 늘 세력권 안이어야 한다 — 홈 안에서는 세력권 밖 대치가 생기지 않는다.
		Test.TestTrue(Label + FString::Printf(TEXT(": home(%.0f) + vision(%.0f) <= chase radius(%.0f)"),
				Targeting.HomeRadius, Targeting.VisionDistance, Targeting.ChaseRadius),
			Targeting.HomeRadius + Targeting.VisionDistance <= Targeting.ChaseRadius);
		if (bGroundReturn)
		{
			// 귀환 목표(Pod 주변 배회점)에 도착했는데 여전히 홈 밖이면 다음 추첨도 귀환이라 정상 속도와 느린 배회가 섞인다.
			Test.TestTrue(Label + FString::Printf(TEXT(": home radius(%.0f) covers wander ring(%.0f)"),
				Targeting.HomeRadius, Movement.WanderMaxDistance), Targeting.HomeRadius > Movement.WanderMaxDistance);
			Test.TestTrue(Label + TEXT(": home radius covers pod spawn radius"), Targeting.HomeRadius > MaxPodSpawnRadius);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPEnemyTerritoryVisionGateTest,
	"LootNPop.Enemy.Territory.VisionGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPEnemyTerritoryVisionGateTest::RunTest(const FString& Parameters)
{
	const FLNPEnemyTargetingConfig Config;
	const FVector Pod(30000, 0, 0);
	const auto At = [&Pod](const float Distance) { return Pod + FVector(0, Distance, 0); };

	// 2026-11-17 재현: Pod에서 58m 떨어져 선 적, 3~4m 앞의 플레이어는 Pod에서 60.9m(세력권 50m 밖).
	const FVector StandoffEnemy = At(5800.f);
	const FVector StandoffPlayer = At(6087.f);
	TestTrue(TEXT("standoff enemy is away from home"), Config.IsAwayFromHome(StandoffEnemy, Pod));
	TestFalse(TEXT("standoff player is outside territory"), Config.IsInTerritory(StandoffPlayer, Pod));
	TestFalse(TEXT("away enemy does not re-acquire player outside territory"),
		Config.AllowsVisionAcquire(StandoffEnemy, Pod, StandoffPlayer));

	// 귀환 중이어도 세력권 안으로 돌아온 플레이어는 발견한다(재진입이 곧 해제).
	TestTrue(TEXT("away enemy acquires player inside territory"),
		Config.AllowsVisionAcquire(StandoffEnemy, Pod, At(4500.f)));
	TestTrue(TEXT("territory boundary is inclusive"), Config.IsInTerritory(At(Config.ChaseRadius), Pod));

	// 홈 안 개체는 예전 규칙 그대로다(세력권 밖 플레이어도 시야 발견 가능, 추격 자격은 따로 판정).
	const FVector HomeEnemy = At(Config.HomeRadius);
	TestFalse(TEXT("home boundary is still home"), Config.IsAwayFromHome(HomeEnemy, Pod));
	TestTrue(TEXT("home enemy keeps old vision rule"), Config.AllowsVisionAcquire(HomeEnemy, Pod, StandoffPlayer));
	TestTrue(TEXT("just past home radius is away"), Config.IsAwayFromHome(At(Config.HomeRadius + 1.f), Pod));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLNPEnemyTerritoryConfigInvariantsTest,
	"LootNPop.Enemy.Territory.ConfigInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPEnemyTerritoryConfigInvariantsTest::RunTest(const FString& Parameters)
{
	LNPEnemyTerritoryTest::CheckInvariants(*this, TEXT("defaults"), FLNPEnemyTargetingConfig(), FLNPEnemyMovementConfig(), true);

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
		if (TestNotNull(Path, Config))
		{
			LNPEnemyTerritoryTest::CheckInvariants(*this, Path, Config->TargetingConfig, Config->MovementConfig,
				!Config->IsFlying() && !Config->IsCrawling());
		}
	}
	return true;
}

#endif
