// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassEntityTraitBase.h"
#include "Mass/EntityHandle.h"
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

	/** --- 피격 반응 --- */

	/**
	 * 피격 후 그 방향을 주시하며 정지해 있을 잔여 시간(초). 피격 판정(근접·투사체)이
	 * `ULNPEnemyConfig::HitReactLookTime`으로 세팅하고, `ULNPEnemyMovementProcessor`가 감소시킨다.
	 *
	 * ⚠️ 감소는 상태와 무관하게 매 프레임 돈다. Alert/Confirmed 중에 맞아 남은 타이머가
	 * 나중에 Idle이 될 때 엉뚱하게 발동하는 것을 막기 위해서다 (연출은 `None`에서만 재생된다).
	 */
	UPROPERTY(Transient)
	float HitReactTimer = 0.0f;

	/** 마지막 피격이 날아온 방향 (피격자 → 공격자, 월드 단위벡터). HitReactTimer가 살아 있는 동안 주시 방향으로 쓴다. */
	UPROPERTY(Transient)
	FVector HitReactDirection = FVector::ZeroVector;
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

	/**
	 * **경계 인내** — 추격 자격도 없이 경계만 유지한 누적 시간(초). ScoringProcessor가 누적하며,
	 * 추격 자격을 얻거나(슬롯 대기 포함) 상태가 Alert를 벗어나면 0으로 되돌린다.
	 * `ULNPEnemyConfig::AlertPatienceTime`에 도달하면 그 프레임에 후보 유지를 끊어 Idle로 내려간다.
	 *
	 * ⚠️ **Reset()이 지우지 않는다.** Reset()은 매 프레임 후보 목록을 비우는 용도이고,
	 * 이 값은 프레임을 가로질러 누적되어야 하기 때문이다.
	 */
	UPROPERTY(Transient)
	float AlertDwellTime = 0.0f;

	/**
	 * **재발견 금지 잔여 시간(초)** — 인내를 소진해 포기한 직후 `AlertRecoveryTime`으로 세팅되고
	 * 매 프레임 감소한다. 0보다 크면 시야 발견(`VisionDistance` + FOV)을 건너뛴다.
	 * `AwarenessDistance` 안까지 들어온 상대는 이 금지를 무시한다.
	 *
	 * 인내와 **별도의 float로 둔다.** 하나에 겹치면 값 하나만 보고는 "차오르는 중인지
	 * 회복 중인지"를 구분할 수 없어, 계측으로 원인을 가릴 때 그대로 함정이 된다.
	 *
	 * ⚠️ `AlertDwellTime`과 마찬가지로 **Reset()이 지우지 않는다.**
	 */
	UPROPERTY(Transient)
	float DisengageTimer = 0.0f;

	void Reset()
	{
		NumPotentialTargets = 0;
		for (int32 i = 0; i < 4; ++i)
			PotentialTargets[i].Reset();
		// AlertDwellTime·DisengageTimer는 의도적으로 건드리지 않는다 —
		// 둘 다 프레임을 가로질러 유지되어야 하는 값이다.
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

	/**
	 * 현재 배회 목표를 발급한 뒤 도착하지 못한 채 흐른 시간(초).
	 * IdleTask의 Tick은 신호 구동이라 스스로 시간을 잴 수 없으므로, 매 프레임 도는
	 * ULNPEnemyMovementProcessor가 누적하고 도착 시 0으로 되돌린다.
	 */
	UPROPERTY(Transient)
	float TimeSinceWanderIssued = 0.0f;

	/**
	 * 배회 목표 미도달 타임아웃이 발생했다. MovementProcessor가 세우면서 StateTree를 깨우고,
	 * IdleTask가 목표를 폐기·재추첨하며 내린다. 목표를 결정하는 주체는 IdleTask 하나로 유지한다.
	 */
	UPROPERTY(Transient)
	uint8 bWanderTargetTimedOut : 1 = false;
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

/**
 * 사망한 Player를 적 타게팅에서 제외하는 Tag. 적 쪽 FLNPEnemyDyingTag와 대칭이다.
 *
 * 사망해도 폰은 파괴되지 않고 랙돌 상태로 리스폰 지연시간만큼 월드에 남으므로(ALNPPlayerCharacter::HandleDeathOnServer),
 * 이 태그가 없으면 적들이 시체를 계속 유효한 타겟으로 삼는다.
 * 해제 경로는 필요 없다 — 리스폰은 폰을 파괴하고 새로 스폰하므로(ALNPGameMode::DoRespawn)
 * 살아난 플레이어는 태그 없는 새 엔티티를 받는다.
 */
USTRUCT() struct LOOTNPOP_API FLNPPlayerDeadTag : public FMassTag { GENERATED_BODY() };

/** 이 Entity의 Actor가 초기화됐음을 표시하는 Tag. ActorSyncProcessor가 null Actor 감지 시 제거하여 다음 High LOD 전환 때 ActorInitializer가 재실행되도록 한다. */
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

class UMassReplicationTrait;

/** Enemy Entity Template 설정을 위한 Trait */
UCLASS()
class LOOTNPOP_API ULNPEnemyTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	ULNPEnemyTrait();

	UPROPERTY(EditAnywhere, Category = "LNP|Enemy")
	TObjectPtr<ULNPEnemyConfig> EnemyConfig;

	/** 이 거리를 넘으면 클라이언트 버블에서 제거된다(= 클라에서 사라진다).
	 *  같은 EntityConfig의 MassCrowdVisualizationTrait → VisibleLODDistance[Off]와 같은 값으로 맞출 것 —
	 *  더 크면 렌더링되지도 않을 NPC에 대역폭을 쓰고, 더 작으면 서버엔 보이는데 클라엔 안 보인다.
	 *  Pod과 달리 Enemy는 매 갱신 위치/자세를 싣기 때문에 이 값이 곧 대역폭이다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Enemy")
	float ReplicationCullDistance = 12000.0f;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	/** Enemy MassReplication(Phase 6) — BubbleInfoClass/ReplicatorClass를 LNP 전용 클래스로 고정해 내부적으로 위임한다.
	 *  Standalone(NM_Standalone)에서는 UMassReplicationTrait::BuildTemplate 자체가 조기 반환하므로 별도 분기가 필요 없다. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMassReplicationTrait> ReplicationTrait;
};
