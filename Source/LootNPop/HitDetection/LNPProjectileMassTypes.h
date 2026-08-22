// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
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
	ELNPProjectileType Type = ELNPProjectileType::Linear;

	UPROPERTY()
	float Damage      = 10.0f;

	UPROPERTY()
	float HitRadius       = 5.0f;    // 피격 판정 반경

	UPROPERTY()
	float ParryRadius     = 6.0f;    // 패링 판정 반경 — HitRadius보다 크게 설정

	UPROPERTY()
	float ExplosionRadius = 0.f;     // 스플래시 데미지 반경 (0이면 미사용)

	UPROPERTY()
	float KnockbackStrength       = 0.f;  // 직격 넉백 강도

	UPROPERTY()
	float SplashKnockbackStrength = 0.f;  // 스플래시 넉백 강도
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

	/** 이 엔티티를 생성한 머신에서 공격자가 로컬 컨트롤 대상인지 여부 (IsLocallyControlled()).
	 *  true인 머신에서만 클라이언트 예측 HitDetection(HitStop + Ghost 소멸)을 수행한다. */
	bool bIsLocalInstigator = false;

	/** 공격자의 APlayerState::GetPlayerId(). Enemy AI 등 PlayerState가 없으면 INDEX_NONE.
	 *  Projectile.Impact GameplayCue 핸들러가 "이 클라이언트가 공격자 본인인지" 판정하는 데 사용한다. */
	int32 InstigatorPlayerID = INDEX_NONE;

	/** Ghost 대조 식별자 (FLNPGhostKey::KeyOrSalvo). 예측 발사는 FPredictionKey(<= 65535),
	 *  예측 키가 없는 발사(리슨 호스트·NPC·패링 반사)는 서버 발급 SalvoID(>= 65536).
	 *  InstigatorPlayerID·SpawnIndex와 조합해야 전역 고유가 된다. */
	int32 PredictionKeyID = 0;

	/** 한 번의 발사(산탄 등)에서 몇 번째로 생성된 Projectile인지. PredictionKeyID와 조합해 Ghost를 고유 식별한다. */
	uint8 SpawnIndex = 0;

	/** 서버 전용 — Lag Compensation 되감기 시간. 발사(또는 패링 반사) 시점의 공격자 RTT/2를 1회 캐싱해
	 *  비행 내내 재사용한다. 매 프레임 재계산 시 "대상이 이미 피했는데 과거 잔상을 쫓아가 맞는" 문제 방지 (섹션 5.0). */
	float CachedRewindSeconds = 0.f;
};

/** 이 Projectile에 Niagara 트레일 Component가 할당됐는지 추적한다. */
USTRUCT()
struct LOOTNPOP_API FLNPProjectileVisualFragment : public FMassFragment
{
	GENERATED_BODY()

	bool bInitialized = false;

	/** 트레일에 실제로 반영된 진영. FLNPProjectileFragment::InstigatorTeam과 어긋나면
	 *  패링으로 소유권이 넘어갔다는 뜻이므로 트레일 색을 다시 주입한다. */
	ELNPInstigatorTeam AppliedTeam = ELNPInstigatorTeam::Enemy;
};

/** StartPhysics 단계 종료 시 Projectile Destroy을 표시하는 Tag. */
USTRUCT()
struct LOOTNPOP_API FLNPProjectileDeadTag : public FMassTag
{
	GENERATED_BODY()
};
