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

	UCapsuleComponent*             GetCapsule()               const { return CapsuleComponent; }
	USkeletalMeshComponent*        GetWeaponMeshComponent()    const { return WeaponMeshComponent; }

	void SetAIMoveInput(FVector InMoveInput);
	void SetAIOrientationIntent(FVector InOrientationIntent);

	virtual bool TryActivateAttack();

	/** 이 캐릭터에 현재 장착/설정된 무기 데이터를 반환한다. */
	virtual const ULNPWeaponData* GetActiveWeaponDef() const { return ActiveWeaponData; }

	/**
	 * 무기를 장착한다. nullptr을 전달하면 맨손 상태로 전환.
	 * - ASC에 무기·조준모드 태그 부여
	 * - VisualMesh에 서브 AnimBP 레이어 연결
	 * - bFaceMoveDirection 자동 설정
	 */
	void EquipWeapon(ULNPWeaponData* WeaponData);

	/** 테스트용: SlotIndex로 TestWeaponList에서 무기 장착. 범위 초과 시 맨손. */
	void EquipTestWeapon(int32 SlotIndex);

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
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

	/** 현재 장착 중인 무기 데이터. nullptr = 맨손. */
	UPROPERTY(VisibleInstanceOnly, Category = "LNP|Weapon")
	TObjectPtr<ULNPWeaponData> ActiveWeaponData;

	/** 무기 스켈레탈 메시를 표시하는 컴포넌트. EquipWeapon()이 메시와 소켓을 동적으로 교체. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMeshComponent;

	/** 테스트용 무기 목록. BP에서 슬롯 순서대로 지정. (0=LongSword, 1=Pistol, 2=Rifle) */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Weapon|Test")
	TArray<TObjectPtr<ULNPWeaponData>> TestWeaponList;

private:
	FGameplayTag CurrentWeaponTag;
	FGameplayTag CurrentAimModeTag;
};
