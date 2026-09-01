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

	/**
	 * 사망 연출 진입점. **서버 전용** — 전 클라이언트에 랙돌을 방송한다. 여러 번 호출해도 안전.
	 * 사망 판정 자체는 서버 전용 Mass 프로세서(ULNPHealthProcessor)가 내리므로,
	 * 방송하지 않으면 클라이언트는 적이 그냥 사라지는 것만 본다.
	 */
	void TriggerRagdoll();

	/** 락온 표식 위젯의 표시 상태를 설정한다. LNPLockOnComponent가 호출. */
	void SetLockOnMarkerVisible(bool bVisible);

	/**
	 * 조준 목표 지점을 갱신한다. **서버 전용** — StateTree의 AttackTask가 발사 직전에 호출한다.
	 *
	 * 저장하는 값은 목표 지점이 아니라 **캐릭터 로컬 좌표계 기준 Pitch 각도 하나**다.
	 * 구면 중력 위에서 월드 Z 기준 Pitch는 의미가 없고, 로컬 각도로 두면 게스트가
	 * 자기 화면의 액터 회전으로 복원해도 같은 자세가 나온다
	 * (같은 규약: TechDesign_CombatAnimation.md §4.1의 Aim Offset 계산).
	 * Yaw는 이미 몸통 조향(MoveTarget)이 타겟을 향하고 있으므로 담지 않는다.
	 */
	void SetAimTargetLocation(const FVector& InWorldTarget);

	/** 조준을 수평으로 되돌린다. **서버 전용** — 공격 상태를 벗어날 때 호출한다. */
	void ClearAimTarget();

	/**
	 * 조준 방향. 적은 컨트롤러가 없어 APawn 기본 구현이 액터 회전(= 수평)만 돌려주므로,
	 * 복제된 로컬 Pitch를 얹어 실제 조준선을 만든다.
	 * 소비처는 둘이다 — `ULNPAnimInstance`의 Aim Offset(모든 머신)과
	 * `ULNPAbility_RangedAttack::GetFireDirections`의 발사 방향(서버).
	 */
	virtual FRotator GetBaseAimRotation() const override;

protected:
	/**
	 * 각 머신 로컬 랙돌 연출. 물리 상태는 복제하지 않으므로 위치가 머신마다 갈라지지만,
	 * 시체는 게임플레이 판정이 없는 코스메틱이라 허용한다.
	 * Reliable — 사망은 1회성 상태 전이라 유실되면 그 화면에서만 적이 계속 서 있게 된다.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_TriggerRagdoll(FVector PopVelocity);

	/** 사망 시 표면 Up 방향으로 부여할 Pop 속도 (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Death")
	float RagdollPopSpeed = 2000.f;;

	/**
	 * 조준 Pitch 추종 속도(FInterpTo). 목표를 잡는 순간 상체가 튀지 않도록 한 박자 늦춘다.
	 * 가용 각도(`AimPitchMin/MaxDeg`)와 달리 순전히 코스메틱이라 Actor 쪽에 남긴다 —
	 * 가용 각도는 인지 판정(Mass)도 읽어야 해서 `FLNPEnemyMovementConfig`에 있다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Combat", meta = (ClampMin = "0.1"))
	float AimPitchInterpSpeed = 8.f;

	/**
	 * 현재 조준 Pitch(도, 로컬 좌표계). 서버가 보간해 만들고 그 결과를 복제한다.
	 *
	 * 복제하는 이유: 타게팅은 서버 전용 Mass 로직이라 게스트는 이 적이 무엇을 겨누는지 알 방법이 없다.
	 * 각도 하나만 보내면 되고, 로컬 좌표계 값이라 게스트의 액터 회전이 미세하게 달라도 자세는 일치한다.
	 */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	float AimPitchDeg = 0.f;

	/** 보간 목표. 서버에서만 의미가 있어 복제하지 않는다. */
	float TargetAimPitchDeg = 0.f;

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
