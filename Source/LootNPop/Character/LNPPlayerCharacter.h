// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/LNPCharacterBase.h"
#include "LNPPlayerCharacter.generated.h"

class UGameplayCameraComponent;
class ULNPItemDefinitionBase;
class ULNPInventoryItemInstance;
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

	/** 아이템 인스턴스를 캐릭터 전방에 LootDice로 드랍한다 (파티원 양도용). UI 진입점 — ItemId로 대상 지정. */
	void DropItem(const FGuid& ItemId);

	/** 가방 무기 인스턴스를 장착한다 (UI 진입점). 클라 예측 + 서버 권위 GAS 부여. */
	void EquipWeaponInstance(ULNPInventoryItemInstance* Instance);

protected:
	virtual bool TryActivateAttack_Impl() override;
	virtual void CancelCurrentAttackAbility() override;

	/** 서버 전용: LootSpeed Attribute 값을 플레이어 Mass 엔티티의 FLNPPlayerLootingFragment에 반영한다.
	 *  버프 GE가 Attribute를 변조하면 델리게이트(PossessedBy에서 바인딩)를 통해 자동 호출된다. */
	void PushLootSpeedToEntity(float NewLootSpeed);

	UFUNCTION(Server, Reliable)
	void Server_EquipWeapon(ULNPWeaponData* WeaponData);

	/** 원격 클라이언트의 가방 무기 장착을 서버에 전달한다 (ItemId로 인스턴스 조회). */
	UFUNCTION(Server, Reliable)
	void Server_EquipWeaponInstance(FGuid ItemId);

	/** 원격 클라이언트의 아이템 드랍을 서버에 전달한다 — 검증(미장착·보유)은 서버가 수행한다. */
	UFUNCTION(Server, Reliable)
	void Server_DropItem(FGuid ItemId);

	/** 서버 전용: 인스턴스 조회·미장착 검증 → 인벤토리 제거 → LootDice 스폰 (제거 성공 전 스폰 금지). */
	void DropItemOnServer(const FGuid& ItemId);

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
