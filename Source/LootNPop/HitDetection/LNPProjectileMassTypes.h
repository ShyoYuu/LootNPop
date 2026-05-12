// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityHandle.h"
#include "LNPProjectileMassTypes.generated.h"

class ULNPVFXData;

UENUM(BlueprintType)
enum class ELNPProjectileType : uint8
{
	Linear UMETA(DisplayName = "Linear"),
	Guided UMETA(DisplayName = "Guided"),
	Lobbed UMETA(DisplayName = "Lobbed"),
};

/**
 * Weapon-type constants shared across all projectiles spawned from the same weapon.
 * Stored as a ConstSharedFragment so entities with the same values share a chunk.
 */
USTRUCT()
struct LOOTNPOP_API FLNPProjectileSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ULNPVFXData> VFXData = nullptr;

	ELNPProjectileType Type      = ELNPProjectileType::Linear;
	float              Damage    = 10.0f;
	float              HitRadiusSq = 25.0f;
};

/**
 * Per-projectile simulation state.
 * PreviousPos/CurrentPos form the swept line segment used by HitDetection each frame.
 */
USTRUCT()
struct LOOTNPOP_API FLNPProjectileFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector PreviousPos   = FVector::ZeroVector; // previous frame position, used for sweep-based hit detection
	FVector Velocity      = FVector::ZeroVector;
	FVector SpawnLocation = FVector::ZeroVector; // initial spawn position, used once for SpawnEffects

	float LifetimeRemaining = 5.0f;

	UPROPERTY()
	FMassEntityHandle Instigator;
};

/** Tracks whether Niagara trail components have been allocated for this projectile. */
USTRUCT()
struct LOOTNPOP_API FLNPProjectileVisualFragment : public FMassFragment
{
	GENERATED_BODY()

	bool bInitialized = false;
};
