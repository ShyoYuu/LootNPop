// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Item/LNPItemDefinitionBase.h"
#include "GameplayTagContainer.h"

#include "HitDetection/LNPProjectileMassTypes.h"
#include "Item/LNPWeaponLevelRow.h"
#include "LNPWeaponData.generated.h"

class ULNPVFXData;
class UDataTable;
class ULNPWeaponVisualSet;

/**
 * 무기 DataAsset.
 *
 * 무기의 공격력은 여기가 아니라 `LevelTable`의 레벨 행이 선언한다 — 어트리뷰트에 합산되어야
 * 곱연산 버프가 무기분까지 증폭하고 스탯 UI에도 함께 표시된다.
 * 테이블을 지정하지 않은 무기는 베이스의 `StatModifiers`를 레벨 1 값으로 쓰며 합성 대상이 아니다.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPWeaponData : public ULNPItemDefinitionBase
{
	GENERATED_BODY()
public:
	// --- 레벨 (LNPWeaponLevelRow.h) ---

	/**
	 * 레벨별 스텟·어빌리티 계수 테이블. **행 이름 = 레벨 숫자**("1"~"N"), 마지막 행이 최대 레벨이다.
	 * 지정하면 베이스의 StatModifiers는 무시된다 (둘 다 채우면 경고 로그).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Level",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/LootNPop.LNPWeaponLevelRow"))
	TObjectPtr<UDataTable> LevelTable;

	/** 해당 레벨의 행을 조회한다. 테이블이 없거나 행이 없으면 nullptr. */
	const FLNPWeaponLevelRow* FindLevelRow(int32 Level) const;

	/** 이 무기가 도달 가능한 최대 레벨 ("1"부터 연속으로 존재하는 행 수). 테이블이 없으면 1. */
	int32 GetMaxLevel() const;

	/** 해당 레벨의 스탯 목록. 테이블이 없으면 베이스 StatModifiers로 폴백한다 (레거시·적 NPC 무기). */
	TConstArrayView<FLNPStatModifier> GetStatModifiersForLevel(int32 Level) const;

	/** 해당 레벨의 어빌리티 피해 계수 배율. 테이블이 없으면 1.0. */
	float GetAbilityCoefScale(int32 Level) const;

	/**
	 * 이 무기의 표현 세트 — 메시·그립 보정·애님 레이어·몽타주 키. 여러 무기가 공유할 수 있다
	 * (런처는 `VS_Shotgun`을 가리킨다). 비어 있으면 맨손 표현으로 떨어진다.
	 *
	 * ⚠️ 규칙이 무기를 갈라야 할 때 이 에셋을 키로 쓰면 안 된다 — 공유하는 무기들이 함께 묶인다.
	 * 무기의 정체성은 이 DataAsset 그 자체다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<ULNPWeaponVisualSet> VisualSet;

	/**
	 * 장착 직후 부여되는 기본 조준 모드 태그.
	 * - 원거리 무기: LNP.AimMode.FreeAim
	 * - 근거리·맨손: 비워두면 LNP.AimMode.None으로 처리
	 * LockOn 전환은 DefaultAimMode가 None일 때만 허용 (코드 하드코딩).
	 *
	 * ⚠️ VisualSet이 아니라 여기 있다 — 표현이 아니라 조작 규칙이고, 같은 메시를 든 두 무기가
	 * 서로 다른 조준 모드를 가질 수 있어야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (Categories = "LNP.AimMode"))
	FGameplayTag DefaultAimMode;

	/** 기본 공격 연사 쿨타임 (초). 어빌리티가 Cooldown GE의 Duration으로 주입한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	float FireCooldown = 0.2f;

	/**
	 * 탄창 크기 — 이만큼 쏘면 재장전해야 이어서 쏠 수 있다. **0이면 탄약 개념이 없다**(근접·적 NPC 무기).
	 * 발사 1회가 1발이다 (산탄도 마찬가지).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	int32 MagazineSize = 0;

	/** 재장전 시간 (초). AttackSpeed로 나눈 값이 실제 시간이다 — GetReloadDuration(). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.05", EditCondition = "MagazineSize > 0"))
	float ReloadTime = 1.5f;

	/** AttackSpeed를 반영한 실제 재장전 시간. 재장전 어빌리티와 무기 애니 큐가 같은 값을 봐야 박자가 맞는다. */
	float GetReloadDuration(float AttackSpeed) const
	{
		return ReloadTime / FMath::Max(0.01f, AttackSpeed);
	}

	/** 최대 콤보 연결 횟수. 콤보 인덱스는 이 값을 초과하면 처음(Attack_1)으로 순환한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combo", meta = (ClampMin = "1"))
	int32 MaxComboCount = 5;

	/**
	 * 근접 공격 보정이 목표로 삼는 "무기가 잘 닿는 거리" (cm). 접평면 기준이다.
	 * 타겟이 이 거리 안에 있으면 위치 보정량은 0이 된다. 0이면 이 무기는 위치 보정을 쓰지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Melee", meta = (ClampMin = "0", Units = "cm"))
	float MeleeIdealDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile")
	ELNPProjectileType ProjectileType = ELNPProjectileType::Linear;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "1"))
	float ProjectileSpeed = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "0.1"))
	float HitRadius = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "0.1"))
	float ExplosionRadius = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "0.1"))
	float ProjectileLifetime = 5.f;

	/**
	 * 발사체에 걸리는 구면 중력 가속도 (cm/s²). **ProjectileType이 Lobbed일 때만** 적용된다.
	 * 기본값은 폰 중력(ULNPPawnGravityComponent::GravityStrength)과 같은 값이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "0"))
	float ProjectileGravity = 2000.f;

	/** 실제 비행에 적용되는 중력 가속도. Lobbed가 아니면 0이고, 0이면 등속 직선과 정확히 같다.
	 *  스폰 경로와 ADS 궤도 가이드가 모두 이 창구를 봐야 예측과 실탄이 갈리지 않는다. */
	float GetEffectiveProjectileGravity() const
	{
		return (ProjectileType == ELNPProjectileType::Lobbed) ? ProjectileGravity : 0.f;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile")
	FVector MuzzleOffset = FVector(100.f, 0.f, 0.f);

	/** Actor(GAS) 상태에서 피격 대상에 적용되는 GE. TAG_GE_Data_Damage를 SetByCaller로 사용해야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile")
	TSubclassOf<UGameplayEffect> ProjectileDamageEffect;

	/** 스폰 시, 비행 중(트레일), 임팩트 시 재생되는 VFX. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|VFX")
	TObjectPtr<ULNPVFXData> ProjectileVFXData;

private:
	/** 레벨 테이블 저작 오류를 최초 사용 시 1회만 로그로 알린다 (PostLoad는 테이블 로드 순서에 의존). */
	void ValidateLevelTable() const;

	mutable bool bValidatedLevelTable = false;
};
