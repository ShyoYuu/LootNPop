// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "Item/LNPWeaponData.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "LNPEnemyConfig.generated.h"

class ALNPEnemyCharacter;
class UStateTree;

/**
 * 전투 진입 시 이 적을 Actor로 승격할 것인가.
 *
 * ⚠️ **코드·데이터 애셋에 엘리트/잡몹/티어 같은 등급 어휘를 넣지 않는다.**
 * 시스템이 아는 것은 이 한 축뿐이고, 등급은 기획 문서와 밸런싱에서만 쓴다.
 * 등급은 늘어나도 이 축은 영원히 두 값이며, "엘리트인데 PureEntity"(대규모 정예 웨이브)나
 * "잡몹인데 ActorPromoted"(튜토리얼 1마리) 같은 조합이 기획상 충분히 성립한다 —
 * 이름이 모드를 단정하는 순간 코드가 거짓말을 시작한다.
 *
 * `bCanPromoteToActor` + `bHasEntityAttack` 같은 bool 두 개로 쪼개지 않는 이유도 같다.
 * bool 조합은 정의되지 않은 상태(둘 다 false / 둘 다 true)를 만들고 그 동작을 문서로 방어해야 한다.
 */
UENUM(BlueprintType)
enum class ELNPEnemyCombatMode : uint8
{
	/** 전투 진입 시 High LOD Actor로 승격한다 — GAS·몽타주·랙돌. */
	ActorPromoted,

	/** 승격하지 않는다 — ULNPEntityAttackProcessor가 공격 위상을 직접 구동한다. */
	PureEntity,
};

/**
 * 이 적의 기본 공격 방식.
 *
 * ⚠️ **근접/원거리 판별의 단일 원본이다.** 예전에는 `EnemyTypeTag`에 "Melee"가 들어 있는지로
 * 판정했는데, 그 규약은 태그 이름과 거동이 조용히 어긋날 수 있고 소비처가 늘수록 위험해진다.
 */
UENUM(BlueprintType)
enum class ELNPEnemyAttackType : uint8
{
	Melee,
	Ranged,
};

/**
 * 타게팅 슬롯 풀 — 어떤 적이 어떤 예산을 놓고 경쟁하는가.
 *
 * ⚠️ **`ActorPromoted`는 근접/원거리를 나누지 않는다.** 이 풀을 가르는 실제 비용 축은
 * 교전 거리가 아니라 **Actor 스폰 수**이기 때문이다 (승격된 적 1기당 700~900 B/s).
 *
 * 풀을 나누는 이유는 *"잡몹에 둘러싸여 슬롯이 찬 탓에 승격 개체가 구경만 하는"* 그림을
 * 원천 차단하기 위해서다. 같은 풀에 점수 가산으로 처리하면 가산치가 크면 잡몹이 통째로 밀려나고
 * 작으면 거리로 다시 뒤집힌다 — 튜닝 축만 하나 늘어난다.
 */
UENUM()
enum class ELNPTargetSlotPool : uint8
{
	Melee,      // PureEntity + 근접
	Ranged,     // PureEntity + 원거리
	Promoted,   // ActorPromoted (근접·원거리 구분 없음)

	Count UMETA(Hidden)
};

/**
 * 우선순위 점수 계산 및 인지 설정.
 *
 * ```
 *   None(Idle) ──[발견]──▶ Alert ──[슬롯 획득]──▶ Confirmed(추격·공격)
 *                            ▲                        │
 *                            └──[플레이어가 세력권 밖]─┘
 *   Alert ──[타겟이 AlertRetentionDistance 밖]────────▶ None
 *   Alert ──[추격 못 한 채 AlertPatienceTime 경과]────▶ None (+ AlertRecoveryTime 동안 재발견 금지)
 * ```
 *
 * **추격 자격은 "플레이어가 Pod 세력권 안에 있는가"로만 판정한다** — NPC 자신의 위치는 보지 않는다.
 * NPC 위치를 자격 조건에 넣으면 NPC가 움직일 때마다 자기 조건이 뒤집혀 경계선에서 자기진동한다
 * (실측: "부들부들 떨며 안절부절"). 히스테리시스를 걸어도 진동 주기가 늘어날 뿐 사라지지 않는다 —
 * 기준점을 제어 주체 밖으로 옮기는 것이 유일한 해법이다.
 *
 * 필수 대소 관계: `AwarenessDistance` < `VisionDistance` < `AlertRetentionDistance` <= `ChaseRadius`
 * `AlertRetentionDistance`가 `VisionDistance`보다 커야 발견↔망각이 경계선에서 깜빡이지 않는다.
 */
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

	/**
	 * **세력권 반경** — Pod에서 **플레이어**까지의 거리가 이 안일 때만 추격한다(슬롯 경쟁 참가).
	 * 밖으로 나가면 귀속 적 전원이 동시에 Alert로 강등된다. 어그로가 풀리는 것은 아니다.
	 *
	 * 재는 대상이 NPC가 아니라 **플레이어**인 것이 핵심이다 — NPC가 어떻게 움직이든 자격이
	 * 바뀌지 않으므로 경계선 진동이 원천적으로 불가능하고, 복귀 래치 같은 히스테리시스 장치가
	 * 필요 없다. 플레이어 입장에서는 "Pod 세력권을 벗어나면 추격이 끊긴다"로 읽힌다.
	 *
	 * 단, `AwarenessDistance` 안까지 들어온 상대에게는 세력권과 무관하게 반격한다 —
	 * 그러지 않으면 세력권 밖의 적이 눈앞의 플레이어를 멀뚱히 보고만 있게 된다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Scoring")
	float ChaseRadius = 5000.0f;

	/** **발견 거리** — 새로운 Player를 인지하는 거리 (VisionAngle 시야각 안일 때만) */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception")
	float VisionDistance = 2000.0f;

	/** 전체 시야각 (도) */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception")
	float VisionAngle = 90.0f;

	/** **초근접 발견 거리** — FOV·재발견 금지와 무관하게 항상 Player를 감지하는 거리 */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception")
	float AwarenessDistance = 200.0f;

	/**
	 * **추적 유지 거리** — 이미 추적 중인 타겟 **한 명**을 후보로 유지하는 상한.
	 * 이 판정은 FOV를 보지 않으므로 등 뒤로 돌아도 놓치지 않는다.
	 *
	 * `VisionDistance`보다 커야 한다. 같으면 그 거리에서 발견과 망각이 매 프레임 뒤집힌다.
	 * 대신 새로운 대상의 인지 상한은 언제나 `VisionDistance`/`AwarenessDistance`이므로,
	 * "NPC는 자기 주변만 본다"는 규약은 깨지지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception")
	float AlertRetentionDistance = 2500.0f;

	/**
	 * **경계 인내 시간(초)** — 추격 자격도 없이 경계만 이만큼 지속하면 어그로를 포기하고
	 * Idle로 내려간다. 거리만으로는 사다리가 닫히지 않기 때문에 필요하다 — 플레이어가
	 * 세력권 바로 바깥에 서 있으면 NPC는 싸우지도(자격 없음) 잊지도(시야 안) 못한 채 굳는다.
	 *
	 * **슬롯 대기 중인 개체에는 적용되지 않는다.** 추격 자격은 있는데 슬롯만 못 얻은 상태는
	 * 전투 대기열이므로, 시간이 지난다고 흩어지면 큰 무리와의 교전이 말라 버린다.
	 * `Confirmed`·`None`일 때도 0으로 초기화되므로 "교전에 성공하면 인내는 새로 시작"이
	 * 공격을 특수 처리하지 않고도 자동으로 성립한다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception", meta = (ClampMin = "0.0"))
	float AlertPatienceTime = 8.0f;

	/**
	 * **재발견 금지 시간(초)** — 인내를 소진해 포기한 직후, 이 시간 동안 시야 발견을 막는다.
	 *
	 * 없으면 인내가 무의미해진다. 포기한 그 프레임에도 플레이어는 여전히 정면 시야 안에 있으므로
	 * **다음 프레임에 곧바로 재발견되어** Idle로 내려가자마자 Alert로 되돌아오고, NPC는 Pod 쪽으로
	 * 한 발짝도 못 걷는다. 이 시간이 벌어 주는 것은 **등을 돌릴 시간**이다 — 돌아서기만 하면
	 * 플레이어가 시야각 밖으로 빠져 상황이 스스로 해소된다.
	 *
	 * 기본값 1.0초는 Low LOD 등속 회전(`RotationRate` 360°/s로 180° = 0.5초)의 2배 버퍼다.
	 * 실제로 필요한 회전은 시야각 절반(45° = 0.125초)뿐이라 여유가 충분하다.
	 * `AwarenessDistance` 안까지 들어온 상대는 이 금지를 무시하고 즉시 발견한다 — 회복 중이라고
	 * 눈앞의 플레이어를 못 보는 장님이 되지는 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception", meta = (ClampMin = "0.0"))
	float AlertRecoveryTime = 1.0f;

	/**
	 * **피격 주시 시간(초)** — 배회(`None`) 중에 피격당하면 이만큼 그 자리에 서서 피격 방향을
	 * 바라본다. 돌아본 결과 시야 안에 플레이어가 있으면 평소의 발견 → 경계 → 슬롯 경쟁 플로우를
	 * 그대로 타고, 없으면 배회로 복귀한다 — 별도의 예외 규칙이 필요 없다.
	 *
	 * **추격 자격은 주지 않는다.** 주면 세력권 밖에서 원거리로 찔러 NPC를 무한정 끌고 다닐 수 있다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Perception", meta = (ClampMin = "0.0"))
	float HitReactLookTime = 2.5f;
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
	 * **상하 조준 가용 각도**(도, 캐릭터 로컬 좌표계). **기준면은 캐릭터의 로컬 수평면**이고 양수가 위쪽이다.
	 * 즉 ∓75°는 "수평에서 위로 75°, 아래로 75°"이며, 못 겨누는 영역은 로컬 Up/Down 기준 15° 원뿔 안쪽뿐이다.
	 *
	 * 상한은 Aim Offset 에셋이 아니라 **게임플레이 판단**으로 정한다 —
	 * 플레이어 기준 실측상 AO 자세 자체는 거의 수직까지 무리 없이 나온다.
	 *
	 * **소비처가 셋이고 반드시 같은 값을 봐야 한다.**
	 * ① 조준 자세(`ALNPEnemyCharacter::SetAimTargetLocation`의 클램프)
	 * ② 발사 방향(같은 값에서 파생 — 겨눈 곳과 맞는 곳이 어긋나지 않게)
	 * ③ **피격 인지의 상하 게이트**(`ULNPEnemyScoringProcessor`) — 이 범위 밖에서 날아온 공격은
	 *    반격이 원천적으로 불가능하므로 아예 인지하지 않는다. 인지만 하면 겨눌 수 없는 각도를
	 *    향해 영원히 헛쏘는 상태가 된다.
	 *
	 * 즉 이 값을 좁히면 "못 겨누는 각도"와 "못 알아채는 각도"가 **함께** 움직인다. 그래야 모순이 없다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat", meta = (ClampMin = "-90.0", ClampMax = "0.0"))
	float AimPitchMinDeg = -75.f;

	UPROPERTY(EditAnywhere, Category = "LNP|Combat", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float AimPitchMaxDeg = 75.f;

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
 * 순수 엔티티(`ELNPEnemyCombatMode::PureEntity`) 기본 공격 설정.
 *
 * 무기 상수(발사체 속도·수명·폭발 반경·피해 GE)는 여기가 아니라 `ULNPEnemyConfig::WeaponData`에 있다.
 * 이 구조체가 정의하는 것은 지금까지 **어빌리티 인스턴스가 공급하던 값**뿐이다 —
 * Actor 경로에서 GAS가 채우던 자리를 그대로 대신한다.
 *
 * ⚠️ **공격 간격은 여기 두지 않는다.** `FLNPEnemyMovementConfig::AttackInterval`이 이미 그 의미이고,
 * 같은 뜻의 필드가 둘이 되면 어느 쪽이 사는지 데이터만 보고는 알 수 없다.
 */
USTRUCT(BlueprintType)
struct FLNPEntityAttackConfig
{
	GENERATED_BODY()

	// --- 공용 ---

	/** 원거리는 **펠릿 하나당** 값이다 — 산탄이면 명중 수만큼 곱해져 들어간다. */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack", meta = (ClampMin = "0.0"))
	float Damage = 10.f;

	/** 원거리는 **펠릿 하나당** 값이다. 경직은 명중마다 누적되므로 산탄에서는 특히 작게 잡는다. */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack", meta = (ClampMin = "0.0"))
	float PoiseDamage = 10.f;

	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 0.f;

	/** 패링 판정 반경. **무기 HitRadius보다 크게** 둔다 — 2단계 판정(패링 먼저)의 규약이다. */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack", meta = (ClampMin = "0.0"))
	float ParryRadius = 40.f;

	/** 선딜 — 플레이어가 읽고 반응할 구간. 원거리는 이 구간이 끝나는 순간 1회 발사한다. */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack", meta = (ClampMin = "0.0"))
	float WindupTime = 0.35f;

	/** 근접 = 칼날이 살아 있는 구간. 원거리에서는 쓰이지 않는다. */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack", meta = (ClampMin = "0.0"))
	float ActiveTime = 0.20f;

	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack", meta = (ClampMin = "0.0"))
	float RecoveryTime = 0.45f;

	/**
	 * 패링당한 뒤 자세가 무너진 채로 남아 있는 시간(초). 이 동안 행동 상태가 `Parried`로 나간다.
	 *
	 * ⚠️ **재생할 모션의 길이와 손으로 맞춘다.** 위상 시간과 달리 파생시킬 원본이 없다 —
	 * 서버는 어떤 시퀀스가 재생되는지 모르고(그건 표현의 몫), 시퀀스 길이를 서버 판정에 끌어들이면
	 * 애니가 게임플레이를 정하게 된다. 짧으면 모션이 잘리고, 길면 굳은 채로 서 있는다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack", meta = (ClampMin = "0.0"))
	float ParriedRecoveryTime = 1.05f;

	// --- 근접 전용: 가상 칼날 (캡슐 중심 기준 로컬 치수) ---

	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Melee")
	float PivotForward = 20.f;

	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Melee")
	float PivotUp = 30.f;

	/** 회전 원점~칼밑 거리. */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Melee", meta = (ClampMin = "0.0"))
	float BladeInner = 30.f;

	/**
	 * 회전 원점~칼끝 거리. ⚠️ **시각 무기 길이와 반드시 일치시킬 것** —
	 * 애니메이션에서 뽑을 수 없는 상수라 어긋나면 "칼이 안 닿았는데 맞는다"가 된다.
	 * `ULNPWeaponTraceDebugDrawProcessor`로 눈으로 맞춘다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Melee", meta = (ClampMin = "0.0"))
	float BladeOuter = 140.f;

	/** 스윙 시작 로컬 Yaw(도). */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Melee")
	float ArcStartDeg = -70.f;

	/** 스윙 종료 로컬 Yaw(도). */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Melee")
	float ArcEndDeg = 70.f;

	/**
	 * 스윙 **시작** 기울기(도). 접평면 기준이고 **양수가 위**(머리 쪽), 음수가 아래다.
	 *
	 * Yaw와 마찬가지로 Active 구간 동안 End까지 보간된다 — 둘을 같게 두면 예전처럼 일정 기울기로
	 * 수평 훑기가 되고, 벌리면 사선 베기가 된다. 애니가 수직에 가깝게 내려벤다면 Start를 크게
	 * 양수로, End를 크게 음수로 준다(예: +50 → -50).
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Melee")
	float ArcPitchStartDeg = -15.f;

	/** 스윙 **종료** 기울기(도). `ArcPitchStartDeg`와 같으면 기울기가 고정된다. */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Melee")
	float ArcPitchEndDeg = -15.f;

	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Melee", meta = (ClampMin = "0.1"))
	float HitRadius = 12.f;

	// --- 원거리 전용 ---

	/**
	 * 총구 위치(캡슐 중심 기준 로컬: X=전방, Y=우측, Z=Up).
	 * ⚠️ X는 **캡슐 반경보다 크게** 둘 것 — 캡슐 안에서 스폰하면 발사체가 자기 몸에 닿아 즉시 파괴된다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Ranged")
	FVector MuzzleLocalOffset = FVector(40.f, 0.f, 10.f);

	/**
	 * 조준점의 상하 보정(cm). 0이면 타겟의 **캡슐 중심**을 그대로 겨눈다.
	 *
	 * 캡슐 중심은 반높이(기본 96cm) 지점이라 서 있는 캐릭터에서는 **골반 높이**다.
	 * 그대로 겨누면 "하반신을 노리는" 그림이 되므로, 가슴께를 겨누고 싶으면 양수를 준다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Ranged")
	float AimTargetUpOffset = 0.f;

	/**
	 * 산탄 육각 링 수. 발사 수 = 1 + 3N(N+1) → 0=단발, 1=7발, 2=19발.
	 * 펠릿마다 Mass 엔티티와 트레일 VFX가 하나씩 생기므로 올릴 때 비용을 함께 본다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Ranged", meta = (ClampMin = "0", ClampMax = "5"))
	int32 HexRingCount = 0;

	/** 인접한 육각 셀 사이의 각도 간격(도). 링 수와 곱한 값이 확산의 최대 반각이 된다. */
	UPROPERTY(EditAnywhere, Category = "LNP|EntityAttack|Ranged", meta = (ClampMin = "0.0"))
	float HexStepDegrees = 5.f;
};

/**
 * 한 행동이 쓸 ISKM 시퀀스 인덱스 목록.
 *
 * 인덱스는 `UAnimSequenceTransformProviderData::Sequences`에 **구워진 배열 순서**다.
 * 적 타입마다 시퀀스 수와 순서가 다르므로 프로세서에 하드코딩하지 않고 데이터로 둔다.
 *
 * 항목이 둘 이상이면 `FLNPEnemyActionFragment::Seq`에서 유도해 번갈아 쓴다 — **연출 변화가 목적이고,
 * 재생 보장은 아니다.** 공격은 반드시 다른 상태를 경유해 재진입하므로(Move -> Attack) 변형이 하나여도
 * 인덱스는 어차피 바뀐다.
 *
 * ⚠️ 다만 엔진이 `SequenceIndex`가 **바뀔 때만** 트랙을 다시 앵커링한다는 사실은 알아 둘 것
 * (`MassVisualizationComponent.cpp`의 `GetSequenceIndex(...) != AnimData.SequenceIndex` 분기).
 * 같은 인덱스를 연속으로 실으면 재생이 처음부터 다시 돌지 않고 직전 재생을 이어간다 —
 * 중간 상태 없이 같은 행동을 반복하는 채널을 나중에 추가하면 여기서 걸린다.
 *
 * ⚠️ **추가(additive) 애니메이션은 넣지 말 것.** ASTP 컴파일러는 절대 포즈로 굽기 때문에
 * `AAT_LocalSpaceBase` 클립을 넣으면 뼈가 원점으로 모여 **메시가 통째로 사라진다**
 * (2026-09-06 실측 — Lyra의 `MM_HitReact_*`가 전부 추가 클립이다).
 */
USTRUCT(BlueprintType)
struct FLNPEnemyActionSequences
{
	GENERATED_BODY()

	/** Provider 배열 순서 기준 인덱스. 비어 있으면 그 행동은 직전 시퀀스를 그대로 유지한다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Visual", meta = (ClampMin = "0"))
	TArray<int32> Indices;
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

	/**
	 * 전투 진입 시 Actor로 승격할 것인가. **이 옵션 하나가 슬롯·비주얼·공격 경로를 전부 가른다** —
	 * "이 개체가 무엇을 못 하는가"를 다른 데이터를 보지 않고 답할 수 있어야 한다.
	 *
	 * `PureEntity`면 `ULNPEnemyLODOverrideProcessor`가 표현 LOD를 Actor 아닌 단계로 눌러
	 * 거리가 가까워져도 Actor가 스폰되지 않는다. EntityConfig의 표현 매핑은 그 모드에서 실제로 쓸
	 * 비주얼을 정의할 뿐이고, **단일 진실은 이 enum이다.**
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	ELNPEnemyCombatMode CombatMode = ELNPEnemyCombatMode::ActorPromoted;

	/** 근접/원거리 판별의 단일 원본. 슬롯 풀 분류와 순수 엔티티 공격 경로가 함께 읽는다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	ELNPEnemyAttackType AttackType = ELNPEnemyAttackType::Melee;

	/** CombatMode == PureEntity일 때 쓰는 기본 공격 설정. */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat")
	FLNPEntityAttackConfig EntityAttackConfig;

	/**
	 * `PureEntity`가 죽고 나서 엔티티가 소멸하기까지의 시간(초). 랙돌 대신 Death 시퀀스가 재생될 자리다
	 * (`ActorPromoted`는 `ULNPSettings::EnemyRagdollDuration`을 그대로 쓴다).
	 *
	 * ⚠️ **복제 LOD의 최장 갱신 주기(0.3초)보다 넉넉히 커야 한다.** 엔티티 파괴가 곧 버블 제거라,
	 * 이 시간이 짧으면 게스트가 `Dying`을 받기도 전에 적이 사라져 **소리 없이 소멸한다.**
	 * `Dying` 전이는 일회성이라 갱신 주기 게이트를 우회하지만, 그래도 패킷이 한 번은 나가야 한다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Combat", meta = (ClampMin = "0.5"))
	float PureEntityDeathDuration = 1.5f;

	/**
	 * 행동 상태 -> ISKM 시퀀스 인덱스 매핑. 비워 두면 이 적에게는 ISKM 애니가 붙지 않는다.
	 *
	 * 짝이 되는 시퀀스 배열은 EntityConfig의 시각화 트레이트
	 * (`SkinnedMeshInstanceDesc.Meshes[n].TransformProvider`)에 있다. 둘은 서로를 모르므로
	 * 인덱스가 어긋나도 **컴파일도 실행도 실패하지 않는다** — 엉뚱한 모션이 나올 뿐이다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Visual")
	TMap<ELNPEnemyAction, FLNPEnemyActionSequences> ActionSequences;

	/** 상태가 바뀔 때의 블렌드 시간(초). `FAnimSequenceTrackAutoPlayData::BlendTime`으로 그대로 들어간다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Visual", meta = (ClampMin = "0.0"))
	float AnimBlendTime = 0.15f;

	/** Mass에서 Actor로 전환 시 스폰할 Actor 클래스 (CombatMode == ActorPromoted일 때만 쓰인다) */
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

	/**
	 * 이 적 타입의 경직저항력. 엔티티 스폰 시 FLNPPoiseFragment::Resistance로 시드된다.
	 * ULNPBaseAttributeSet의 기초값(플레이어 기준)을 대신하는 값이며, 낮을수록 쉽게 굳는다.
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Attributes", meta = (ClampMin = "0.0"))
	float PoiseResistance = 20.f;

	/** T1 — 이 값 이상이면 그로기. 엔티티 스폰 시 FLNPPoiseFragment로 시드된다. */
	UPROPERTY(EditAnywhere, Category = "LNP|Attributes", meta = (ClampMin = "1.0"))
	float PoiseStaggerThreshold = 60.f;

	/**
	 * T2 — 도달하면 다운. T1과의 간격이 곧 **플레이어에게 내주는 딜 구간**이다.
	 * 적 쪽은 넉넉히 잡아 굳은 동안 실컷 두들길 수 있게 한다 (플레이어 쪽은 반대로 좁다).
	 */
	UPROPERTY(EditAnywhere, Category = "LNP|Attributes", meta = (ClampMin = "1.0"))
	float PoiseDownThreshold = 200.f;

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

	/**
	 * 이 적이 경쟁할 슬롯 풀. **파생값이므로 데이터로 따로 두지 않는다** —
	 * `CombatMode`와 `AttackType`이 이미 원본이고, 셋을 따로 편집하게 하면 어긋날 자리가 생긴다.
	 */
	ELNPTargetSlotPool GetSlotPool() const
	{
		if (CombatMode == ELNPEnemyCombatMode::ActorPromoted)
			return ELNPTargetSlotPool::Promoted;

		return (AttackType == ELNPEnemyAttackType::Melee)
			? ELNPTargetSlotPool::Melee
			: ELNPTargetSlotPool::Ranged;
	}

	/** Asset Manager가 ID로 이 에셋을 식별하기 위해 필요 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("EnemyConfig"), GetFName());
	}
};
