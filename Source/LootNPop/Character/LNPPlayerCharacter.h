// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/LNPCharacterBase.h"
#include "LNPPlayerCharacter.generated.h"

class UGameplayCameraComponent;
class ULNPInteractionComponent;
class ULNPLockOnComponent;
class ULNPControlRotationComponent;

/**
 * Player가 조종하는 캐릭터 클래스.
 */
UCLASS()
class LOOTNPOP_API ALNPPlayerCharacter : public ALNPCharacterBase
{
	GENERATED_BODY()

public:
	ALNPPlayerCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual void EquipWeapon(ULNPWeaponData* WeaponData) override;
	virtual const ULNPWeaponData* GetActiveWeaponDef() const override;

	/**
	 * 시선 회전(에임) 반환. 로컬 제어 폰은 기존처럼 Control Rotation을 쓰고,
	 * 서버의 원격 폰과 시뮬레이티드 프록시는 Mover InputCmd에 실려 온 소유 클라이언트의
	 * ControlRotation을 사용한다 — 원거리 발사 피치와 Aim Offset(상체 자세) 동기화의 단일 데이터 소스.
	 * (서버: Network Prediction 입력 복제 / 프록시: bSyncInputsForSimProxy로 SyncState에 동봉)
	 */
	virtual FRotator GetBaseAimRotation() const override;

	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	TArray<AActor*> GetInteractionCandidates() const;

	UFUNCTION(BlueprintPure, Category = "LNP|Interaction")
	AActor* GetInteractionCandidate() const;

protected:
	virtual bool TryActivateAttack_Impl() override;
	virtual void CancelCurrentAttackAbility() override;

	UFUNCTION(Server, Reliable)
	void Server_EquipWeapon(ULNPWeaponData* WeaponData);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGameplayCameraComponent> GameplayCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPLockOnComponent> LockOnComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULNPControlRotationComponent> ControlRotationComponent;
};
