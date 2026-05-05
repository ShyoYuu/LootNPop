// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityTraitBase.h"
#include "LNPLootPodMassTypes.generated.h"

// 1. LootPod State Definition
UENUM(BlueprintType)
enum class ELNPLootPodState : uint8
{
	Idle,       // Waiting for interaction
	Looting,    // Looting in progress (gauge increasing)
	Interrupted,// Interrupted by enemy attack (waiting for reset)
	Popped      // Looting complete (waiting for rewards spawn/cleanup)
};

// --- Tags ---
/** Tag for LootPods waiting for a player */
USTRUCT() struct LOOTNPOP_API FLNPLootPodIdleTag : public FMassTag { GENERATED_BODY() };

/** Tag for LootPods currently being looted */
USTRUCT() struct LOOTNPOP_API FLNPLootPodLootingTag : public FMassTag { GENERATED_BODY() };

/** Tag for Players currently engaged in looting */
USTRUCT() struct LOOTNPOP_API FLNPPlayerLootingTag : public FMassTag { GENERATED_BODY() };

/** 2. LootPod Data Fragment */
USTRUCT()
struct LOOTNPOP_API FLNPLootPodFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	ELNPLootPodState State = ELNPLootPodState::Idle;

	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	float CurrentGauge = 0.0f;

	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	float MaxGauge = 100.0f;

	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	float LootableDistSquared = 90000.0f;

	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	int32 PodID = 0; // Unique identifier for reward lookup
};

/** 3. Player Looting Fragment (Added to Player Entity) */
USTRUCT()
struct LOOTNPOP_API FLNPPlayerLootingFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	float BuffedLootSpeed = 1.0f;
};

// 4. LootPod Identification Tag
USTRUCT()
struct LOOTNPOP_API FLNPLootPodTag : public FMassTag { GENERATED_BODY() };

// 5. LootPod Entity Trait
UCLASS()
class LOOTNPOP_API ULNPLootPodTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()
protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
