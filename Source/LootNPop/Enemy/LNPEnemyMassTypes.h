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

/** 순수 엔티티 기본 공격의 위상. 근접은 Active가 칼날 생존 구간, 원거리는 Windup 종료 시 1회 발사한다. */
UENUM()
enum class ELNPEntityAttackPhase : uint8
{
	None,
	Windup,     // 선딜 — 플레이어가 읽고 반응할 구간
	Active,     // 근접: 칼날이 살아 있는 구간 / 원거리: 미사용
	Recovery,   // 후딜
};

/**
 * 순수 엔티티 공격의 상태 기계. **판단은 StateTree Task가, 진행은 프로세서가** 맡는다.
 *
 * ⚠️ **`ActorPromoted` 개체에도 붙인다 — 모드로 아키타입을 가르지 않는다.**
 * 아키타입이 갈리면 같은 쿼리를 두 벌 유지해야 하고, StateTree 외부 데이터 핸들이 Optional이 되어
 * Task 코드에 null 분기가 생긴다. 대가는 개체당 ~24바이트로 두 비용보다 압도적으로 싸다.
 *
 * 위상 진행을 Task에 두지 않는 이유는 배회 교착과 같다 — Mass StateTree의 Task Tick은
 * `StateTreeActivate` 신호가 있어야만 돌아서, 신호가 끊기면 스윙이 중간에 멈춘 채 칼날 엔티티만
 * 살아남는다. **시간 측정과 진행은 매 프레임 도는 프로세서가 맡는다.**
 */
USTRUCT()
struct LOOTNPOP_API FLNPEntityAttackFragment : public FMassFragment
{
	GENERATED_BODY()

	ELNPEntityAttackPhase Phase = ELNPEntityAttackPhase::None;

	/** 현재 위상이 시작된 뒤 흐른 시간(초). */
	float PhaseElapsed = 0.f;

	/** 다음 공격까지 남은 시간(초). FLNPEnemyMovementConfig::AttackInterval에서 채워진다. */
	float CooldownRemaining = 0.f;

	/** 근접 — Active 구간 동안 살아 있는 칼날(FLNPWeaponTraceFragment) 엔티티. */
	UPROPERTY(Transient)
	FMassEntityHandle SwingEntity;

	/** StateTree Task가 세우고 프로세서가 소비하는 1회성 요청. */
	uint8 bAttackRequested : 1 = 0;
};

/**
 * 게스트가 그릴 수 있는 최소한의 행동 상태. **3비트에 들어가야 한다** — 복제 페이로드에서
 * 전이 카운터 5비트와 한 바이트를 나눠 쓴다 (FLNPReplicatedAgent::ActionAndSeq).
 *
 * 값을 늘리기 전에 반드시 대역폭을 다시 볼 것. 8개를 넘기는 순간 1바이트가 깨진다.
 */
UENUM()
enum class ELNPEnemyAction : uint8
{
	Idle,
	Move,
	Attack,
	Stagger,
	Dying,

	MAX UMETA(Hidden)
};

/**
 * 서버가 산출하고 게스트가 그대로 읽는 **행동 상태 채널**. 순수 엔티티의 공격은 서버 전용
 * Mass 로직이라, 이 채널이 없으면 게스트 화면에서 적은 아무것도 하지 않는 것처럼 보인다.
 *
 * 발사마다 Multicast RPC를 쏘지 않는 이유: `RPC 수 = 발사 수 x 개체 수`가 되어
 * "순수 엔티티는 다수"라는 전제와 정면으로 충돌한다. 다수를 전제하면 연출은 이벤트가 아니라
 * **상태**로 흘러야 한다.
 *
 * ⚠️ **`Seq`(전이 카운터)가 반드시 필요하다.** 상태 값만 보내면 연속 공격(Attack -> Attack)의
 * 두 번째 시작을 놓친다 — 값이 바뀌지 않기 때문이다. 전이마다 1 올리고, 클라는 **카운터가
 * 바뀌면** 해당 연출을 처음부터 재생한다. 5비트 wrap으로 충분하다.
 *
 * ⚠️ **재생 시각(타임스탬프)은 싣지 않는다.** 갱신 주기가 0.1~0.3초라 위상 동기는 어차피 근사이고,
 * 4바이트가 늘어난다. 엔진의 같은 계열 구조체(`FReplicatedAgentPathData`)는 `ActionServerStartTime`을
 * double로 싣지만, 판정이 이미 서버 권위인 여기서는 위상이 어긋나도 게임플레이에 영향이 없다.
 *
 * `FLNPEntityAttackFragment`와 같은 이유로 **`CombatMode`와 무관하게 전원에게 붙인다** —
 * 모드로 아키타입을 가르면 같은 쿼리를 두 벌 유지해야 한다.
 */
USTRUCT()
struct LOOTNPOP_API FLNPEnemyActionFragment : public FMassFragment
{
	GENERATED_BODY()

	ELNPEnemyAction Action = ELNPEnemyAction::Idle;

	/** 전이마다 +1. 하위 5비트만 복제되므로 비교는 반드시 "같은가"로만 한다(대소 비교 금지). */
	uint8 Seq = 0;

	/**
	 * **서버 전용 장부** — 직전 프레임의 캡슐 중심. Idle/Move 판별을 실제 변위로 하기 위해 든다.
	 *
	 * 이동 여부의 신호로 `FMassMoveTargetFragment::DesiredSpeed`를 쓰지 않는 이유: 배회(`None`) 중에는
	 * `ULNPEnemyMovementProcessor`가 속도를 자기 안에서(`BaseMoveSpeed * 0.3`) 정하고 MoveTarget에
	 * 되쓰지 않아 그 값이 낡는다. 실제 변위는 어느 경로로 움직였든 항상 맞고, 애니메이션이 원하는 것도
	 * "실제로 움직이는가"다. 클라이언트에서는 쓰이지 않는다(수신값을 그대로 쓴다).
	 */
	FVector PrevPosition = FVector::ZeroVector;

	/**
	 * 발사 시점의 상하 조준각 — **1/127 단위로 양자화한 ∓90도**(약 0.7도, 30m에서 37cm).
	 *
	 * 서버가 **실제로 발사하는 순간** 쓴 클램프된 Pitch를 그대로 싣는다. 게스트가 이 값을 모르면
	 * 고스트가 수평으로 날아가, 고저차 교전에서 **"게스트 화면에선 피했는데 서버에선 맞는"** 상태가 된다.
	 * (2026-09-05 플레이 테스트에서 확인되어 넣었다 — 상태 채널은 "없어서 못 읽겠다"가 확인된 것만 넓힌다.)
	 *
	 * 발사 순간에만 바뀌므로 델타 압축이 나머지 갱신에서 1비트로 접는다.
	 */
	int8 AimPitch = 0;

	/** 도 단위 각도를 와이어 표현으로. ∓90도를 int8 전 범위에 편다. */
	static int8 EncodeAimPitch(const float PitchDeg)
	{
		return static_cast<int8>(FMath::Clamp(FMath::RoundToInt32(PitchDeg * (127.f / 90.f)), -127, 127));
	}

	static float DecodeAimPitch(const int8 Encoded) { return static_cast<float>(Encoded) * (90.f / 127.f); }

	/**
	 * **클라이언트 전용 장부** — 관전용 Ghost를 이미 만들어 준 전이 카운터.
	 *
	 * `Seq`와 다르면 아직 소비하지 않은 전이다. 버블에 새로 들어온 적은 스폰 시점에 `Seq`로 맞춰
	 * **이미 지나간 발사를 뒤늦게 쏘지 않게** 한다 — 그 발사체는 서버에서 이미 날아가는 중이다.
	 * 서버에서는 쓰이지 않는다.
	 */
	uint8 ConsumedSeq = 0;

	/**
	 * **클라이언트 전용 장부** — 위 카운터를 소비할 때의 행동. 한 번의 공격이 전이를 **두 번** 만들기 때문에
	 * 둘을 갈라야 한다:
	 *
	 * ```
	 *   Move -> Attack (Seq+1)   선딜 시작. 게스트는 자세만 바꾸고 발사하지 않는다.
	 *   Attack -> Attack (Seq+1) 발사. 이때 AimPitch가 확정되고 게스트가 Ghost를 만든다.
	 * ```
	 *
	 * 즉 **직전에 소비한 행동이 이미 Attack이었을 때만** 발사로 읽는다. 두 전이가 한 갱신에 뭉쳐 도착하면
	 * 게스트는 그 발사의 Ghost를 그냥 건너뛴다 — 없는 탄이 생기는 것보다 안 보이는 편이 안전하고,
	 * 이는 복제 주기가 공격 길이보다 길 때 원래 감수하기로 한 스킵과 같은 성질이다.
	 */
	ELNPEnemyAction ConsumedAction = ELNPEnemyAction::Idle;

	/**
	 * 이 행동이 **일회성 연출**인가 — 즉 시작 시각을 놓치면 통째로 못 보게 되는가.
	 *
	 * 복제 갱신 주기 게이트(Low 0.3초)를 우회할 자격의 판별 기준이다. 짧은 공격(총 1.0초)은
	 * 시작과 끝이 두 갱신 사이에 들어가 스킵될 수 있는 반면, 루프 상태(Idle/Move)는 늦게 도착해도
	 * 그림이 같다. **Idle<->Move까지 우회시키면** 멈췄다 걷기를 반복하는 배회 개체가 갱신 수를
	 * 통제 없이 밀어올린다 — 이 구분 하나가 스킵과 플랩을 동시에 막는다.
	 */
	static bool IsOneShot(const ELNPEnemyAction InAction)
	{
		return InAction == ELNPEnemyAction::Attack
			|| InAction == ELNPEnemyAction::Stagger
			|| InAction == ELNPEnemyAction::Dying;
	}
};

/**
 * 순수 엔티티가 만든 가상 칼날임을 표시하고 주인을 되가리킨다.
 *
 * ⚠️ **Tag가 아니라 Fragment인 이유:** 칼날 엔티티는 `FMassCommandBuildEntity` 한 번으로 만들어야 한다.
 * `BuildEntity`와 `AddTag`를 같은 배치에 디퍼드하면 아키타입 전환 타이밍 때문에 쿼리가 그 엔티티를
 * 못 찾는다 (`UANS_LNPMeleeHitWindow`가 같은 이유로 Tag를 쓰지 않는다).
 */
USTRUCT()
struct LOOTNPOP_API FLNPEntitySwingFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FMassEntityHandle Owner;
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

	/**
	 * `ULNPEnemyConfig::CombatMode`와 EntityConfig의 표현 매핑이 어긋났는지 검사한다.
	 *
	 * 승격 여부의 단일 진실은 enum이지만 **실제로 Actor를 스폰할지는 표현 매핑이 정한다.**
	 * 둘은 서로를 모르므로 어긋나도 컴파일도 실행도 실패하지 않고 조용히 틀린다 —
	 * `PureEntity`인데 매핑에 Actor가 남아 있으면 가까이 간 것만으로 승격되고,
	 * `ActorPromoted`인데 매핑에 Actor가 없으면 전투에 들어가도 영영 승격되지 않는다.
	 * 실제로 전자를 밟았기 때문에 경고로 잡는다.
	 */
	virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World,
		FAdditionalTraitRequirements& OutTraitRequirements) const override;

	/** Enemy MassReplication(Phase 6) — BubbleInfoClass/ReplicatorClass를 LNP 전용 클래스로 고정해 내부적으로 위임한다.
	 *  Standalone(NM_Standalone)에서는 UMassReplicationTrait::BuildTemplate 자체가 조기 반환하므로 별도 분기가 필요 없다. */
	UPROPERTY(VisibleAnywhere, Category = "LNP|Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMassReplicationTrait> ReplicationTrait;
};
