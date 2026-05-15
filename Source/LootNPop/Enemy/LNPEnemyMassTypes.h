// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityTraitBase.h"
#include "MassEntityHandle.h"
#include "GameplayTagContainer.h"
#include "LNPEnemyMassTypes.generated.h"

/** State of an enemy regarding targeting slots and awareness */
UENUM(BlueprintType)
enum class ELNPTargetingState : uint8
{
	None,           // No target detected, Idle behavior
	Alert,          // Target detected but no slot secured, facing target
	Confirmed,      // Slot secured, aggressive chase/attack
};

/** Core combat data for Enemy Entities */
USTRUCT()
struct LOOTNPOP_API FLNPEnemyFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	float Health = 100.0f;

	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	float Defense = 0.0f;

	/** Seconds remaining before a dying entity is destroyed. Set when Health hits 0. */
	float DeathCountdown = 0.f;

	/** Identifies the type of enemy (Melee, Ranged, Elite, etc.) */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	FGameplayTag EnemyTypeTag;

	/** --- Leash Data --- */

	/** Center of the leash area (assigned LootPod location) */
	UPROPERTY(Transient)
	FVector ParentPodLocation = FVector::ZeroVector;

	/** The LootPod this enemy belongs to */
	UPROPERTY(Transient)
	FMassEntityHandle ParentLootPod;
};

/** Candidate players detected by perception, pending slot confirmation */
USTRUCT()
struct LOOTNPOP_API FLNPEnemyTargetingCandidateFragment : public FMassFragment
{
	GENERATED_BODY()

	/** List of potential player targets, sorted by priority (closest first) */
	UPROPERTY(Transient)
	FMassEntityHandle PotentialTargets[4];

	/** Number of valid potential targets in the array above */
	UPROPERTY(Transient)
	uint8 NumPotentialTargets = 0;

	void Reset()
	{
		NumPotentialTargets = 0;
		for (int32 i = 0; i < 4; ++i)
			PotentialTargets[i].Reset();
	}
};

/** Final decision data for targeting */
USTRUCT()
struct LOOTNPOP_API FLNPEnemyTargetingFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Current player entity being targeted (confirmed or best alert target) */
	UPROPERTY(Transient)
	FMassEntityHandle TargetPlayer;

	/** Current slot occupancy state */
	UPROPERTY(Transient)
	ELNPTargetingState State = ELNPTargetingState::None;

	/** Last known location of the target player */
	UPROPERTY(Transient)
	FVector TargetLocation = FVector::ZeroVector;

	/** Squared distance to target for sorting optimization */
	UPROPERTY(Transient)
	float DistanceToTargetSq = 0.0f;

	void ResetTargeting()
	{
		TargetPlayer.Reset();
		State = ELNPTargetingState::None;
		TargetLocation = FVector::ZeroVector;
		DistanceToTargetSq = 0.0f;
	}
};

/** Fragment for persistent idle behavior state */
USTRUCT()
struct LOOTNPOP_API FLNPEnemyIdleFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	double LastWanderTime = 0.0;

	UPROPERTY(Transient)
	uint8 bNeedNewWanderTarget : 1 = true;
};

/** Tag to identify an entity as an Enemy */
USTRUCT() struct LOOTNPOP_API FLNPEnemyTag : public FMassTag { GENERATED_BODY() };

/** Tag to identify an entity as a Player */
USTRUCT() struct LOOTNPOP_API FLNPPlayerTag : public FMassTag { GENERATED_BODY() };

/** Tag to mark that the actor for this entity has been initialized */
USTRUCT() struct LOOTNPOP_API FLNPEnemyActorInitializedTag : public FMassTag { GENERATED_BODY() };

/** Tag marking an enemy whose Health hit 0; entity is waiting for DeathCountdown before being destroyed */
USTRUCT() struct LOOTNPOP_API FLNPEnemyDyingTag : public FMassTag { GENERATED_BODY() };

class ULNPEnemyConfig;

/** Shared configuration data for a group of enemies */
USTRUCT()
struct LOOTNPOP_API FLNPEnemySharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "LNP|Config")
	TObjectPtr<ULNPEnemyConfig> Config;
};

/** Trait to configure an Enemy Entity Template */
UCLASS()
class LOOTNPOP_API ULNPEnemyTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "LNP|Enemy")
	TObjectPtr<ULNPEnemyConfig> EnemyConfig;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
