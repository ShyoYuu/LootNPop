// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Character/LNPCharacterBase.h"
#include "Enemy/LNPEnemyMassTypes.h"
#include "GameplayAbilitySpec.h"
#include "AbilitySystemComponent.h"
#include "LNPEnemyCharacter.generated.h"

class ULNPEnemyConfig;
class UAbilitySystemComponent;
class ULNPBaseAttributeSet;
class UWidgetComponent;
class ULNPHpBarWidget;
struct FMassEntityHandle;

/**
 * 셸 역할을 하는 범용 Enemy 캐릭터.
 * 비주얼과 Ability는 ULNPEnemyConfig로 초기화된다.
 */
UCLASS()
class LOOTNPOP_API ALNPEnemyCharacter : public ALNPCharacterBase
{
	GENERATED_BODY()

public:
	ALNPEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual bool TryActivateAttack() override;

	/** 제공된 config 에셋으로 캐릭터를 초기화한다 */
	void InitializeFromConfig(ULNPEnemyConfig* InConfig);

	/** Mass -> Actor 동기화: Mass에서 Actor가 스폰/활성화될 때 호출 */
	void SyncFromEntity(FMassEntityHandle InEntityHandle, float InHealth, ELNPTargetingState InTargetingState, FVector InVelocity);

	/** Actor -> Mass 동기화: Actor가 Mass로 비활성화/Destroy되기 전 호출 */
	void SyncToEntity(float& OutHealth, FVector& OutVelocity) const;

	/** 물리 Ragdoll을 활성화하고 이동을 비활성화한다. 여러 번 호출해도 안전. */
	void TriggerRagdoll();

protected:
	virtual void BeginPlay() override;
	virtual const ULNPWeaponData* GetActiveWeaponDef() const override;

	/** 이 Enemy을 초기화하는 데 사용된 config 에셋 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Enemy")
	TObjectPtr<ULNPEnemyConfig> EnemyConfig;

	/** WeaponData에서 부여된 무기 공격 Ability Handle */
	FGameplayAbilitySpecHandle WeaponAbilityHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|GAS")
	TObjectPtr<UAbilitySystemComponent> ASC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|GAS")
	TObjectPtr<ULNPBaseAttributeSet> AttributeSet;

	/** World Space HpBar Widget Component. HP > 0 이고 HP < MaxHP 일 때만 표시. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|UI")
	TObjectPtr<UWidgetComponent> HpBarComponent;

	/** HpBar에 사용할 Widget 클래스. Blueprint CDO에서 설정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LNP|UI")
	TSubclassOf<ULNPHpBarWidget> HpBarWidgetClass;

private:
	void OnHpAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshHpBar(float Current, float Max);
};
