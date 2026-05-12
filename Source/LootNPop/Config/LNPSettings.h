// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LNPSettings.generated.h"

class ULNPOctantPoolData;
class ULNPMassSpawnConfig;

/**
 * Global project settings for LootNPop.
 * These can be edited in Project Settings -> Game -> LNP Settings.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="LNP Settings"))
class LOOTNPOP_API ULNPSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULNPSettings();

	UPROPERTY(Config, EditAnywhere, Category = "World Generation")
	float SphereRadius = 10000.0f;

	/** Default pool of octants to use for world generation. */
	UPROPERTY(Config, EditAnywhere, Category = "World Generation")
	TSoftObjectPtr<ULNPOctantPoolData> OctantPool;

	/** Default configuration for initial mass entity spawning. */
	UPROPERTY(Config, EditAnywhere, Category = "Mass Spawning")
	TSoftObjectPtr<ULNPMassSpawnConfig> MassSpawnConfig;

	/** Target arc-length distance between adjacent cache cells at the equator (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Surface Cache", meta=(ClampMin="1.0", Units="cm"))
	float SurfaceCacheCellSpacing = 200.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Surface Cache")
	int32 SurfaceCacheTracesPerFrame = 256;

	/** Maximum number of key-mapped Active Skill slots per player character. */
	UPROPERTY(Config, EditAnywhere, Category = "Ability System", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxActiveSkillSlots = 4;
};
