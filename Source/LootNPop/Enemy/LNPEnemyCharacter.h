// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
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
class UUserWidget;

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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Config 기반 1회 초기화 (Ability, 무기). 내부적으로 중복 호출을 무시한다. 서버 전용 — 클라이언트는 EnemyConfig 복제로 받는다. */
	void InitializeOnce(ULNPEnemyConfig* InConfig);

	/** 매 High LOD 활성화마다 호출: AnimSourceMesh 숨김, HP/속도 동기화 */
	void SyncFromEntity(float InHealth, ELNPTargetingState InTargetingState, FVector InVelocity);

	/** Actor -> Mass 동기화: Actor가 Mass로 비활성화/Destroy되기 전 호출 */
	void SyncToEntity(float& OutHealth, FVector& OutVelocity) const;

	/** 물리 Ragdoll을 활성화하고 이동을 비활성화한다. 여러 번 호출해도 안전. */
	void TriggerRagdoll();

	/** 락온 표식 위젯의 표시 상태를 설정한다. LNPLockOnComponent가 호출. */
	void SetLockOnMarkerVisible(bool bVisible);

protected:
	virtual void Tick(float DeltaTime) override;
	virtual bool TryActivateAttack_Impl() override;
	virtual void CancelCurrentAttackAbility() override;
	virtual void BeginPlay() override;
	virtual const ULNPWeaponData* GetActiveWeaponDef() const override;
	virtual ULNPWeaponData* ResolveWeaponDefForVisuals() const override;

	/**
	 * 이 Enemy를 초기화하는 데 사용된 config 에셋 — 적 무기 상태의 단일 원본.
	 *
	 * 복제되는 이유: Enemy Actor 스폰·InitializeOnce는 서버 전용이고(ULNPEnemyActorInitializerProcessor가
	 * 클라 월드에서 early-return) 클라이언트의 Actor는 일반 Actor Relevancy로 온다.
	 * 이게 없으면 클라이언트에서 무기 메시·애님 레이어가 붙지 않고 GetActiveWeaponDef()도 null이 된다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_EnemyConfig, VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Enemy")
	TObjectPtr<ULNPEnemyConfig> EnemyConfig;

	/** 비주얼만 갱신한다. GAS 부여·무기 스텟 적용은 서버 InitializeOnce의 책임이다. */
	UFUNCTION()
	void OnRep_EnemyConfig();

	/** WeaponData에서 부여된 무기 공격 Ability Handle */
	FGameplayAbilitySpecHandle WeaponAbilityHandle;

	/**
	 * WeaponData의 StatModifiers로 적용한 GE 핸들.
	 * 적은 EquipmentComponent를 거치지 않으므로 여기서 직접 적용·해제한다.
	 * LOD 전환마다 재초기화되므로 해제하지 않으면 무기 스텟이 누적된다.
	 */
	TArray<FActiveGameplayEffectHandle> WeaponStatEffects;

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

	/** 락온 표식 Widget Component. 캐릭터 머리 위에 표시. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|UI")
	TObjectPtr<UWidgetComponent> LockOnMarkerComponent;

	/** 락온 표식에 사용할 Widget 클래스. Blueprint CDO에서 설정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LNP|UI")
	TSubclassOf<UUserWidget> LockOnMarkerWidgetClass;

private:
	bool bInitializedOnce = false;

	void OnHpAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshHpBar(float Current, float Max);
};
