// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "Item/LNPWeaponData.h"
#include "LNPEnemyConfig.generated.h"

class ALNPEnemyCharacter;
class UStateTree;

/** Configuration for priority score calculation */
USTRUCT(BlueprintType)
struct FLNPEnemyTargetingConfig
{
	GENERATED_BODY()

	/** Weight for distance (Closer = Higher Score) */
	UPROPERTY(EditAnywhere, Category = "LNP|Scoring")
	float DistanceWeight = 1.0f;

	/** Weight for player view angle (In Front = Higher Score) */
	UPROPERTY(EditAnywhere, Category = "LNP|Scoring")
	float AngleWeight = 0.5f;

	/** Maximum distance to even consider targeting */
	UPROPERTY(EditAnywhere, Category = "LNP|Scoring")
	float MaxTargetingDistance = 10000.0f;

	/** Maximum distance from the LootPod before scoring drops to zero */
	UPROPERTY(EditAnywhere, Category = "LNP|Scoring")
	float MaxLeashDistance = 2000.0f;

	/** Distance within which the enemy can detect the player */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception")
	float VisionDistance = 2000.0f;

	/** Total field of view angle in degrees */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception")
	float VisionAngle = 90.0f;

	/** Distance within which the enemy always detects the player regardless of FOV */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception")
	float AwarenessDistance = 200.0f;
};

/** Configuration for movement and rotation */
USTRUCT(BlueprintType)
struct FLNPEnemyMovementConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	float MoveSpeed = 600.0f;

	/** Degrees per second */
	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	float RotationRate = 360.0f;

	/** Acceleration magnitude for gravity (Radial Outward) */
	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	float GravityStrength = 2000.0f;

	/** Center of gravity origin (center of the sphere world) */
	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	FVector GravityOrigin = FVector::ZeroVector;

	/** Minimum wander distance from the parent Pod */
	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	float WanderMinDistance = 300.0f;

	/** Maximum wander distance from the parent Pod */
	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	float WanderMaxDistance = 800.0f;

	/** Distance within which the enemy will start attacking */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	float AttackRange = 200.0f;

	/** Time between attacks in seconds */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	float AttackInterval = 1.5f;
};

/**
 * Data asset defining an Enemy's identity, visuals, and behavior.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Tag to identify this enemy type (e.g., Enemy.Type.Humanoid.Melee) */
	UPROPERTY(EditAnywhere, Category = "LNP|Identity")
	FGameplayTag EnemyTypeTag;

	/** The Actor class to spawn when transitioning from Mass to Actor */
	UPROPERTY(EditAnywhere, Category = "LNP|Spawning")
	TSubclassOf<ALNPEnemyCharacter> EnemyActorClass;

	/** The StateTree asset to run for this enemy type */
	UPROPERTY(EditAnywhere, Category = "LNP|AI")
	TObjectPtr<UStateTree> StateTree;

	/** Static weapon used for all attacks (enemies cannot swap weapons) */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	TObjectPtr<ULNPWeaponData> WeaponData;

	/** Gameplay Ability System setup */
	UPROPERTY(EditAnywhere, Category = "LNP|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Initial attributes (Health, Defense, etc.) */
	UPROPERTY(EditAnywhere, Category = "LNP|Attributes")
	TMap<FGameplayTag, float> InitialAttributeValues;

	/** Targeting and Balancing setup */
	UPROPERTY(EditAnywhere, Category = "LNP|Targeting")
	FLNPEnemyTargetingConfig TargetingConfig;

	/** Movement and Rotation setup */
	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	FLNPEnemyMovementConfig MovementConfig;

	/** Collision capsule dimensions used by hit detection processors. */
	UPROPERTY(EditAnywhere, Category = "LNP|Collision", meta = (ClampMin = "1"))
	float CapsuleHalfHeight = 88.f;

	UPROPERTY(EditAnywhere, Category = "LNP|Collision", meta = (ClampMin = "1"))
	float CapsuleRadius = 35.f;

	/** Required for Asset Manager to identify this asset by ID */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("EnemyConfig"), GetFName());
	}
};
