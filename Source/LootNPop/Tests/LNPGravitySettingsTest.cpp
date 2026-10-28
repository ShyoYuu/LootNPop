// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Config/LNPSettings.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "Gravity/LNPPawnGravityComponent.h"
#include "Item/LNPWeaponData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLNPGravitySettingsTest,
	"LootNPop.Gravity.SharedSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLNPGravitySettingsTest::RunTest(const FString& Parameters)
{
	ULNPSettings* Settings = GetMutableDefault<ULNPSettings>();
	TGuardValue<float> RestoreGravity(Settings->GravityStrength, 980.f);
	ULNPPawnGravityComponent* PawnGravity = NewObject<ULNPPawnGravityComponent>();
	ULNPWeaponData* Weapon = NewObject<ULNPWeaponData>();

	// 같은 객체를 유지한 채 원본을 바꿔도 개별 기본값이나 캐시가 공통 설정을 가리지 않아야 한다.
	for (const float Gravity : {980.f, 1372.f, 0.f})
	{
		Settings->GravityStrength = Gravity;
		TestEqual(TEXT("Pawn and ragdoll use shared gravity"), PawnGravity->GetGravityStrength(), Gravity);

		const LNPEnemyExactMovement::FParams EnemyParams;
		TestEqual(TEXT("Enemy movement starts with shared gravity"), EnemyParams.GravityStrength, Gravity);

		Weapon->ProjectileType = ELNPProjectileType::Lobbed;
		TestEqual(TEXT("Lobbed spawning and trajectory guide use shared gravity"), Weapon->GetEffectiveProjectileGravity(), Gravity);
		Weapon->ProjectileType = ELNPProjectileType::Linear;
		TestEqual(TEXT("Linear projectiles stay gravity free"), Weapon->GetEffectiveProjectileGravity(), 0.f);
		Weapon->ProjectileType = ELNPProjectileType::Guided;
		TestEqual(TEXT("Guided projectiles stay gravity free"), Weapon->GetEffectiveProjectileGravity(), 0.f);
	}

	return true;
}

#endif
