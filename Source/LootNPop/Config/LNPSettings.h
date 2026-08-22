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
