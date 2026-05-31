// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Item/LNPItemDefinitionBase.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"

class USkeletalMesh;
#include "HitDetection/LNPProjectileMassTypes.h"
#include "LNPWeaponData.generated.h"

class ULNPVFXData;

UCLASS(BlueprintType)
class LOOTNPOP_API ULNPWeaponData : public ULNPItemDefinitionBase
{
	GENERATED_BODY()
public:
	/** 이 무기의 타입 태그 (LNP.Weapon.Pistol 등). EquipWeapon()이 ASC에 부여한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon",
		meta = (Categories = "LNP.Weapon"))
	FGameplayTag WeaponTag;

	/**
	 * 장착 직후 부여되는 기본 조준 모드 태그.
	 * - 원거리 무기: LNP.AimMode.FreeAim
	 * - 근거리·맨손: 비워두면 LNP.AimMode.None으로 처리
	 * LockOn 전환은 DefaultAimMode가 None일 때만 허용 (코드 하드코딩).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon",
		meta = (Categories = "LNP.AimMode"))
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	float FireCooldown = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile")
	ELNPProjectileType ProjectileType = ELNPProjectileType::Linear;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "1"))
	float ProjectileSpeed = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "0.1"))
	float ProjectileHitRadius = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "0"))
	float ProjectileDamage = 10.f;

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
};
