// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityHandle.h"
#include "GameplayEffect.h"
#include "LNPProjectileMassTypes.generated.h"

class ULNPVFXData;

UENUM(BlueprintType)
enum class ELNPProjectileType : uint8
{
	Linear UMETA(DisplayName = "Linear"),
	Guided UMETA(DisplayName = "Guided"),
	Lobbed UMETA(DisplayName = "Lobbed"),
};

UENUM(BlueprintType)
enum class ELNPInstigatorTeam : uint8
{
	Enemy,
	Player,
};

/**
 * 같은 무기에서 스폰된 모든 Projectile가 공유하는 무기 타입 상수.
 * 동일한 값을 가진 Entity가 Chunk를 공유하도록 ConstSharedFragment로 저장된다.
 */
USTRUCT()
struct LOOTNPOP_API FLNPProjectileSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ULNPVFXData> VFXData = nullptr;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY()
	ELNPProjectileType Type        = ELNPProjectileType::Linear;

	UPROPERTY()
	float Damage      = 10.0f;

	UPROPERTY()
	float HitRadiusSq = 25.0f;
};

/**
 * Projectile별 시뮬레이션 상태.
 * PreviousPos/CurrentPos가 매 프레임 HitDetection에서 사용하는 스윕 선분을 형성한다.
 */
USTRUCT()
struct LOOTNPOP_API FLNPProjectileFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector PreviousPos   = FVector::ZeroVector; // 이전 프레임 위치, 스윕 기반 피격 감지에 사용
	FVector Velocity      = FVector::ZeroVector;
	FVector SpawnLocation = FVector::ZeroVector; // 초기 스폰 위치, SpawnEffects에 한 번 사용

	float LifetimeRemaining = 5.0f;

	UPROPERTY()
	FMassEntityHandle Instigator;

	ELNPInstigatorTeam InstigatorTeam = ELNPInstigatorTeam::Enemy;
};

/** 이 Projectile에 Niagara 트레일 Component가 할당됐는지 추적한다. */
USTRUCT()
struct LOOTNPOP_API FLNPProjectileVisualFragment : public FMassFragment
{
	GENERATED_BODY()

	bool bInitialized = false;
};

/** StartPhysics 단계 종료 시 Projectile Destroy을 표시하는 Tag. */
USTRUCT()
struct LOOTNPOP_API FLNPProjectileDeadTag : public FMassTag
{
	GENERATED_BODY()
};
