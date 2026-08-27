// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "Item/LNPWeaponData.h"
#include "LNPEnemyConfig.generated.h"

class ALNPEnemyCharacter;
class UStateTree;

/** 우선순위 점수 계산 설정 */
USTRUCT(BlueprintType)
struct FLNPEnemyTargetingConfig
{
	GENERATED_BODY()

	/** 거리 가중치 (가까울수록 높은 점수) */
	UPROPERTY(EditAnywhere, Category = "LNP|Scoring")
	float DistanceWeight = 1.0f;

	/** Player 시야 각도 가중치 (정면일수록 높은 점수) */
	UPROPERTY(EditAnywhere, Category = "LNP|Scoring")
	float AngleWeight = 0.5f;

	/** 타게팅 고려 최대 거리 */
	UPROPERTY(EditAnywhere, Category = "LNP|Scoring")
	float MaxTargetingDistance = 10000.0f;

	/** 점수가 0으로 떨어지기 시작하는 LootPod로부터의 최대 Leash 거리 */
	UPROPERTY(EditAnywhere, Category = "LNP|Scoring")
	float MaxLeashDistance = 2000.0f;

	/** Enemy가 Player를 감지할 수 있는 거리 */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception")
	float VisionDistance = 2000.0f;

	/** 전체 시야각 (도) */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception")
	float VisionAngle = 90.0f;

	/** FOV와 관계없이 항상 Player를 감지하는 거리 */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception")
	float AwarenessDistance = 200.0f;
};

/** 이동 및 회전 설정 */
USTRUCT(BlueprintType)
struct FLNPEnemyMovementConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	float MoveSpeed = 600.0f;

	/** 초당 회전 각도 */
	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	float RotationRate = 360.0f;

	/** 중력 가속도 크기 (RadialOutward) */
	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	float GravityStrength = 2000.0f;

	/** 중력 원점 (구형 세계의 중심) */
	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	FVector GravityOrigin = FVector::ZeroVector;

	/** 부모 Pod로부터의 최소 배회 거리 */
	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	float WanderMinDistance = 300.0f;

	/** 부모 Pod로부터의 최대 배회 거리 */
	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	float WanderMaxDistance = 800.0f;

	/** Enemy가 공격을 시작하는 거리 */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	float AttackRange = 200.0f;

	/** 공격 간격 (초) */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	float AttackInterval = 1.5f;

	/**
	 * 도착 판정 임계값(cm). 구면 위에서는 **접평면 거리**로 잰다 (TechDesign_EnemyNPC.md §5.1).
	 *
	 * MovementProcessor의 도착 신호와 IdleTask의 배회 완료 판정이 반드시 같은 값을 봐야 한다.
	 * StateTree Tick은 신호 구동이라, 신호 조건(이 값)보다 완료 조건이 느슨하면 완료 처리가
	 * 신호 없이 성립할 수 없고, 반대면 신호가 와도 완료가 안 잡혀 배회가 교착된다.
	 */
	static constexpr float ArrivalTolerance = 30.f;

	/**
	 * 배회 목표 미도달 타임아웃(초). 이 시간 안에 도착하지 못하면 목표를 폐기하고 재추첨한다.
	 *
	 * 도착 신호만으로는 복구가 불가능하기 때문에 반드시 필요하다 — 도달 불가능한 지점을 한 번
	 * 뽑으면 도착 신호가 영영 오지 않고, 신호가 없으면 StateTree Tick도 돌지 않아 그 개체가
	 * 영구 정지한다. 배회 거리는 WanderMin\~MaxDistance(300\~800cm)이고 배회 속도는
	 * MoveSpeed의 0.3배(=180cm/s)이므로 정상 도달은 2\~5초다. 우회를 감안해 넉넉히 잡는다.
	 */
	static constexpr float WanderTimeout = 10.f;

	/**
	 * Chase 정지 거리: AttackRange 안쪽에서 멈추되, 도착 신호가 반드시 발생하도록
	 * ArrivalTolerance 이상의 버퍼를 확보한다. TargetFollow(MoveTarget 산출)·Movement(속도 결정)·
	 * SteeringTask(StateTree)가 반드시 같은 값을 봐야 정지 지점이 일치한다.
	 */
	static float ComputeStopDistance(const float AttackRange)
	{
		const float StopBuffer = FMath::Max(ArrivalTolerance, FMath::Min(AttackRange * 0.1f, 100.f));
		return FMath::Max(0.f, AttackRange - StopBuffer);
	}
};

/**
 * Enemy의 정체성, 비주얼, 행동을 정의하는 Data Asset.
 */
UCLASS()
class LOOTNPOP_API ULNPEnemyConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 이 Enemy 타입을 식별하는 Tag (예: Enemy.Type.Humanoid.Melee) */
	UPROPERTY(EditAnywhere, Category = "LNP|Identity")
	FGameplayTag EnemyTypeTag;

	/** Mass에서 Actor로 전환 시 스폰할 Actor 클래스 */
	UPROPERTY(EditAnywhere, Category = "LNP|Spawning")
	TSubclassOf<ALNPEnemyCharacter> EnemyActorClass;

	/** 이 Enemy 타입에서 실행할 StateTree 에셋 */
	UPROPERTY(EditAnywhere, Category = "LNP|AI")
	TObjectPtr<UStateTree> StateTree;

	/** 모든 공격에 사용하는 고정 무기 (Enemy은 무기를 교체할 수 없다) */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	TObjectPtr<ULNPWeaponData> WeaponData;

	/** GAS 설정 */
	UPROPERTY(EditAnywhere, Category = "LNP|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** 초기 Attribute (Health, Defense 등) */
	UPROPERTY(EditAnywhere, Category = "LNP|Attributes")
	TMap<FGameplayTag, float> InitialAttributeValues;

	/** 타게팅 및 균형 설정 */
	UPROPERTY(EditAnywhere, Category = "LNP|Targeting")
	FLNPEnemyTargetingConfig TargetingConfig;

	/** 이동 및 회전 설정 */
	UPROPERTY(EditAnywhere, Category = "LNP|Movement")
	FLNPEnemyMovementConfig MovementConfig;

	/** 피격 감지 Processor가 사용하는 충돌 Capsule 크기. */
	UPROPERTY(EditAnywhere, Category = "LNP|Collision", meta = (ClampMin = "1"))
	float CapsuleHalfHeight = 88.f;

	UPROPERTY(EditAnywhere, Category = "LNP|Collision", meta = (ClampMin = "1"))
	float CapsuleRadius = 35.f;

	/** Asset Manager가 ID로 이 에셋을 식별하기 위해 필요 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("EnemyConfig"), GetFName());
	}
};
