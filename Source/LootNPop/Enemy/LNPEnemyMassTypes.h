// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityTraitBase.h"
#include "MassEntityHandle.h"
#include "GameplayTagContainer.h"
#include "LNPEnemyMassTypes.generated.h"

/** 타게팅 슬롯과 인식에 관한 Enemy 상태 */
UENUM(BlueprintType)
enum class ELNPTargetingState : uint8
{
	None,           // 타겟 미감지, 대기 동작
	Alert,          // 타겟 감지됐으나 슬롯 미확보, 타겟 방향 전환
	Confirmed,      // 슬롯 확보, 적극적 추격/공격
};

/** Enemy Entity의 핵심 전투 데이터 */
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

	/** 죽어가는 Entity가 Destroy되기까지 남은 시간(초). Health가 0이 될 때 설정. */
	float DeathCountdown = 0.f;

	/** Enemy 타입 식별 (Melee, Ranged, Elite 등) */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	FGameplayTag EnemyTypeTag;

	/** --- Leash 데이터 --- */

	/** Leash 영역의 중심 (할당된 LootPod 위치) */
	UPROPERTY(Transient)
	FVector ParentPodLocation = FVector::ZeroVector;

	/** 이 Enemy가 속한 LootPod */
	UPROPERTY(Transient)
	FMassEntityHandle ParentLootPod;
};

/** 인식으로 감지된 후보 Player, 슬롯 확인 대기 중 */
USTRUCT()
struct LOOTNPOP_API FLNPEnemyTargetingCandidateFragment : public FMassFragment
{
	GENERATED_BODY()

	/** 우선순위 순(가장 가까운 순)으로 정렬된 잠재적 Player 타겟 목록 */
	UPROPERTY(Transient)
	FMassEntityHandle PotentialTargets[4];

	/** 위 배열의 유효한 잠재적 타겟 수 */
	UPROPERTY(Transient)
	uint8 NumPotentialTargets = 0;

	void Reset()
	{
		NumPotentialTargets = 0;
		for (int32 i = 0; i < 4; ++i)
			PotentialTargets[i].Reset();
	}
};

/** 타게팅의 최종 결정 데이터 */
USTRUCT()
struct LOOTNPOP_API FLNPEnemyTargetingFragment : public FMassFragment
{
	GENERATED_BODY()

	/** 현재 타게팅 중인 Player Entity (확정됐거나 최선의 Alert 타겟) */
	UPROPERTY(Transient)
	FMassEntityHandle TargetPlayer;

	/** 현재 슬롯 점유 상태 */
	UPROPERTY(Transient)
	ELNPTargetingState State = ELNPTargetingState::None;

	/** 타겟 Player의 마지막으로 알려진 위치 */
	UPROPERTY(Transient)
	FVector TargetLocation = FVector::ZeroVector;

	/** 정렬 최적화를 위한 타겟까지의 거리 제곱 */
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

/** 지속적인 대기 행동 상태 Fragment */
USTRUCT()
struct LOOTNPOP_API FLNPEnemyIdleFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	double LastWanderTime = 0.0;

	UPROPERTY(Transient)
	uint8 bNeedNewWanderTarget : 1 = true;
};

/** Entity 모드 시뮬레이션용 물리 속도 (넉백, 포물선). 지면 접지 시 0. */
USTRUCT()
struct LOOTNPOP_API FLNPEnemyVelocityFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FVector Velocity = FVector::ZeroVector;
};

/** Entity를 Enemy으로 식별하는 Tag */
USTRUCT() struct LOOTNPOP_API FLNPEnemyTag : public FMassTag { GENERATED_BODY() };

/** Entity를 Player로 식별하는 Tag */
USTRUCT() struct LOOTNPOP_API FLNPPlayerTag : public FMassTag { GENERATED_BODY() };

/** 이 Entity의 Actor가 초기화됐음을 표시하는 Tag */
USTRUCT() struct LOOTNPOP_API FLNPEnemyActorInitializedTag : public FMassTag { GENERATED_BODY() };

/** Destroy 대기 상태 Tag */
USTRUCT() struct LOOTNPOP_API FLNPEnemyDyingTag : public FMassTag { GENERATED_BODY() };

class ULNPEnemyConfig;

/** Enemy 그룹의 Shared config 데이터 */
USTRUCT()
struct LOOTNPOP_API FLNPEnemySharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "LNP|Config")
	TObjectPtr<ULNPEnemyConfig> Config;
};

/** Enemy Entity Template 설정을 위한 Trait */
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
