// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityTraitBase.h"
#include "LNPLootPodMassTypes.generated.h"

// 1. LootPod 상태 정의
UENUM(BlueprintType)
enum class ELNPLootPodState : uint8
{
	Idle,       // 상호작용 대기
	Looting,    // 루팅 진행 중 (게이지 증가)
	Interrupted,// Enemy의 공격으로 중단 (리셋 대기)
	Popped      // 루팅 완료 (보상 스폰/정리 대기)
};

// --- Tag ---
/** Player를 기다리는 LootPod Tag */
USTRUCT() struct LOOTNPOP_API FLNPLootPodIdleTag : public FMassTag { GENERATED_BODY() };

/** 현재 루팅 중인 LootPod Tag */
USTRUCT() struct LOOTNPOP_API FLNPLootPodLootingTag : public FMassTag { GENERATED_BODY() };

/** 현재 루팅 중인 Player Tag */
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
	int32 PodID = 0; // 보상 조회를 위한 고유 식별자
};

/** 3. Player 루팅 Fragment (Player Entity에 추가) */
USTRUCT()
struct LOOTNPOP_API FLNPPlayerLootingFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "LNP|LootPod")
	float BuffedLootSpeed = 1.0f;
};

// 4. LootPod 식별 Tag
USTRUCT()
struct LOOTNPOP_API FLNPLootPodTag : public FMassTag { GENERATED_BODY() };

// 5. LootPod Entity Trait (Entity 트레이트)
UCLASS()
class LOOTNPOP_API ULNPLootPodTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()
protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
