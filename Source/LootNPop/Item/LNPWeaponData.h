// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"

class USkeletalMesh;
#include "HitDetection/LNPProjectileMassTypes.h"
#include "Item/LNPWeaponLevelRow.h"
#include "LNPWeaponData.generated.h"

class ULNPVFXData;
class UDataTable;

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

	/** 이 무기의 타입 태그 (LNP.Weapon.Pistol 등). EquipWeapon()이 ASC에 부여한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (Categories = "LNP.Weapon"))
	FGameplayTag WeaponTag;

	/**
	 * 장착 직후 부여되는 기본 조준 모드 태그.
	 * - 원거리 무기: LNP.AimMode.FreeAim
	 * - 근거리·맨손: 비워두면 LNP.AimMode.None으로 처리
	 * LockOn 전환은 DefaultAimMode가 None일 때만 허용 (코드 하드코딩).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (Categories = "LNP.AimMode"))
	FGameplayTag DefaultAimMode;

	/** 장착 시 LinkAnimClassLayers()에 전달할 서브 AnimBP 클래스. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<UAnimInstance> AnimLayerClass;

	/** 캐릭터 메시에 어태치할 무기 스켈레탈 메시. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Mesh")
	TObjectPtr<USkeletalMesh> WeaponMesh;

	/** 무기 메시를 어태치할 캐릭터 소켓 이름. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Mesh")
	FName AttachSocketName = NAME_None;

	/** 장착 시 무기 메시의 상대 위치 오프셋. 피벗이 그립 위치에서 벗어난 경우 미세 보정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Mesh")
	FVector WeaponMeshRelativeLocation = FVector::ZeroVector;

	/** 장착 시 무기 메시의 상대 회전 오프셋. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Mesh")
	FRotator WeaponMeshRelativeRotation = FRotator(0.0f, 90.0f, -4.0f);

	/** 기본 공격 연사 쿨타임 (초). 어빌리티가 Cooldown GE의 Duration으로 주입한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	float FireCooldown = 0.2f;

	/** 최대 콤보 연결 횟수. 콤보 인덱스는 이 값을 초과하면 처음(Attack_1)으로 순환한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combo", meta = (ClampMin = "1"))
	int32 MaxComboCount = 5;

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
