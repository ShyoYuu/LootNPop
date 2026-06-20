// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemInterface.h"
#include "MassAgentComponent.h"
#include "GameplayTagContainer.h"
#include "LNPCharacterBase.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UAbilitySystemComponent;
class UMassAgentComponent;
class UAnimMontage;
class UGameplayAbility;
class UChooserTable;
class ULNPMontageChooserContext;

class ULNPCharacterMoverComponent;
class ULNPInputHandlerComponent;
class ULNPPawnGravityComponent;
class ALNPLootPod;
class ULNPWeaponData;
class UAbilitySystemComponent;


/**
 * ALNPCharacterBase
 */
UCLASS(Abstract)
class LOOTNPOP_API ALNPCharacterBase : public APawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ALNPCharacterBase(const FObjectInitializer& ObjectInitializer);

	// --- Component 접근자 ---
	UFUNCTION(BlueprintPure, Category = "LNP|Mover")
	ULNPCharacterMoverComponent* GetMoverComponent() const { return MoverComponent; }

	UFUNCTION(BlueprintPure, Category = "LNP|Movement")
	bool GetFaceMoveDirection() const;

	// IAbilitySystemInterface 구현
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UCapsuleComponent*      GetCapsule()      const { return CapsuleComponent; }
	USkeletalMeshComponent* GetWeaponMesh()   const { return WeaponMesh;       }
	UAnimInstance*          GetAnimInstance() const { return AnimSourceMesh ? AnimSourceMesh->GetAnimInstance() : nullptr; }

	void SetAIMoveInput(FVector InMoveInput);
	void SetAIOrientationIntent(FVector InOrientationIntent);

	/**
	 * 공격 입력 처리 공용 진입점. TAG_Block_AttackInput 체크 후 TryActivateAttack_Impl을 호출한다.
	 * true  = 공격이 발동됨
	 * false = 발동 실패 (블락 구간, GAS 쿨다운 등) — 호출자가 재시도 버퍼를 세울 수 있다.
	 *         블락 구간 + ComboWindow 활성 시에는 false를 반환하면서 콤보 버퍼도 동시에 설정한다.
	 */
	bool TryActivateAttack();

	/** 현재 콤보 인덱스 (0 = 첫 번째 공격 Attack_1). */
	int32 GetCurrentComboIndex() const { return CurrentComboIndex; }

	/** 콤보 창에서 입력 수신 시 ANS가 호출한다. */
	void SetComboInputBuffered(bool bBuffered) { bComboInputBuffered = bBuffered; }

	/** 콤보 버퍼 소비. 버퍼가 있었으면 true 반환 후 초기화. */
	bool ConsumeComboInput();

	/** 콤보 인덱스를 MaxComboCount에 맞게 1 증가(순환). */
	void IncrementComboIndex();

	/** 콤보 상태를 초기값으로 리셋. */
	void ResetCombo();

	/** 피격 방향에 맞는 HitReact 몽타주 섹션을 재생한다. */
	void PlayHitReact(FVector HitFromWorldDir);

	/**
	 * MontageChooser를 평가해 조건에 맞는 몽타주를 반환한다.
	 * @param WeaponType     장착 무기 태그 (LNP.Weapon.*)
	 * @param SituationType  상황 태그    (LNP.Montage.Situation.*)
	 * @param Value          세부 값 태그 (LNP.Montage.Value.*, 생략 가능)
	 */
	UFUNCTION(BlueprintCallable, Category = "LNP|Animation")
	UAnimMontage* EvaluateMontage(FGameplayTag WeaponType, FGameplayTag SituationType, FGameplayTag Value) const;
	UAnimMontage* EvaluateMontage(FGameplayTag SituationType, FGameplayTag Value = FGameplayTag()) const;

	/** Duration 동안 CustomTimeDilation을 TimeDilation으로 낮춰 HitStop 효과를 준다. */
	void ApplyHitStop(float Duration, float TimeDilation = 0.1f);

	/** HitFromDirection 방향으로 Strength 크기의 넉백 임펄스를 가한다. */
	void ApplyKnockback(const FVector HitFromDirection, const float Strength);

	/** 이 캐릭터에 현재 장착/설정된 무기 데이터를 반환한다. */
	virtual const ULNPWeaponData* GetActiveWeaponDef() const { return nullptr; }

	/**
	 * 무기를 장착한다. nullptr을 전달하면 맨손 상태로 전환.
	 * - ASC에 무기·조준모드 태그 부여
	 * - VisualMesh에 서브 AnimBP 레이어 연결
	 * - bFaceMoveDirection 자동 설정
	 */
	virtual void EquipWeapon(ULNPWeaponData* WeaponData);

	/** 테스트용: SlotIndex로 TestWeaponList에서 무기 장착. 범위 초과 시 맨손. */
	void EquipTestWeapon(int32 SlotIndex);

protected:
	/** 실제 공격 발동 로직. 서브클래스에서 오버라이드. */
	virtual bool TryActivateAttack_Impl();

	/** 현재 실행 중인 기본 공격 어빌리티를 취소한다. 서브클래스에서 오버라이드. */
	virtual void CancelCurrentAttackAbility() {}
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> AnimSourceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMassAgentComponent> MassAgentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPCharacterMoverComponent> MoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPInputHandlerComponent> InputHandlerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Gravity", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPPawnGravityComponent> GravityComponent;

	/** 맨손 상태에서 사용할 서브 AnimBP 클래스. BP 서브클래스에서 지정. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Animation")
	TSubclassOf<UAnimInstance> UnarmedAnimLayerClass;

	/**
	 * 몽타주 선택 테이블. EvaluateMontage()가 평가하는 단일 Chooser Table.
	 * 열 구성: WeaponType(Has Tag) / SituationType(Has Tag) / Value(Has Tag) → UAnimMontage*
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Animation")
	TObjectPtr<UChooserTable> MontageChooser;

	/** 무기 스켈레탈 메시를 표시하는 컴포넌트. EquipWeapon()이 메시와 소켓을 동적으로 교체. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** 테스트용 무기 목록. BP에서 슬롯 순서대로 지정. (0=Pistol, 1=Rifle, 2=LongSword) */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Weapon|Test")
	TArray<TObjectPtr<ULNPWeaponData>> TestWeaponList;

	/** 항상 보유할 기본 어빌리티 목록. BP 서브클래스에서 지정. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

private:
	void InitAbilitySystem();

	FGameplayTag CurrentWeaponTag;
	FGameplayTag CurrentAimModeTag;

	int32 CurrentComboIndex   = 0;
	bool  bComboInputBuffered = false;

	// EvaluateMontage() 호출마다 재할당을 피하기 위해 캐싱
	UPROPERTY(Transient)
	TObjectPtr<ULNPMontageChooserContext> MontageCtx;
};
