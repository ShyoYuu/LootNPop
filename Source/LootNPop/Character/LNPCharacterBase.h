// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemInterface.h"
#include "MassAgentComponent.h"
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

	void SetAIMoveInput(FVector InMoveInput);
	void SetAIOrientationIntent(FVector InOrientationIntent);

	virtual bool TryActivateAttack();

	/** 이 캐릭터에 현재 장착/설정된 무기 데이터를 반환한다. */
	virtual const ULNPWeaponData* GetActiveWeaponDef() const { return nullptr; }

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
};
