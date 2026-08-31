// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LNPSettings.generated.h"

class ULNPOctantPoolData;
class ULNPMassSpawnConfig;
class ALNPLootDice;
class ULNPLootDiceRewardTable;
class UNiagaraSystem;

/**
 * LootNPop 전역 프로젝트 설정.
 * Project Settings -> Game -> LNP Settings에서 편집할 수 있다.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="LNP Settings"))
class LOOTNPOP_API ULNPSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULNPSettings();

	UPROPERTY(Config, EditAnywhere, Category = "World Generation")
	float SphereRadius = 25000.0f;

	/** World 생성에 사용할 기본 Octant Pool. */
	UPROPERTY(Config, EditAnywhere, Category = "World Generation")
	TSoftObjectPtr<ULNPOctantPoolData> OctantPool;

	/** 초기 Mass Entity 스폰을 위한 기본 config. */
	UPROPERTY(Config, EditAnywhere, Category = "Mass Spawning")
	TSoftObjectPtr<ULNPMassSpawnConfig> MassSpawnConfig;

	/** Player MassReplication(Phase 6.5): 클라이언트 Player 엔티티 템플릿 warm-up용 폰 클래스.
	 *  Bubble 복제 스폰은 TemplateID로 템플릿을 조회하는데, 그 ID는 폰의 MassAgentComponent EntityConfig에서
	 *  파생되므로 접속 전에 CDO 기준으로 동일 템플릿을 클라이언트 TemplateRegistry에 등록해 둬야 한다. */
	UPROPERTY(Config, EditAnywhere, Category = "Mass Spawning")
	TSoftClassPtr<APawn> PlayerPawnClass;

	/** 적도에서 인접 Cache 셀 간의 목표 호 길이 거리 (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Surface Cache", meta=(ClampMin="1.0", Units="cm"))
	float SurfaceCacheCellSpacing = 200.0f;

	/**
	 * 베이킹 중 한 프레임에 발사할 표면 트레이스 수.
	 *
	 * 전량을 한 프레임에 발사하면 다음 프레임 UWorld::ResetAsyncTrace가 WaitForAllAsyncTraceTasks로
	 * 게임 Thread를 막고 전체 Callback을 한 번에 쏟아내 큰 히치가 발생한다. 나눠 쏘면 부하가 분산되고
	 * GetBakingProgress()도 실제로 차오르는 값이 된다.
	 * 기본값 기준 308,505 샘플 / 2,000 = 약 155 프레임 (60fps에서 약 2.6초).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Surface Cache", meta=(ClampMin="1"))
	int32 SurfaceCacheSamplesPerFrame = 2000;

	/** Player 캐릭터당 키 매핑된 Active Skill Slots의 최대 수. */
	UPROPERTY(Config, EditAnywhere, Category = "Ability System", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxActiveSkillSlots = 4;

	/**
	 * 다음 레벨 무기 1개를 만드는 데 필요한 같은 종류·같은 레벨 무기 개수 (n).
	 * 장착 중인 무기를 올릴 때는 장착본 자신이 1개로 세어져 재료는 n-1개만 소모된다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Inventory", meta = (ClampMin = "2"))
	int32 WeaponMergeMaterialCount = 3;

	/** true이면 Player Projectile이 다른 Player에게 피해를 줄 수 있다. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	bool bFriendlyFire = false;

	/**
	 * Player 진영 Projectile 트레일 색. Niagara User 파라미터 `TintColor`로 주입된다.
	 *
	 * ⚠️ M_LNP_ProjectileGlow는 Unlit이라 이 값이 곧 Emissive다 — 세 채널이 모두 1을 넘으면
	 *    톤매핑에서 흰색으로 포화돼 진영 구분이 사라진다. 색조를 담당하는 채널 하나만
	 *    1을 살짝 넘기고(발광감) 나머지는 1 이하로 둘 것.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|VFX")
	FLinearColor PlayerProjectileTintColor = FLinearColor(0.06f, 0.35f, 1.50f, 1.0f);

	/** Enemy 진영 Projectile 트레일 색. 주의사항은 PlayerProjectileTintColor와 동일. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|VFX")
	FLinearColor EnemyProjectileTintColor = FLinearColor(1.50f, 0.14f, 0.05f, 1.0f);

	/** 플레이어 사망 후 랙돌을 유지하다가 리스폰하기까지의 시간 (초). */
	UPROPERTY(Config, EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", Units = "s"))
	float PlayerRespawnDelay = 10.0f;

	/** Enemy 사망 후 시체(랙돌)가 남아 있는 시간 (초). 만료 시 Mass 엔티티가 파괴된다. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", Units = "s"))
	float EnemyRagdollDuration = 5.0f;

	/**
	 * 경직도 자연 감쇠 속도 (초당). 밸런스상 필요해지기 전까지는 스텟이 아니라 전역 상수로 둔다.
	 * 경직력은 경직저항력으로 나뉘어 들어오므로 임계값·감쇠는 모든 폰이 같은 눈금을 쓴다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Poise", meta = (ClampMin = "0.0"))
	float PoiseDecayPerSecond = 30.0f;

	/** 마지막 피격 후 감쇠가 다시 시작되기까지의 유예 (초). 짧은 시간 안에 몰아친 연타가 실제로 쌓이게 한다. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Poise", meta = (ClampMin = "0.0", Units = "s"))
	float PoiseDecayDelaySeconds = 0.6f;

	/**
	 * T1 — 이 값 이상이면 그로기(공격·이동 불가). **플레이어 기본값**이며,
	 * 적은 ULNPEnemyConfig가 자기 값을 따로 정한다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Poise", meta = (ClampMin = "1.0"))
	float PoiseStaggerThreshold = 60.0f;

	/**
	 * T2 — 도달하면 다운(게이지 리셋 + 면역). **플레이어 기본값**이며 T1보다 커야 한다.
	 *
	 * T1~T2 간격이 곧 상대에게 내주는 딜 구간이다. 플레이어 쪽은 무력한 시간이므로 **좁게** 잡고,
	 * 적 쪽(ULNPEnemyConfig, 기본 60~200)은 넓게 잡아 실컷 두들길 수 있게 한다.
	 *
	 * ⚠️ 이 값을 적과 같게(또는 더 넓게) 두면 의도가 **정반대로 뒤집힌다**. 밴드 통과 타수는
	 * `(T2 - T1) / (경직력 x 저항계수)`인데 저항이 높을수록 분모가 작아지기 때문이다 —
	 * 저항 150인 플레이어는 같은 밴드를 저항 20인 적보다 훨씬 느리게 지나간다.
	 * 기본값 60~95(밴드 35)는 그 점을 반영한 초안이다. 상세는 TechDesign_Poise.md 3.3.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Poise", meta = (ClampMin = "1.0"))
	float PoiseDownThreshold = 95.0f;

	/** 가드로 막아낸 공격이 경직도에 기여하는 비율. 1.0이면 가드해도 경직력을 그대로 받는다 (가드 브레이크). */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Poise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PoiseGuardMultiplier = 0.5f;

	/**
	 * 다운이 지속되는 시간 (초). GA_Stagger의 어빌리티 수명이다.
	 * ⚠️ 몽타주 길이와는 독립이다 — 몽타주는 GameplayCue가 코스메틱으로 재생하고, 행동 잠금은 이 값이 정한다.
	 * (그로기에는 지속 시간이 없다 — 경직도가 T1 아래로 회복할 때까지 이어진다.)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Poise", meta = (ClampMin = "0.05", Units = "s"))
	float PoiseDownLockSeconds = 1.8f;

	/**
	 * 다운 직후 경직 면역 시간 (초). **자연회복 외에 경직도를 끊는 유일한 장치**이자
	 * 스턴락 방지의 전부다. 다운 지속 시간보다 길게 잡아 일어선 직후 잠깐의 유예를 준다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Poise", meta = (ClampMin = "0.0", Units = "s"))
	float PoiseDownImmunitySeconds = 2.5f;

	/**
	 * 그로기가 1초 이어질 때마다 경직력 유입에 더해지는 보너스 비율.
	 * 유입 = 기본 × (1 + 이 값 × 그로기 경과 시간).
	 *
	 * 게이지를 T1 바로 위에 걸쳐 두는 화력이면 T2에 영영 닿지 않아 무력 상태가 무한정 유지되는데,
	 * 유입이 시간에 비례해 무한히 커지고 자연회복은 상수이므로 **계속 때리는 한 T2 도달이 보장된다**.
	 *
	 * 시계로 강제 다운시키는 타임아웃 대신 이 방식을 쓰는 이유: 타임아웃은 아무도 때리지 않는 순간에
	 * 픽 쓰러질 수 있어 인과가 화면에 안 보인다. 보너스는 **다운이 항상 타격 위에서** 일어난다.
	 * 몰아치는 정상 전투에서는 보너스가 붙기 전에 이미 T2에 닿으므로 거의 개입하지 않는다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Poise", meta = (ClampMin = "0.0"))
	float PoiseGroggyBonusPerSecond = 0.75f;

	/**
	 * 근접 패링 성공 시 공격자에게 쏟아붓는 경직도 — **피격자 자신의 T1에 대한 배율**이다.
	 * 1.0이면 딱 그로기 진입, 그 이상이면 초과분이 남아 그로기가 그만큼 오래 간다.
	 *
	 * 절대값이 아니라 배율인 이유: 임계값이 폰별이라 절대값으로 두면 폰마다 효과가 제각각이 된다.
	 * 경직저항력으로 나누지도 않는다 — 패링은 스텟 대결이 아니라 타이밍 판정이다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Poise", meta = (ClampMin = "0.0"))
	float PoiseParryBreakRatio = 1.75f;

	/**
	 * 근접 공격 보정 강도 (0~1). 0이면 보정을 끈다.
	 *
	 * "타겟까지의 초과 거리" 중 몇 할을 메울지를 뜻한다 — 이미 이상 거리 안이면 보정량은 0이고,
	 * 멀수록 커지되 MeleeAssistMaxCorrectionDistance에서 잘린다.
	 * 사용자 환경설정 UI가 생기면 ULNPSettings 대신 그쪽을 출처로 바꿀 지점이다
	 * (읽기는 LNPMeleeAssist::GetStrength() 한 곳으로 모아 뒀다).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Melee Assist", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MeleeAssistStrength = 0.5f;

	/** 한 번의 공격에서 보정으로 이동할 수 있는 최대 거리 (cm). 보정이 커져 어색해지는 것을 막는 상한이다. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Melee Assist", meta = (ClampMin = "0.0", Units = "cm"))
	float MeleeAssistMaxCorrectionDistance = 150.0f;

	/** 락온이 꺼져 있을 때 타겟을 찾는 반경 (cm). 무기 공격 범위보다 살짝 넓게 잡는다. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Melee Assist", meta = (ClampMin = "0.0", Units = "cm"))
	float MeleeAssistSearchRadius = 300.0f;

	/** 락온이 꺼져 있을 때 타겟으로 인정할 캐릭터 전방 기준 최대 각도 (도). 접평면에 투영해서 잰다. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Melee Assist", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float MeleeAssistMaxSearchAngleDeg = 75.0f;

	/** 타겟 점수의 각도 가중치. 거리 가중치보다 크면 "정면 우선", 작으면 "근접 우선"이 된다. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Melee Assist", meta = (ClampMin = "0.0"))
	float MeleeAssistAngleWeight = 0.6f;

	/** 타겟 점수의 거리 가중치. MeleeAssistAngleWeight와 짝을 이룬다. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Melee Assist", meta = (ClampMin = "0.0"))
	float MeleeAssistDistanceWeight = 0.4f;

	/**
	 * 보정 이동 속력 상한 — 애니메이션 원본 루트모션 속도 대비 배율.
	 * URootMotionModifier_SkewWarp::MaxSpeedClampRatio로 그대로 전달된다. 0이면 무제한.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Melee Assist", meta = (ClampMin = "0.0"))
	float MeleeAssistMaxSpeedClampRatio = 2.0f;

	/**
	 * 보정 구간(워프 창)의 길이 (초). 몽타주 섹션 안에서 **루트모션 이동량이 가장 큰** 이 길이의 구간을 찾아 쓴다.
	 * 검 공격은 선딜에 자세만 잡고 휘두르는 순간 밀고 나가므로 결과적으로 스윙 구간이 뽑힌다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Melee Assist", meta = (ClampMin = "0.02", Units = "s"))
	float MeleeAssistWarpWindowSeconds = 0.2f;

	/**
	 * 위치 보정 경로를 가르는 기준 (cm). 워프 구간의 루트모션 이동량이 이 값 이상이면 Motion Warping,
	 * 미만이면 LayeredMove로 동작한다. "어느 쪽이 나은가"가 아니라 **워프할 원본이 있는가**가 기준이다.
	 *
	 * 루트모션은 애니메이터가 의도한 캐릭터 이동 그 자체다. 이동량이 실려 있으면 그것을 스케일하는
	 * Motion Warping이 가장 자연스럽고, in-place 애니메이션(= 움직이지 않는 것이 자연스럽다는 의도)에는
	 * 스케일할 원본이 없어 코드가 만든 속도를 얹는 LayeredMove로 간다.
	 *
	 * ⚠️ 낮추면 안 된다. 스큐 워프는 창에 이동량이 없으면 엔진 내부에서 전혀 다른 경로(타겟까지 Lerp)로
	 * 빠지는데, 그 경로에는 MaxSpeedClampRatio 클램프가 **없어서** 장애물에 막히면 한 프레임에 보정 거리
	 * 전체를 속도로 환산해 캐릭터를 날려 버린다 (2026-08-30 실측 — Falling 전환 후 멀리 튕겨나감).
	 *
	 * Motion Warping 쪽 보정 상한이기도 하다 — 클램프가 "원본 속도 x MaxSpeedClampRatio"이므로
	 * 실제 보정량은 대략 `창 이동량 x MeleeAssistMaxSpeedClampRatio`를 넘지 못한다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Melee Assist", meta = (ClampMin = "1.0", Units = "cm"))
	float MeleeAssistMinWindowRootMotion = 10.0f;

	/**
	 * LayeredMove 경로의 보정 속력 상한 (cm/s). 요구 속도가 이를 넘으면 그만큼 덜 당긴다.
	 * 이동 모드가 최고 속도로 클램프하므로 캐릭터 MaxSpeed보다 크게 잡아도 의미가 없다.
	 * (Motion Warping 경로는 MeleeAssistMaxSpeedClampRatio가 대신 맡는다 — 그쪽은 애니메이션 속도 대비 배율이다.)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Melee Assist", meta = (ClampMin = "1.0"))
	float MeleeAssistMaxPullSpeed = 400.0f;

	/** LootDice 스폰 클래스 (BP_LootDice — 큐브 메시·아이콘 머티리얼 지정). 미설정 시 C++ 기본 클래스 폴백. */
	UPROPERTY(Config, EditAnywhere, Category = "Loot Dice")
	TSoftClassPtr<ALNPLootDice> LootDiceClass;

	/** LootPod Popped 시 PodID로 보상을 조회하는 테이블. 미설정 시 보상 스폰이 생략된다. */
	UPROPERTY(Config, EditAnywhere, Category = "Loot Dice")
	TSoftObjectPtr<ULNPLootDiceRewardTable> LootDiceRewardTable;

	/** LootPod Popped 축하 이펙트(Confetti). 보상과 동일하게 위치 기반·Actor 독립으로 스폰된다.
	 *  미설정 시 스폰 생략. (서버 스폰이라 원격 클라 복제는 별도 — MP 후순위) */
	UPROPERTY(Config, EditAnywhere, Category = "Loot Pod")
	TSoftObjectPtr<UNiagaraSystem> LootPodConfettiVFX;

	/** Debug Draw 거리 컬링 기준값 (DistSq). 플레이어와의 거리² 가 이 값 미만인 엔티티만 그린다. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Draw", meta = (ClampMin="0.0"))
	float DebugDrawProjectileDistSq = 100000000.f;

	/** Debug Draw 거리 컬링 기준값 (DistSq). 플레이어와의 거리² 가 이 값 미만인 엔티티만 그린다. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Draw", meta = (ClampMin = "0.0"))
	float DebugDrawProximityDistSq = 250000.f;
};
