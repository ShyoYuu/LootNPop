// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LNPSettings.generated.h"

class ULNPOctantPoolData;
class ULNPMassSpawnConfig;

/**
 * LootNPop 전역 프로젝트 설정.
 * Project Settings -> Game -> LNP Settings에서 편집할 수 있다.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="LNP Settings"))
class LOOTNPOP_API ULNPSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULNPSettings();

	UPROPERTY(Config, EditAnywhere, Category = "World Generation")
	float SphereRadius = 10000.0f;

	/** World 생성에 사용할 기본 Octant Pool. */
	UPROPERTY(Config, EditAnywhere, Category = "World Generation")
	TSoftObjectPtr<ULNPOctantPoolData> OctantPool;

	/** 초기 Mass Entity 스폰을 위한 기본 config. */
	UPROPERTY(Config, EditAnywhere, Category = "Mass Spawning")
	TSoftObjectPtr<ULNPMassSpawnConfig> MassSpawnConfig;

	/** 적도에서 인접 Cache 셀 간의 목표 호 길이 거리 (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Surface Cache", meta=(ClampMin="1.0", Units="cm"))
	float SurfaceCacheCellSpacing = 200.0f;

	/** Player 캐릭터당 키 매핑된 Active Skill Slots의 최대 수. */
	UPROPERTY(Config, EditAnywhere, Category = "Ability System", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxActiveSkillSlots = 4;

	/** true이면 Player Projectile이 다른 Player에게 피해를 줄 수 있다. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	bool bFriendlyFire = false;
};
